/* orm-worker.c
 *
 * Copyright 2025 Zach Podbielniak
 *
 * This file is part of orm-glib.
 *
 * orm-glib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * orm-glib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with orm-glib.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "orm-worker.h"

/*
 * OrmWorker - the one thread a connection's work runs on.
 *
 * A queue of jobs and a thread that pops them.  Everything interesting
 * about it is what it does not do: there is no pool, no work stealing
 * and no parallelism, because the whole point is that two operations on
 * one connection can never overlap.  Ordering falls out of that for
 * free -- jobs complete in the order they were pushed, which is what a
 * caller issuing three statements in a row expects even when it does not
 * wait for each one.
 */

struct _OrmWorker
{
    GThread     *thread;
    GAsyncQueue *queue;
};

typedef struct
{
    OrmWorkerJobFunc  run;
    GTask            *task;
    gpointer          data;
    GDestroyNotify    data_free;
} OrmWorkerJob;

/*
 * The rendezvous behind orm_worker_invoke_sync().  Lives on the calling
 * thread's stack: that thread is blocked on @cond for as long as the
 * worker can see this, so there is nothing to own or free.
 */
typedef struct
{
    OrmWorkerSyncFunc  func;
    gpointer           data;
    GMutex             mutex;
    GCond              cond;
    gboolean           done;
} OrmWorkerRendezvous;

static void
orm_worker_job_free (OrmWorkerJob *job)
{
    if (job->data_free != NULL && job->data != NULL)
        job->data_free (job->data);

    g_clear_object (&job->task);
    g_free (job);
}

/*
 * Runs a synchronous job and wakes the thread waiting on it.
 *
 * Nothing may touch @data after the signal: the waiting thread returns
 * from orm_worker_invoke_sync() and unwinds the stack the rendezvous
 * lives on.
 */
static void
orm_worker_run_rendezvous (gpointer  data,
                           GTask    *task)
{
    OrmWorkerRendezvous *rendezvous = (OrmWorkerRendezvous *) data;

    rendezvous->func (rendezvous->data);

    g_mutex_lock (&rendezvous->mutex);
    rendezvous->done = TRUE;
    g_cond_signal (&rendezvous->cond);
    g_mutex_unlock (&rendezvous->mutex);
}

/*
 * Pops and runs jobs until the shutdown sentinel arrives.
 *
 * The sentinel is a job with no body rather than a %NULL pointer,
 * because GAsyncQueue refuses to carry %NULL.  Queueing it rather than
 * setting a flag is what makes shutdown drain: every job pushed before
 * it still runs.
 */
static gpointer
orm_worker_thread_func (gpointer data)
{
    OrmWorker *self = (OrmWorker *) data;

    for (;;)
    {
        OrmWorkerJob *job = (OrmWorkerJob *) g_async_queue_pop (self->queue);

        if (job->run == NULL)
        {
            g_free (job);
            break;
        }

        job->run (job->data, job->task);
        orm_worker_job_free (job);
    }

    return NULL;
}

/*
 * Internal: starts a worker thread.
 *
 * Returns: (transfer full): A new #OrmWorker
 */
OrmWorker *
orm_worker_new (const gchar *name)
{
    OrmWorker *self;

    self = g_new0 (OrmWorker, 1);
    self->queue = g_async_queue_new ();
    self->thread = g_thread_new (name, orm_worker_thread_func, self);

    return self;
}

/*
 * Internal: drains the queue, stops the thread and frees the worker.
 *
 * Must not be called from the worker thread itself, which would be
 * joining itself.
 */
void
orm_worker_shutdown (OrmWorker *self)
{
    OrmWorkerJob *sentinel;

    g_return_if_fail (self != NULL);
    g_return_if_fail (!orm_worker_is_current (self));

    sentinel = g_new0 (OrmWorkerJob, 1);
    g_async_queue_push (self->queue, sentinel);

    g_thread_join (self->thread);

    g_async_queue_unref (self->queue);
    g_free (self);
}

/*
 * Internal: whether the calling thread is this worker's thread.
 *
 * This is what keeps a job that reaches back into the synchronous API
 * from deadlocking on itself.
 *
 * Returns: %TRUE when called from the worker thread
 */
gboolean
orm_worker_is_current (OrmWorker *self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    return self->thread == g_thread_self ();
}

/*
 * Internal: queues a job.
 *
 * @task is carried along and released with the job; it is the worker's
 * business only in that it must outlive @run.
 */
void
orm_worker_push (OrmWorker        *self,
                 OrmWorkerJobFunc  run,
                 GTask            *task,
                 gpointer          data,
                 GDestroyNotify    data_free)
{
    OrmWorkerJob *job;

    g_return_if_fail (self != NULL);
    g_return_if_fail (run != NULL);

    job = g_new0 (OrmWorkerJob, 1);
    job->run = run;
    job->task = task != NULL ? g_object_ref (task) : NULL;
    job->data = data;
    job->data_free = data_free;

    g_async_queue_push (self->queue, job);
}

/*
 * Internal: runs @func on the worker thread and blocks until it returns.
 *
 * This is how the synchronous API stays correct once a connection has a
 * worker: the statement still runs on the connection's own thread, so a
 * synchronous call cannot overlap a queued asynchronous one.  It waits
 * its turn instead.
 */
void
orm_worker_invoke_sync (OrmWorker         *self,
                        OrmWorkerSyncFunc  func,
                        gpointer           data)
{
    OrmWorkerRendezvous rendezvous;

    g_return_if_fail (self != NULL);
    g_return_if_fail (func != NULL);
    g_return_if_fail (!orm_worker_is_current (self));

    rendezvous.func = func;
    rendezvous.data = data;
    rendezvous.done = FALSE;
    g_mutex_init (&rendezvous.mutex);
    g_cond_init (&rendezvous.cond);

    orm_worker_push (self, orm_worker_run_rendezvous, NULL, &rendezvous, NULL);

    g_mutex_lock (&rendezvous.mutex);
    while (!rendezvous.done)
        g_cond_wait (&rendezvous.cond, &rendezvous.mutex);
    g_mutex_unlock (&rendezvous.mutex);

    g_cond_clear (&rendezvous.cond);
    g_mutex_clear (&rendezvous.mutex);
}
