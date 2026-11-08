/* orm-connection-pool.c
 *
 * Copyright 2026 Zach Podbielniak
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

#include "orm-connection-pool.h"
#include "orm-engine-private.h"
#include "../core/orm-error.h"
#include "../core/orm-value.h"

#include <string.h>

/*
 * OrmConnectionPool - several connections, lent one borrower at a time.
 *
 * The concurrency model of the rest of the engine layer is unchanged: a
 * connection runs one statement at a time on its own worker thread.  The
 * pool adds the missing piece above that -- a bounded set of such
 * connections and a fair way to share them -- so an application gets N
 * threads of database work by asking for N leases.
 *
 * Bookkeeping, all under @lock:
 *
 *   n_open   every connection that exists or is being opened, so the
 *            max-size bound covers connections still in their handshake
 *   idle     open and unlent, most recently returned at the head
 *   lent     the set of connections out on loan, so a release can be
 *            checked against what was actually lent
 *   waiters  acquirers queued while the pool is exhausted, oldest first
 *
 * Nothing slow happens under the lock.  Opening, validating, resetting
 * and closing connections all run with it dropped, and so does every
 * GTask return, because a GTask may run its callback synchronously and
 * that callback may well call back into the pool.
 *
 * Acquiring follows one rule: take an idle connection if there is one;
 * otherwise reserve a slot (n_open++) and open one outside the lock;
 * otherwise queue.  A released connection goes straight to the oldest
 * waiter, and a discarded one hands its slot to that waiter instead, so a
 * queued acquirer never waits behind somebody who arrived later.
 */

/* The idle reaper's tick is half the timeout, kept within these bounds */
#define ORM_CONNECTION_POOL_REAPER_MIN_INTERVAL (1)
#define ORM_CONNECTION_POOL_REAPER_MAX_INTERVAL (60)

/* The statement an idle connection must survive before it is lent */
#define ORM_CONNECTION_POOL_VALIDATION_SQL "SELECT 1"

struct _OrmConnectionPool
{
    GObject parent_instance;

    OrmEngine    *engine;           /* Owned */
    GMainContext *main_context;     /* Owned */

    GMutex        lock;

    /* Settings */
    guint         max_size;
    guint         min_size;
    guint         idle_timeout;         /* seconds, 0 = never reap */
    guint         acquire_timeout;      /* milliseconds, 0 = wait forever */
    guint         validation_interval;  /* seconds, G_MAXUINT = never */
    gchar        *init_sql;

    /* State */
    guint         n_open;
    GQueue        idle;             /* OrmPoolEntry * */
    GHashTable   *lent;             /* OrmConnection * -> OrmPoolLoan *, refs not held */
    GQueue        waiters;          /* OrmPoolWaiter * */
    gboolean      closed;

    GSource      *reaper;           /* Owned, attached to @main_context */

    /*
     * Construct-time property sets arrive before @main_context is known,
     * so until constructed() they only store values; starting the reaper
     * then would attach it to the global default context.
     */
    gboolean      is_constructed;
};

static void orm_connection_pool_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (OrmConnectionPool, orm_connection_pool, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                orm_connection_pool_initable_iface_init))

enum {
    PROP_0,
    PROP_ENGINE,
    PROP_MAIN_CONTEXT,
    PROP_MAX_SIZE,
    PROP_MIN_SIZE,
    PROP_IDLE_TIMEOUT,
    PROP_ACQUIRE_TIMEOUT,
    PROP_VALIDATION_INTERVAL,
    PROP_INIT_SQL,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

/* ------------------------------------------------------------------ */
/* Idle entries and leases                                            */
/* ------------------------------------------------------------------ */

/*
 * An idle connection and when it went idle, which drives both reaping
 * and the validate-before-lending decision.
 */
typedef struct
{
    OrmConnection *connection;      /* Owned */
    gint64         idle_since;      /* g_get_monotonic_time() */
} OrmPoolEntry;

/*
 * A connection on its way to an asynchronous acquirer, handed back when
 * this is freed.  It rides in the GTask's result so that a caller who
 * never calls _finish() -- a NULL callback, a task dropped on the floor --
 * still returns the connection to the pool when GTask frees the unclaimed
 * result.
 */
typedef struct
{
    OrmConnectionPool *pool;        /* Owned */
    OrmConnection     *connection;  /* Owned; NULL once collected */
} OrmPoolLease;

static OrmPoolLease *
orm_pool_lease_new (OrmConnectionPool *pool,
                    OrmConnection     *connection)
{
    OrmPoolLease *lease;

    lease = g_new0 (OrmPoolLease, 1);
    lease->pool = g_object_ref (pool);
    lease->connection = connection;

    return lease;
}

static void
orm_pool_lease_free (gpointer data)
{
    OrmPoolLease *lease = (OrmPoolLease *) data;

    if (lease->connection != NULL)
        orm_connection_pool_release (lease->pool,
                                     g_steal_pointer (&lease->connection));

    g_object_unref (lease->pool);
    g_free (lease);
}

/* ------------------------------------------------------------------ */
/* Waiters                                                            */
/* ------------------------------------------------------------------ */

/*
 * An acquirer queued while the pool is exhausted.
 *
 * Exactly one party resolves a waiter, under the pool lock: whoever
 * takes it off the queue -- a release handing it a connection or a slot,
 * a timeout, a cancellation, or close().  The outcome fields are written
 * then and read by the waiter's own side after.
 *
 * A synchronous waiter lives on the acquiring thread's stack and is woken
 * through @cond.  An asynchronous one is reference counted: the queue,
 * the cancel handler and the timeout source each hold a reference, and
 * the resolver passes the queue's on to orm_pool_waiter_finish().
 */
typedef struct
{
    gint               ref_count;
    gboolean           is_async;
    OrmConnectionPool *pool;            /* Not owned: the task holds it */

    /* Outcome, written once under the pool lock */
    gboolean           done;
    OrmConnection     *connection;      /* Handed over, owned */
    gboolean           slot;            /* The right to open one instead */
    GError            *error;

    /* Synchronous */
    GCond              cond;

    /* Asynchronous */
    GTask             *task;            /* Owned */
    gulong             cancel_id;
    gboolean           finished;
    GSource           *timeout_source;  /* Owned */
} OrmPoolWaiter;

static OrmPoolWaiter *
orm_pool_waiter_ref (OrmPoolWaiter *waiter)
{
    g_atomic_int_inc (&waiter->ref_count);
    return waiter;
}

static void
orm_pool_waiter_unref (gpointer data)
{
    OrmPoolWaiter *waiter = (OrmPoolWaiter *) data;

    if (!g_atomic_int_dec_and_test (&waiter->ref_count))
        return;

    /*
     * Only reachable with an outcome nobody collected, e.g. on error
     * paths.  A connection handed over is lent, so it goes back properly.
     */
    if (waiter->connection != NULL)
        orm_connection_pool_release (waiter->pool,
                                     g_steal_pointer (&waiter->connection));
    g_clear_error (&waiter->error);
    g_clear_object (&waiter->task);
    g_free (waiter);
}

/*
 * Takes @waiter off the queue and records how it ends.  Called with the
 * lock held.  A synchronous waiter is woken here; an asynchronous one is
 * left for the caller to finish once the lock is dropped.
 *
 * @connection is (transfer full); @error is (transfer full).
 */
static void
orm_pool_waiter_resolve_locked (OrmConnectionPool *self,
                                OrmPoolWaiter     *waiter,
                                OrmConnection     *connection,
                                gboolean           slot,
                                GError            *error)
{
    g_queue_remove (&self->waiters, waiter);

    waiter->done = TRUE;
    waiter->connection = connection;
    waiter->slot = slot;
    waiter->error = error;

    if (!waiter->is_async)
        g_cond_signal (&waiter->cond);
}

/*
 * Lets the oldest waiter open a connection, if there is one waiting and
 * room under max-size.  Called with the lock held, after a connection
 * has stopped counting against n_open.
 *
 * Returns: (nullable): an asynchronous waiter the caller must finish
 *   after unlocking
 */
static OrmPoolWaiter *
orm_pool_grant_slot_locked (OrmConnectionPool *self)
{
    OrmPoolWaiter *head;

    head = (OrmPoolWaiter *) g_queue_peek_head (&self->waiters);
    if (head == NULL || self->n_open >= self->max_size)
        return NULL;

    self->n_open++;
    orm_pool_waiter_resolve_locked (self, head, NULL, TRUE, NULL);

    return head->is_async ? head : NULL;
}

/* ------------------------------------------------------------------ */
/* Loans                                                              */
/* ------------------------------------------------------------------ */

/*
 * The pool's record of one lent connection: the value in @lent, and the
 * data of a weak reference on the connection.
 *
 * The weak reference is what catches a borrower who drops the connection
 * with g_object_unref() instead of releasing it.  Without it the pool
 * would keep counting a connection that no longer exists, and that slot
 * would be gone for the life of the pool -- enough such mistakes and
 * every acquire times out.  When it fires, the slot is recovered and a
 * warning names the mistake.
 *
 * It holds the pool weakly too: the notification can arrive after the
 * pool is gone, and then there is nothing to recover.
 */
typedef struct
{
    GWeakRef pool;
} OrmPoolLoan;

static void orm_pool_waiter_finish (OrmPoolWaiter *waiter);

static void
orm_pool_loan_free (OrmPoolLoan *loan)
{
    g_weak_ref_clear (&loan->pool);
    g_free (loan);
}

/*
 * The weak notification: @where_the_object_was was finalized while still
 * lent.  Runs on whichever thread dropped the last reference.
 */
static void
orm_pool_loan_lost (gpointer  data,
                    GObject  *where_the_object_was)
{
    OrmPoolLoan       *loan = (OrmPoolLoan *) data;
    OrmConnectionPool *self;
    OrmPoolWaiter     *wake = NULL;
    gboolean           recovered = FALSE;

    self = (OrmConnectionPool *) g_weak_ref_get (&loan->pool);

    if (self != NULL)
    {
        g_mutex_lock (&self->lock);
        if (g_hash_table_steal (self->lent, where_the_object_was))
        {
            recovered = TRUE;
            self->n_open--;
            wake = orm_pool_grant_slot_locked (self);
        }
        g_mutex_unlock (&self->lock);

        if (recovered)
            g_warning ("OrmConnectionPool: a lent connection was finalized "
                       "without orm_connection_pool_release(); its slot has "
                       "been recovered, but release connections instead of "
                       "unreferencing them");

        if (wake != NULL)
            orm_pool_waiter_finish (wake);

        g_object_unref (self);
    }

    orm_pool_loan_free (loan);
}

/*
 * Records @connection as lent.  Called with the lock held; the caller
 * holds a reference, so the connection cannot be finalized meanwhile.
 */
static void
orm_pool_lend_locked (OrmConnectionPool *self,
                      OrmConnection     *connection)
{
    OrmPoolLoan *loan;

    loan = g_new0 (OrmPoolLoan, 1);
    g_weak_ref_init (&loan->pool, self);
    g_object_weak_ref (G_OBJECT (connection), orm_pool_loan_lost, loan);

    g_hash_table_insert (self->lent, connection, loan);
}

/*
 * Forgets that @connection is lent.  Called with the lock held, by a
 * caller holding a reference.
 *
 * Returns: %TRUE if it was lent
 */
static gboolean
orm_pool_unlend_locked (OrmConnectionPool *self,
                        OrmConnection     *connection)
{
    gpointer loan = NULL;

    if (!g_hash_table_steal_extended (self->lent, connection, NULL, &loan))
        return FALSE;

    g_object_weak_unref (G_OBJECT (connection), orm_pool_loan_lost, loan);
    orm_pool_loan_free ((OrmPoolLoan *) loan);

    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Opening, validating, closing                                       */
/* ------------------------------------------------------------------ */

/*
 * Closes and drops a connection the pool no longer counts.  Never called
 * with the lock held: closing drains the connection's worker.
 */
static void
orm_pool_dispose_connection (OrmConnection *connection)
{
    orm_connection_close (connection);
    g_object_unref (connection);
}

/*
 * Opens a connection and runs the pool's init SQL on it.
 *
 * The connection is re-pointed at the pool's main context: it may have
 * been built on a GIO worker thread, and its signals belong with the
 * pool's other housekeeping rather than wherever it happened to open.
 *
 * Returns: (transfer full) (nullable): the connection, or %NULL on error
 */
static OrmConnection *
orm_pool_open_connection (OrmConnectionPool  *self,
                          GError            **error)
{
    g_autoptr(OrmConnection) connection = NULL;
    g_autofree gchar        *init_sql = NULL;

    connection = orm_engine_connect (self->engine, error);
    if (connection == NULL)
        return NULL;

    orm_connection_set_owner_context (connection, self->main_context);

    init_sql = orm_connection_pool_dup_init_sql (self);
    if (init_sql != NULL &&
        !orm_connection_execute (connection, init_sql, error))
    {
        g_prefix_error (error, "Pool init SQL failed: ");
        orm_connection_close (connection);
        return NULL;
    }

    return (OrmConnection *) g_steal_pointer (&connection);
}

/*
 * Whether an idle connection is still worth lending.
 *
 * The drivers do not reliably notice a server that went away -- a dead
 * socket surfaces as a failed statement like any other -- so the only
 * honest check is to run something and see.
 */
static gboolean
orm_pool_validate (OrmConnection *connection)
{
    g_autoptr(OrmResult) result = NULL;
    g_autoptr(GError)    error = NULL;

    if (!orm_connection_is_open (connection) ||
        orm_connection_get_state (connection) == ORM_CONNECTION_CLOSED)
        return FALSE;

    result = orm_connection_query (connection, ORM_CONNECTION_POOL_VALIDATION_SQL,
                                   &error);
    if (result == NULL)
    {
        g_debug ("OrmConnectionPool: discarding a connection that failed "
                 "validation: %s", error->message);
        return FALSE;
    }

    return TRUE;
}

/*
 * A slot reserved for opening a connection was not used: give it back,
 * and let the next waiter try in its place so nobody queues behind an
 * attempt that already failed.
 */
static void orm_pool_waiter_finish (OrmPoolWaiter *waiter);

static void
orm_pool_slot_failed (OrmConnectionPool *self)
{
    OrmPoolWaiter *wake;

    g_mutex_lock (&self->lock);
    self->n_open--;
    wake = orm_pool_grant_slot_locked (self);
    g_mutex_unlock (&self->lock);

    if (wake != NULL)
        orm_pool_waiter_finish (wake);
}

/*
 * Turns a taken idle connection or a reserved slot into a connection
 * ready to lend.  Runs on whichever thread is acquiring: the caller's
 * for a synchronous acquire, a GIO worker for an asynchronous one.
 *
 * @candidate, when given, is an idle connection already counted as lent.
 * If it fails validation it is thrown away and its slot reused for a
 * fresh connection, so the caller still gets one.
 *
 * Returns: (transfer full) (nullable): a lent connection, or %NULL on error
 */
static OrmConnection *
orm_pool_prepare (OrmConnectionPool  *self,
                  OrmConnection      *candidate,
                  gboolean            validate,
                  GError            **error)
{
    OrmConnection *connection;
    gboolean       closed;

    if (candidate != NULL)
    {
        if (!validate || orm_pool_validate (candidate))
            return candidate;

        g_mutex_lock (&self->lock);
        orm_pool_unlend_locked (self, candidate);
        g_mutex_unlock (&self->lock);

        orm_pool_dispose_connection (candidate);
    }

    connection = orm_pool_open_connection (self, error);
    if (connection == NULL)
    {
        orm_pool_slot_failed (self);
        return NULL;
    }

    g_mutex_lock (&self->lock);
    closed = self->closed;
    if (closed)
        self->n_open--;
    else
        orm_pool_lend_locked (self, connection);
    g_mutex_unlock (&self->lock);

    if (closed)
    {
        orm_pool_dispose_connection (connection);
        g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION_CLOSED,
                     "Connection pool is closed");
        return NULL;
    }

    return connection;
}

/*
 * Puts a connection back: to the oldest waiter, to the idle list, or --
 * when @usable is %FALSE or the pool has closed -- out of the pool
 * altogether, with its slot passed on.
 *
 * @connection is (transfer full) and must already be out of @lent.
 */
static void
orm_pool_put_back (OrmConnectionPool *self,
                   OrmConnection     *connection,
                   gboolean           usable)
{
    OrmPoolWaiter *head;
    OrmPoolWaiter *wake = NULL;
    OrmPoolEntry  *entry;

    g_mutex_lock (&self->lock);

    if (self->closed)
        usable = FALSE;

    if (usable)
    {
        head = (OrmPoolWaiter *) g_queue_peek_head (&self->waiters);

        if (head != NULL)
        {
            /* Straight to whoever has waited longest; still lent */
            orm_pool_lend_locked (self, connection);
            orm_pool_waiter_resolve_locked (self, head, connection, FALSE, NULL);
            wake = head->is_async ? head : NULL;
        }
        else
        {
            entry = g_new0 (OrmPoolEntry, 1);
            entry->connection = connection;
            entry->idle_since = g_get_monotonic_time ();
            g_queue_push_head (&self->idle, entry);
        }

        connection = NULL;
    }
    else
    {
        self->n_open--;
        wake = orm_pool_grant_slot_locked (self);
    }

    g_mutex_unlock (&self->lock);

    if (connection != NULL)
        orm_pool_dispose_connection (connection);

    if (wake != NULL)
        orm_pool_waiter_finish (wake);
}

/* ------------------------------------------------------------------ */
/* Taking                                                             */
/* ------------------------------------------------------------------ */

typedef enum
{
    ORM_POOL_TAKE_CLOSED,
    ORM_POOL_TAKE_IDLE,
    ORM_POOL_TAKE_SLOT,
    ORM_POOL_TAKE_WAIT
} OrmPoolTake;

/*
 * The non-blocking half of acquiring.  Called with the lock held.
 *
 * On %ORM_POOL_TAKE_IDLE, @connection is set (transfer full) and already
 * counted as lent; @validate says whether it sat idle long enough to need
 * checking first.
 */
static OrmPoolTake
orm_pool_try_take_locked (OrmConnectionPool  *self,
                          OrmConnection     **connection,
                          gboolean           *validate)
{
    OrmPoolEntry *entry;
    gint64        idle_for;

    *connection = NULL;
    *validate = FALSE;

    if (self->closed)
        return ORM_POOL_TAKE_CLOSED;

    /* Most recently used first: warmest caches, and lets the tail age out */
    entry = (OrmPoolEntry *) g_queue_pop_head (&self->idle);
    if (entry != NULL)
    {
        idle_for = g_get_monotonic_time () - entry->idle_since;

        *connection = entry->connection;
        *validate = self->validation_interval != G_MAXUINT &&
                    idle_for >= (gint64) self->validation_interval * G_USEC_PER_SEC;

        orm_pool_lend_locked (self, entry->connection);
        g_free (entry);

        return ORM_POOL_TAKE_IDLE;
    }

    if (self->n_open < self->max_size)
    {
        self->n_open++;
        return ORM_POOL_TAKE_SLOT;
    }

    return ORM_POOL_TAKE_WAIT;
}

static void
orm_pool_set_closed_error (GError **error)
{
    g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION_CLOSED,
                 "Connection pool is closed");
}

/* ------------------------------------------------------------------ */
/* Synchronous acquire                                                */
/* ------------------------------------------------------------------ */

/*
 * A synchronous waiter's cancellation.  Runs on whichever thread
 * cancelled, possibly inside g_cancellable_connect() itself.
 */
static void
orm_pool_sync_waiter_cancelled (GCancellable *cancellable,
                                gpointer      user_data)
{
    OrmPoolWaiter *waiter = (OrmPoolWaiter *) user_data;
    OrmConnectionPool *self = waiter->pool;

    g_mutex_lock (&self->lock);
    if (!waiter->done)
        orm_pool_waiter_resolve_locked (self, waiter, NULL, FALSE,
                                        g_error_new_literal (G_IO_ERROR,
                                                             G_IO_ERROR_CANCELLED,
                                                             "Operation was cancelled"));
    g_mutex_unlock (&self->lock);
}

/**
 * orm_connection_pool_acquire:
 * @self: An #OrmConnectionPool
 * @cancellable: (nullable): A #GCancellable
 * @error: Return location for error
 *
 * Borrows a connection, blocking while the pool is exhausted.
 *
 * Takes an idle connection if there is one, opens a new one if the pool
 * is below #OrmConnectionPool:max-size, and otherwise waits in line for
 * up to #OrmConnectionPool:acquire-timeout milliseconds before failing
 * with %G_IO_ERROR_TIMED_OUT.
 *
 * Returns: (transfer full) (nullable): A connection to hand back with
 *   orm_connection_pool_release(), or %NULL on error
 */
OrmConnection *
orm_connection_pool_acquire (OrmConnectionPool  *self,
                             GCancellable       *cancellable,
                             GError            **error)
{
    OrmPoolWaiter  waiter;
    OrmConnection *connection = NULL;
    OrmPoolTake    take;
    gboolean       validate = FALSE;
    gulong         cancel_id = 0;
    gint64         deadline = 0;

    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), NULL);
    g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    if (g_cancellable_set_error_if_cancelled (cancellable, error))
        return NULL;

    g_mutex_lock (&self->lock);
    take = orm_pool_try_take_locked (self, &connection, &validate);

    if (take == ORM_POOL_TAKE_WAIT)
    {
        /* --- Queue, then sleep until resolved, timed out or cancelled --- */
        memset (&waiter, 0, sizeof (waiter));
        waiter.pool = self;
        g_cond_init (&waiter.cond);
        g_queue_push_tail (&self->waiters, &waiter);

        if (self->acquire_timeout > 0)
            deadline = g_get_monotonic_time () +
                       (gint64) self->acquire_timeout * G_TIME_SPAN_MILLISECOND;

        g_mutex_unlock (&self->lock);

        /*
         * Connected with the lock dropped: the handler takes it, and runs
         * immediately if @cancellable is already cancelled.
         */
        if (cancellable != NULL)
            cancel_id = g_cancellable_connect (cancellable,
                                               G_CALLBACK (orm_pool_sync_waiter_cancelled),
                                               &waiter, NULL);

        g_mutex_lock (&self->lock);
        while (!waiter.done)
        {
            if (deadline == 0)
            {
                g_cond_wait (&waiter.cond, &self->lock);
            }
            else if (!g_cond_wait_until (&waiter.cond, &self->lock, deadline) &&
                     !waiter.done)
            {
                orm_pool_waiter_resolve_locked (
                    self, &waiter, NULL, FALSE,
                    g_error_new (G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
                                 "Timed out after %u ms waiting for a pooled "
                                 "connection (max-size %u, all in use)",
                                 self->acquire_timeout, self->max_size));
            }
        }
        g_mutex_unlock (&self->lock);

        /* Waits out a handler still running on another thread */
        if (cancel_id != 0)
            g_cancellable_disconnect (cancellable, cancel_id);

        g_cond_clear (&waiter.cond);

        if (waiter.error != NULL)
        {
            g_propagate_error (error, waiter.error);
            return NULL;
        }

        /* Just released, so it was working moments ago: no need to ping */
        if (waiter.connection != NULL)
            return waiter.connection;

        take = ORM_POOL_TAKE_SLOT;
    }
    else
    {
        g_mutex_unlock (&self->lock);
    }

    /* --- An idle connection to check, or a slot to fill --- */
    switch (take)
    {
    case ORM_POOL_TAKE_CLOSED:
        orm_pool_set_closed_error (error);
        return NULL;

    case ORM_POOL_TAKE_IDLE:
        return orm_pool_prepare (self, connection, validate, error);

    case ORM_POOL_TAKE_SLOT:
    case ORM_POOL_TAKE_WAIT:
    default:
        return orm_pool_prepare (self, NULL, FALSE, error);
    }
}

/* ------------------------------------------------------------------ */
/* Asynchronous acquire                                               */
/* ------------------------------------------------------------------ */

/*
 * Completes an acquire task with a connection, unless it was cancelled
 * in the meantime, in which case the connection goes straight back.
 *
 * The connection travels inside a lease so that a caller who never calls
 * _finish() -- a NULL callback, a task dropped on the floor -- still
 * returns it to the pool when GTask frees the unclaimed result.
 */
static void
orm_pool_deliver (OrmConnectionPool *self,
                  GTask             *task,
                  OrmConnection     *connection)
{
    GCancellable *cancellable = g_task_get_cancellable (task);

    if (cancellable != NULL && g_cancellable_is_cancelled (cancellable))
    {
        orm_connection_pool_release (self, connection);
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                                 "Operation was cancelled");
        return;
    }

    g_task_return_pointer (task, orm_pool_lease_new (self, connection),
                           orm_pool_lease_free);
}

/*
 * Validates or opens on a GIO worker thread, so neither a handshake nor
 * a round trip ever runs on the caller's main loop.  The task data is an
 * idle connection to check, or %NULL to open a fresh one.
 */
static void
orm_pool_prepare_thread (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
    OrmConnectionPool *self = ORM_CONNECTION_POOL (source_object);
    OrmConnection     *candidate = (OrmConnection *) task_data;
    OrmConnection     *connection;
    GError            *error = NULL;

    connection = orm_pool_prepare (self, candidate, candidate != NULL, &error);
    if (connection == NULL)
    {
        g_task_return_error (task, error);
        return;
    }

    orm_pool_deliver (self, task, connection);
}

static void
orm_pool_prepare_in_thread (GTask         *task,
                            OrmConnection *candidate)
{
    g_task_set_task_data (task, candidate, NULL);
    g_task_run_in_thread (task, orm_pool_prepare_thread);
}

/*
 * Acts on an asynchronous waiter's outcome.  Called exactly once, by
 * whoever resolved it, with the lock dropped; consumes the queue's
 * reference.
 */
static void
orm_pool_waiter_finish (OrmPoolWaiter *waiter)
{
    OrmConnectionPool *self = waiter->pool;
    OrmConnection     *connection;
    GSource           *timeout_source;
    GError            *error;
    GTask             *task;
    gulong             cancel_id;
    gboolean           slot;

    g_mutex_lock (&self->lock);
    waiter->finished = TRUE;
    cancel_id = waiter->cancel_id;
    waiter->cancel_id = 0;
    timeout_source = (GSource *) g_steal_pointer (&waiter->timeout_source);
    connection = (OrmConnection *) g_steal_pointer (&waiter->connection);
    error = (GError *) g_steal_pointer (&waiter->error);
    slot = waiter->slot;
    task = (GTask *) g_steal_pointer (&waiter->task);
    g_mutex_unlock (&self->lock);

    /* --- Tear down whatever else could still have resolved it --- */
    if (timeout_source != NULL)
    {
        g_source_destroy (timeout_source);
        g_source_unref (timeout_source);
    }

    if (cancel_id != 0)
        g_cancellable_disconnect (g_task_get_cancellable (task), cancel_id);

    /* --- Deliver --- */
    if (connection != NULL)
        orm_pool_deliver (self, task, connection);
    else if (slot)
        orm_pool_prepare_in_thread (task, NULL);
    else
        g_task_return_error (task, error);

    g_object_unref (task);
    orm_pool_waiter_unref (waiter);
}

static gboolean
orm_pool_waiter_finish_idle (gpointer data)
{
    orm_pool_waiter_finish ((OrmPoolWaiter *) data);
    return G_SOURCE_REMOVE;
}

/*
 * An asynchronous waiter's cancellation.
 *
 * The finishing is deferred to an idle in the task's context rather than
 * done here: finishing disconnects this very handler, and disconnecting a
 * handler from inside itself is a deadlock.
 */
static void
orm_pool_async_waiter_cancelled (GCancellable *cancellable,
                                 gpointer      user_data)
{
    OrmPoolWaiter     *waiter = (OrmPoolWaiter *) user_data;
    OrmConnectionPool *self = waiter->pool;
    GSource           *idle;
    gboolean           resolved = FALSE;
    GMainContext      *context = NULL;

    g_mutex_lock (&self->lock);
    if (!waiter->done)
    {
        orm_pool_waiter_resolve_locked (self, waiter, NULL, FALSE,
                                        g_error_new_literal (G_IO_ERROR,
                                                             G_IO_ERROR_CANCELLED,
                                                             "Operation was cancelled"));
        context = g_task_get_context (waiter->task);
        resolved = TRUE;
    }
    g_mutex_unlock (&self->lock);

    if (!resolved)
        return;

    idle = g_idle_source_new ();
    g_source_set_static_name (idle, "[orm] pool acquire cancelled");
    g_source_set_callback (idle, orm_pool_waiter_finish_idle, waiter, NULL);
    g_source_attach (idle, context);
    g_source_unref (idle);
}

static gboolean
orm_pool_async_waiter_timed_out (gpointer data)
{
    OrmPoolWaiter     *waiter = (OrmPoolWaiter *) data;
    OrmConnectionPool *self = waiter->pool;
    gboolean           resolved = FALSE;

    g_mutex_lock (&self->lock);
    if (!waiter->done)
    {
        orm_pool_waiter_resolve_locked (
            self, waiter, NULL, FALSE,
            g_error_new (G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
                         "Timed out after %u ms waiting for a pooled "
                         "connection (max-size %u, all in use)",
                         self->acquire_timeout, self->max_size));
        resolved = TRUE;
    }
    g_mutex_unlock (&self->lock);

    if (resolved)
        orm_pool_waiter_finish (waiter);

    return G_SOURCE_REMOVE;
}

/**
 * orm_connection_pool_acquire_async:
 * @self: An #OrmConnectionPool
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when a connection is available
 * @user_data: Data for @callback
 *
 * Borrows a connection without blocking the calling thread.
 *
 * An idle connection that needs no check is handed over on the next main
 * loop iteration.  Opening a new one, or validating one that sat idle,
 * runs on a GIO worker thread.  When the pool is exhausted the request
 * queues behind earlier ones, timing out after
 * #OrmConnectionPool:acquire-timeout milliseconds.
 *
 * The callback runs in the thread-default #GMainContext of the calling
 * thread, so that context must be iterated.
 */
void
orm_connection_pool_acquire_async (OrmConnectionPool   *self,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;
    OrmPoolWaiter   *waiter = NULL;
    OrmConnection   *connection = NULL;
    OrmPoolTake      take;
    gboolean         validate = FALSE;
    gboolean         disconnect = FALSE;
    gulong           cancel_id;

    g_return_if_fail (ORM_IS_CONNECTION_POOL (self));
    g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_connection_pool_acquire_async);

    /*
     * The pool decides what cancellation means for a connection that is
     * already in hand -- it goes back -- so GTask must not quietly drop
     * the result instead.
     */
    g_task_set_check_cancellable (task, FALSE);

    if (g_task_return_error_if_cancelled (task))
        return;

    g_mutex_lock (&self->lock);
    take = orm_pool_try_take_locked (self, &connection, &validate);

    if (take == ORM_POOL_TAKE_WAIT)
    {
        waiter = g_new0 (OrmPoolWaiter, 1);
        waiter->ref_count = 1;          /* the queue's */
        waiter->is_async = TRUE;
        waiter->pool = self;
        waiter->task = g_object_ref (task);
        g_queue_push_tail (&self->waiters, waiter);

        if (self->acquire_timeout > 0)
        {
            waiter->timeout_source = g_timeout_source_new (self->acquire_timeout);
            g_source_set_static_name (waiter->timeout_source,
                                      "[orm] pool acquire timeout");
            g_source_set_callback (waiter->timeout_source,
                                   orm_pool_async_waiter_timed_out,
                                   orm_pool_waiter_ref (waiter),
                                   orm_pool_waiter_unref);
            g_source_attach (waiter->timeout_source, g_task_get_context (task));
        }

        /* Keeps it alive across the unlocked stretch below */
        orm_pool_waiter_ref (waiter);
    }

    g_mutex_unlock (&self->lock);

    switch (take)
    {
    case ORM_POOL_TAKE_CLOSED:
        g_task_return_new_error (task, ORM_ERROR, ORM_ERROR_CONNECTION_CLOSED,
                                 "Connection pool is closed");
        return;

    case ORM_POOL_TAKE_IDLE:
        if (validate)
            orm_pool_prepare_in_thread (task, connection);
        else
            orm_pool_deliver (self, task, connection);
        return;

    case ORM_POOL_TAKE_SLOT:
        orm_pool_prepare_in_thread (task, NULL);
        return;

    case ORM_POOL_TAKE_WAIT:
    default:
        break;
    }

    /*
     * --- Wire cancellation for the queued waiter ---
     *
     * Done after queueing, so a cancellation always finds the waiter
     * where it can be resolved.  Whoever finishes the waiter disconnects
     * the handler using the id stored here -- unless it already finished,
     * in which case nobody saw the id and it is ours to disconnect.
     */
    if (cancellable != NULL)
    {
        cancel_id = g_cancellable_connect (cancellable,
                                           G_CALLBACK (orm_pool_async_waiter_cancelled),
                                           orm_pool_waiter_ref (waiter),
                                           orm_pool_waiter_unref);

        g_mutex_lock (&self->lock);
        if (waiter->finished)
            disconnect = TRUE;
        else
            waiter->cancel_id = cancel_id;
        g_mutex_unlock (&self->lock);

        if (disconnect && cancel_id != 0)
            g_cancellable_disconnect (cancellable, cancel_id);
    }

    orm_pool_waiter_unref (waiter);
}

/**
 * orm_connection_pool_acquire_finish:
 * @self: An #OrmConnectionPool
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_connection_pool_acquire_async().
 *
 * Returns: (transfer full) (nullable): A connection to hand back with
 *   orm_connection_pool_release(), or %NULL on error
 */
OrmConnection *
orm_connection_pool_acquire_finish (OrmConnectionPool  *self,
                                    GAsyncResult       *result,
                                    GError            **error)
{
    OrmPoolLease  *lease;
    OrmConnection *connection;

    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    lease = (OrmPoolLease *) g_task_propagate_pointer (G_TASK (result), error);
    if (lease == NULL)
        return NULL;

    /* Collected: the lease no longer releases it on free */
    connection = (OrmConnection *) g_steal_pointer (&lease->connection);
    orm_pool_lease_free (lease);

    return connection;
}

/* ------------------------------------------------------------------ */
/* Release and close                                                  */
/* ------------------------------------------------------------------ */

/**
 * orm_connection_pool_release:
 * @self: An #OrmConnectionPool
 * @connection: (transfer full): A connection acquired from @self
 *
 * Hands a connection back to the pool.  Callable from any thread.
 *
 * Before anyone else can borrow it, the connection is reset:
 *
 *  - result sets and row streams still open on it are closed, and
 *    reading them afterwards fails with %ORM_ERROR_CONNECTION_CLOSED;
 *  - an open transaction is rolled back, including one begun with raw
 *    SQL, and any #OrmTransaction for it stops working rather than act on
 *    the next borrower's statements;
 *  - the session isolation level is restored to the backend's default.
 *
 * A connection that is closed, has lost its server, or cannot be reset is
 * closed and replaced on demand.
 *
 * Dropping a lent connection with g_object_unref() instead is a mistake
 * the pool survives -- it recovers the slot and warns -- but the
 * connection is closed rather than reused.
 *
 * Operations still queued on the connection run first; the next borrower
 * queues behind them.
 */
void
orm_connection_pool_release (OrmConnectionPool *self,
                             OrmConnection     *connection)
{
    g_autoptr(GError) error = NULL;
    gboolean          was_lent;
    gboolean          closed;
    gboolean          usable = FALSE;

    g_return_if_fail (ORM_IS_CONNECTION_POOL (self));
    g_return_if_fail (ORM_IS_CONNECTION (connection));

    g_mutex_lock (&self->lock);
    was_lent = orm_pool_unlend_locked (self, connection);
    closed = self->closed;
    g_mutex_unlock (&self->lock);

    /*
     * Not ours to take.  The reference is deliberately not dropped: the
     * likeliest cause is a second release of the same connection, and
     * then the caller no longer owns one -- unreferencing would take the
     * pool's own, and the idle connection would be freed under it.  A
     * leak on a programming error beats a use-after-free.
     */
    if (!was_lent)
    {
        g_critical ("%s: connection %p was not acquired from pool %p, or "
                    "was already released",
                    G_STRFUNC, (gpointer) connection, (gpointer) self);
        return;
    }

    if (!closed)
    {
        usable = orm_connection_reset_for_reuse (connection, &error);

        if (!usable && error != NULL)
        {
            if (g_error_matches (error, ORM_ERROR, ORM_ERROR_CONNECTION_CLOSED))
                g_debug ("OrmConnectionPool: discarding a closed connection");
            else
                g_warning ("OrmConnectionPool: discarding a connection that "
                           "could not be reset: %s", error->message);
        }
    }

    orm_pool_put_back (self, connection, usable);
}

/**
 * orm_connection_pool_close:
 * @self: An #OrmConnectionPool
 *
 * Shuts the pool down.  Idle connections are closed now, queued
 * acquirers fail with %ORM_ERROR_CONNECTION_CLOSED, lent connections are
 * closed as they are released, and every later acquire fails.  Safe to
 * call more than once.
 */
void
orm_connection_pool_close (OrmConnectionPool *self)
{
    GQueue         idle = G_QUEUE_INIT;
    GList         *async_waiters = NULL;
    GList         *l;
    GSource       *reaper;
    OrmPoolWaiter *waiter;
    OrmPoolEntry  *entry;

    g_return_if_fail (ORM_IS_CONNECTION_POOL (self));

    g_mutex_lock (&self->lock);

    if (self->closed)
    {
        g_mutex_unlock (&self->lock);
        return;
    }

    self->closed = TRUE;

    /* --- Take the idle list whole --- */
    idle = self->idle;
    g_queue_init (&self->idle);
    self->n_open -= idle.length;

    /* --- Fail every waiter --- */
    while ((waiter = (OrmPoolWaiter *) g_queue_peek_head (&self->waiters)) != NULL)
    {
        orm_pool_waiter_resolve_locked (self, waiter, NULL, FALSE,
                                        g_error_new_literal (ORM_ERROR,
                                                             ORM_ERROR_CONNECTION_CLOSED,
                                                             "Connection pool is closed"));
        if (waiter->is_async)
            async_waiters = g_list_prepend (async_waiters, waiter);
    }

    reaper = (GSource *) g_steal_pointer (&self->reaper);

    g_mutex_unlock (&self->lock);

    if (reaper != NULL)
    {
        g_source_destroy (reaper);
        g_source_unref (reaper);
    }

    while ((entry = (OrmPoolEntry *) g_queue_pop_head (&idle)) != NULL)
    {
        orm_pool_dispose_connection (entry->connection);
        g_free (entry);
    }

    async_waiters = g_list_reverse (async_waiters);
    for (l = async_waiters; l != NULL; l = l->next)
        orm_pool_waiter_finish ((OrmPoolWaiter *) l->data);
    g_list_free (async_waiters);
}

/* ------------------------------------------------------------------ */
/* Idle reaper                                                        */
/* ------------------------------------------------------------------ */

/*
 * Closes connections that have sat idle past the timeout, oldest first,
 * never going below min-size.  Runs in the pool's main context.
 */
static void
orm_pool_reap (OrmConnectionPool *self)
{
    GList        *victims = NULL;
    GList        *l;
    OrmPoolEntry *entry;
    gint64        cutoff;

    g_mutex_lock (&self->lock);

    if (self->closed || self->idle_timeout == 0)
    {
        g_mutex_unlock (&self->lock);
        return;
    }

    cutoff = g_get_monotonic_time () -
             (gint64) self->idle_timeout * G_USEC_PER_SEC;

    /* The tail is the longest idle: the head is pushed on every release */
    while (self->n_open > self->min_size &&
           (entry = (OrmPoolEntry *) g_queue_peek_tail (&self->idle)) != NULL &&
           entry->idle_since <= cutoff)
    {
        g_queue_pop_tail (&self->idle);
        self->n_open--;
        victims = g_list_prepend (victims, entry);
    }

    g_mutex_unlock (&self->lock);

    for (l = victims; l != NULL; l = l->next)
    {
        entry = (OrmPoolEntry *) l->data;
        orm_pool_dispose_connection (entry->connection);
        g_free (entry);
    }
    g_list_free (victims);
}

/*
 * The reaper holds the pool weakly: a timer in somebody's main loop is no
 * reason to keep a pool alive, and a pool finalized on another thread
 * mid-dispatch must not be touched.
 */
static gboolean
orm_pool_reaper_tick (gpointer data)
{
    g_autoptr(OrmConnectionPool) self = NULL;

    self = (OrmConnectionPool *) g_weak_ref_get ((GWeakRef *) data);
    if (self == NULL)
        return G_SOURCE_REMOVE;

    orm_pool_reap (self);

    return G_SOURCE_CONTINUE;
}

static void
orm_pool_reaper_data_free (gpointer data)
{
    g_weak_ref_clear ((GWeakRef *) data);
    g_free (data);
}

/*
 * Replaces the reaper to match the current idle timeout.  The tick is
 * half the timeout so a connection outlives it by at most that much.
 */
static void
orm_pool_restart_reaper (OrmConnectionPool *self)
{
    GSource  *old_reaper;
    GSource  *new_reaper = NULL;
    GWeakRef *weak;
    guint     interval;

    g_mutex_lock (&self->lock);

    if (!self->closed && self->idle_timeout > 0)
    {
        interval = CLAMP (self->idle_timeout / 2,
                          ORM_CONNECTION_POOL_REAPER_MIN_INTERVAL,
                          ORM_CONNECTION_POOL_REAPER_MAX_INTERVAL);

        weak = g_new0 (GWeakRef, 1);
        g_weak_ref_init (weak, self);

        new_reaper = g_timeout_source_new_seconds (interval);
        g_source_set_static_name (new_reaper, "[orm] pool idle reaper");
        g_source_set_callback (new_reaper, orm_pool_reaper_tick, weak,
                               orm_pool_reaper_data_free);
    }

    old_reaper = self->reaper;
    self->reaper = new_reaper;

    g_mutex_unlock (&self->lock);

    if (old_reaper != NULL)
    {
        g_source_destroy (old_reaper);
        g_source_unref (old_reaper);
    }

    /* Attached unlocked: a source may dispatch the moment it is attached */
    if (new_reaper != NULL)
        g_source_attach (new_reaper, self->main_context);
}

/* ------------------------------------------------------------------ */
/* One-shot helpers                                                   */
/* ------------------------------------------------------------------ */

/**
 * orm_connection_pool_execute:
 * @self: An #OrmConnectionPool
 * @sql: SQL statement with placeholders
 * @params: (element-type OrmValue) (nullable): Parameter values
 * @error: Return location for error
 *
 * Acquires a connection, runs one statement that returns no rows, and
 * releases it.  Blocks for the acquire as well as the statement.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_connection_pool_execute (OrmConnectionPool  *self,
                             const gchar        *sql,
                             GList              *params,
                             GError            **error)
{
    OrmConnection *connection;
    gboolean       ok;

    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), FALSE);
    g_return_val_if_fail (sql != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    connection = orm_connection_pool_acquire (self, NULL, error);
    if (connection == NULL)
        return FALSE;

    ok = orm_connection_execute_with_params (connection, sql, params, error);
    orm_connection_pool_release (self, connection);

    return ok;
}

/**
 * orm_connection_pool_query:
 * @self: An #OrmConnectionPool
 * @sql: SQL query with placeholders
 * @params: (element-type OrmValue) (nullable): Parameter values
 * @error: Return location for error
 *
 * Acquires a connection, runs one query on it, reads every row, and
 * releases the connection before returning.  The result is held in
 * memory and needs nothing from the pool, so it can be kept as long as
 * you like.  For a result too large to hold, acquire a connection and
 * use orm_connection_query_stream_async() instead.
 *
 * Returns: (transfer full) (nullable): The result set, or %NULL on error
 */
OrmResult *
orm_connection_pool_query (OrmConnectionPool  *self,
                           const gchar        *sql,
                           GList              *params,
                           GError            **error)
{
    OrmConnection *connection;
    OrmResult     *result;

    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), NULL);
    g_return_val_if_fail (sql != NULL, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    connection = orm_connection_pool_acquire (self, NULL, error);
    if (connection == NULL)
        return NULL;

    result = orm_connection_query_with_params (connection, sql, params, error);
    if (result != NULL && !orm_result_buffer (result, error))
        g_clear_object (&result);

    orm_connection_pool_release (self, connection);

    return result;
}

/*
 * A one-shot statement in flight: what to run, and the connection it is
 * running on once there is one.  The parameters are copied because the
 * caller's list is (transfer none) and the statement starts only after
 * an acquire that may take a while.
 */
typedef struct
{
    gchar         *sql;
    GList         *params;
    OrmConnection *connection;      /* Lent, owned */
} OrmPoolStatement;

static OrmPoolStatement *
orm_pool_statement_new (const gchar *sql,
                        GList       *params)
{
    OrmPoolStatement *statement;
    GList            *l;

    statement = g_new0 (OrmPoolStatement, 1);
    statement->sql = g_strdup (sql);

    for (l = params; l != NULL; l = l->next)
        statement->params = g_list_prepend (statement->params,
                                            orm_value_copy ((const OrmValue *) l->data));
    statement->params = g_list_reverse (statement->params);

    return statement;
}

/*
 * Freed with the task.  Both completion paths hand the connection back
 * before returning the task, so @connection is normally %NULL here; the
 * clear is only a backstop against leaking the reference.
 */
static void
orm_pool_statement_free (gpointer data)
{
    OrmPoolStatement *statement = (OrmPoolStatement *) data;

    g_free (statement->sql);
    g_list_free_full (statement->params, (GDestroyNotify) orm_value_free);
    g_clear_object (&statement->connection);
    g_free (statement);
}

static void
orm_pool_execute_done (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
    g_autoptr(GTask)   task = (GTask *) user_data;
    OrmConnectionPool *self = ORM_CONNECTION_POOL (g_task_get_source_object (task));
    OrmPoolStatement  *statement = (OrmPoolStatement *) g_task_get_task_data (task);
    GError            *error = NULL;
    gboolean           ok;

    ok = orm_connection_execute_finish (ORM_CONNECTION (source), result, &error);

    orm_connection_pool_release (self, g_steal_pointer (&statement->connection));

    if (ok)
        g_task_return_boolean (task, TRUE);
    else
        g_task_return_error (task, error);
}

static void
orm_pool_execute_acquired (GObject      *source,
                           GAsyncResult *result,
                           gpointer      user_data)
{
    GTask             *task = (GTask *) user_data;
    OrmPoolStatement  *statement = (OrmPoolStatement *) g_task_get_task_data (task);
    GError            *error = NULL;

    statement->connection = orm_connection_pool_acquire_finish (ORM_CONNECTION_POOL (source),
                                                                result, &error);
    if (statement->connection == NULL)
    {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    orm_connection_execute_async (statement->connection, statement->sql,
                                  statement->params,
                                  g_task_get_cancellable (task),
                                  orm_pool_execute_done, task);
}

/**
 * orm_connection_pool_execute_async:
 * @self: An #OrmConnectionPool
 * @sql: SQL statement with placeholders
 * @params: (element-type OrmValue) (nullable) (transfer none): Parameter
 *   values, copied before the call returns
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the statement has run
 * @user_data: Data for @callback
 *
 * Acquires a connection, runs one statement that returns no rows on its
 * worker thread, and releases it.  Independent calls run in parallel on
 * as many connections as the pool allows.
 */
void
orm_connection_pool_execute_async (OrmConnectionPool   *self,
                                   const gchar         *sql,
                                   GList               *params,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
    GTask *task;

    g_return_if_fail (ORM_IS_CONNECTION_POOL (self));
    g_return_if_fail (sql != NULL);
    g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_connection_pool_execute_async);
    g_task_set_task_data (task, orm_pool_statement_new (sql, params),
                          orm_pool_statement_free);

    /* The task's reference travels through both callbacks */
    orm_connection_pool_acquire_async (self, cancellable,
                                       orm_pool_execute_acquired, task);
}

/**
 * orm_connection_pool_execute_finish:
 * @self: An #OrmConnectionPool
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_connection_pool_execute_async().
 *
 * Returns: %TRUE on success
 */
gboolean
orm_connection_pool_execute_finish (OrmConnectionPool  *self,
                                    GAsyncResult       *result,
                                    GError            **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
orm_pool_query_done (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
    g_autoptr(GTask)   task = (GTask *) user_data;
    OrmConnectionPool *self = ORM_CONNECTION_POOL (g_task_get_source_object (task));
    OrmPoolStatement  *statement = (OrmPoolStatement *) g_task_get_task_data (task);
    OrmResult         *rows;
    GError            *error = NULL;

    /* Already buffered on the worker: the connection can go straight back */
    rows = orm_connection_query_finish (ORM_CONNECTION (source), result, &error);

    orm_connection_pool_release (self, g_steal_pointer (&statement->connection));

    if (rows == NULL)
        g_task_return_error (task, error);
    else
        g_task_return_pointer (task, rows, g_object_unref);
}

static void
orm_pool_query_acquired (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
    GTask             *task = (GTask *) user_data;
    OrmPoolStatement  *statement = (OrmPoolStatement *) g_task_get_task_data (task);
    GError            *error = NULL;

    statement->connection = orm_connection_pool_acquire_finish (ORM_CONNECTION_POOL (source),
                                                                result, &error);
    if (statement->connection == NULL)
    {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    orm_connection_query_buffered_async (statement->connection, statement->sql,
                                         statement->params,
                                         g_task_get_cancellable (task),
                                         orm_pool_query_done, task);
}

/**
 * orm_connection_pool_query_async:
 * @self: An #OrmConnectionPool
 * @sql: SQL query with placeholders
 * @params: (element-type OrmValue) (nullable) (transfer none): Parameter
 *   values, copied before the call returns
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the result set is ready
 * @user_data: Data for @callback
 *
 * Acquires a connection and runs one query on its worker thread, reading
 * every row there too, then releases the connection before @callback
 * runs.  The result is held in memory and needs nothing from the pool;
 * nothing on the calling thread waits for the database.
 */
void
orm_connection_pool_query_async (OrmConnectionPool   *self,
                                 const gchar         *sql,
                                 GList               *params,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
    GTask *task;

    g_return_if_fail (ORM_IS_CONNECTION_POOL (self));
    g_return_if_fail (sql != NULL);
    g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_connection_pool_query_async);
    g_task_set_task_data (task, orm_pool_statement_new (sql, params),
                          orm_pool_statement_free);

    orm_connection_pool_acquire_async (self, cancellable,
                                       orm_pool_query_acquired, task);
}

/**
 * orm_connection_pool_query_finish:
 * @self: An #OrmConnectionPool
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_connection_pool_query_async().
 *
 * Returns: (transfer full) (nullable): The result set, or %NULL on error
 */
OrmResult *
orm_connection_pool_query_finish (OrmConnectionPool  *self,
                                  GAsyncResult       *result,
                                  GError            **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return (OrmResult *) g_task_propagate_pointer (G_TASK (result), error);
}

/* ------------------------------------------------------------------ */
/* Accessors                                                          */
/* ------------------------------------------------------------------ */

/**
 * orm_connection_pool_get_engine:
 * @self: An #OrmConnectionPool
 *
 * Returns: (transfer none): The engine connections are opened from
 */
OrmEngine *
orm_connection_pool_get_engine (OrmConnectionPool *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), NULL);
    return self->engine;
}

/**
 * orm_connection_pool_get_main_context:
 * @self: An #OrmConnectionPool
 *
 * Returns: (transfer none): The context the idle reaper runs in and
 *   pooled connections deliver their signals to
 */
GMainContext *
orm_connection_pool_get_main_context (OrmConnectionPool *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), NULL);
    return self->main_context;
}

/**
 * orm_connection_pool_get_max_size:
 * @self: An #OrmConnectionPool
 *
 * Returns: The most connections the pool keeps open at once
 */
guint
orm_connection_pool_get_max_size (OrmConnectionPool *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), 0);
    return self->max_size;
}

/*
 * Reads one guint setting under the lock; they change from any thread.
 */
static guint
orm_pool_read_setting (OrmConnectionPool *self,
                       const guint       *field)
{
    guint value;

    g_mutex_lock (&self->lock);
    value = *field;
    g_mutex_unlock (&self->lock);

    return value;
}

/*
 * Writes one guint setting under the lock, notifying only on change.
 *
 * Returns: %TRUE if the value changed
 */
static gboolean
orm_pool_write_setting (OrmConnectionPool *self,
                        guint             *field,
                        guint              value,
                        guint              prop_id)
{
    gboolean changed;

    g_mutex_lock (&self->lock);
    changed = *field != value;
    *field = value;
    g_mutex_unlock (&self->lock);

    if (changed)
        g_object_notify_by_pspec (G_OBJECT (self), properties[prop_id]);

    return changed;
}

/**
 * orm_connection_pool_get_min_size:
 * @self: An #OrmConnectionPool
 *
 * Returns: The number of connections the idle reaper never goes below
 */
guint
orm_connection_pool_get_min_size (OrmConnectionPool *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), 0);
    return orm_pool_read_setting (self, &self->min_size);
}

/**
 * orm_connection_pool_set_min_size:
 * @self: An #OrmConnectionPool
 * @min_size: Connections the idle reaper leaves open, clamped to max-size
 *
 * Sets the floor for idle reaping.  Does not open connections: the pool
 * only ever opens one because somebody asked for it.
 */
void
orm_connection_pool_set_min_size (OrmConnectionPool *self,
                                  guint              min_size)
{
    g_return_if_fail (ORM_IS_CONNECTION_POOL (self));

    orm_pool_write_setting (self, &self->min_size,
                            MIN (min_size, self->max_size), PROP_MIN_SIZE);
}

/**
 * orm_connection_pool_get_idle_timeout:
 * @self: An #OrmConnectionPool
 *
 * Returns: Seconds an idle connection is kept, or 0 for forever
 */
guint
orm_connection_pool_get_idle_timeout (OrmConnectionPool *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), 0);
    return orm_pool_read_setting (self, &self->idle_timeout);
}

/**
 * orm_connection_pool_set_idle_timeout:
 * @self: An #OrmConnectionPool
 * @seconds: Seconds an idle connection is kept, or 0 for forever
 *
 * Sets how long an unused connection stays open.  Reaping runs in the
 * pool's main context, so it only happens while that context is being
 * iterated.
 */
void
orm_connection_pool_set_idle_timeout (OrmConnectionPool *self,
                                      guint              seconds)
{
    g_return_if_fail (ORM_IS_CONNECTION_POOL (self));

    if (orm_pool_write_setting (self, &self->idle_timeout, seconds,
                                PROP_IDLE_TIMEOUT))
        orm_pool_restart_reaper (self);
}

/**
 * orm_connection_pool_get_acquire_timeout:
 * @self: An #OrmConnectionPool
 *
 * Returns: Milliseconds an acquirer waits for a free connection, or 0
 *   for forever
 */
guint
orm_connection_pool_get_acquire_timeout (OrmConnectionPool *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), 0);
    return orm_pool_read_setting (self, &self->acquire_timeout);
}

/**
 * orm_connection_pool_set_acquire_timeout:
 * @self: An #OrmConnectionPool
 * @milliseconds: How long an acquirer waits, or 0 for forever
 *
 * Sets how long acquiring waits when the pool is exhausted.  Applies to
 * acquisitions started afterwards.
 */
void
orm_connection_pool_set_acquire_timeout (OrmConnectionPool *self,
                                         guint              milliseconds)
{
    g_return_if_fail (ORM_IS_CONNECTION_POOL (self));

    orm_pool_write_setting (self, &self->acquire_timeout, milliseconds,
                            PROP_ACQUIRE_TIMEOUT);
}

/**
 * orm_connection_pool_get_validation_interval:
 * @self: An #OrmConnectionPool
 *
 * Returns: Seconds a connection may sit idle before it is checked on acquire
 */
guint
orm_connection_pool_get_validation_interval (OrmConnectionPool *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), 0);
    return orm_pool_read_setting (self, &self->validation_interval);
}

/**
 * orm_connection_pool_set_validation_interval:
 * @self: An #OrmConnectionPool
 * @seconds: Idle seconds after which a connection is checked before
 *   being lent; 0 checks every time, %G_MAXUINT never
 *
 * Sets when an idle connection is pinged with "SELECT 1" before it is
 * handed out.  A connection whose server went away while it sat idle is
 * then replaced instead of failing the borrower's first statement.
 */
void
orm_connection_pool_set_validation_interval (OrmConnectionPool *self,
                                             guint              seconds)
{
    g_return_if_fail (ORM_IS_CONNECTION_POOL (self));

    orm_pool_write_setting (self, &self->validation_interval, seconds,
                            PROP_VALIDATION_INTERVAL);
}

/**
 * orm_connection_pool_dup_init_sql:
 * @self: An #OrmConnectionPool
 *
 * Returns: (transfer full) (nullable): The SQL run on each new connection
 */
gchar *
orm_connection_pool_dup_init_sql (OrmConnectionPool *self)
{
    gchar *sql;

    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), NULL);

    g_mutex_lock (&self->lock);
    sql = g_strdup (self->init_sql);
    g_mutex_unlock (&self->lock);

    return sql;
}

/**
 * orm_connection_pool_set_init_sql:
 * @self: An #OrmConnectionPool
 * @sql: (nullable): SQL run once on every connection the pool opens
 *
 * Sets per-connection setup, run before a new connection is first lent:
 * "PRAGMA busy_timeout = 5000" so concurrent SQLite writers wait instead
 * of failing, "SET search_path TO app" on PostgreSQL, and so on.  A
 * connection whose init SQL fails is closed and the acquire fails with
 * that error.  Connections already open are not touched.
 */
void
orm_connection_pool_set_init_sql (OrmConnectionPool *self,
                                  const gchar       *sql)
{
    gboolean changed;

    g_return_if_fail (ORM_IS_CONNECTION_POOL (self));

    g_mutex_lock (&self->lock);
    changed = g_strcmp0 (self->init_sql, sql) != 0;
    if (changed)
    {
        g_free (self->init_sql);
        self->init_sql = g_strdup (sql);
    }
    g_mutex_unlock (&self->lock);

    if (changed)
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INIT_SQL]);
}

/**
 * orm_connection_pool_get_size:
 * @self: An #OrmConnectionPool
 *
 * Returns: Connections open or opening, idle and lent alike
 */
guint
orm_connection_pool_get_size (OrmConnectionPool *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), 0);
    return orm_pool_read_setting (self, &self->n_open);
}

/**
 * orm_connection_pool_get_idle_count:
 * @self: An #OrmConnectionPool
 *
 * Returns: Connections open and waiting to be lent
 */
guint
orm_connection_pool_get_idle_count (OrmConnectionPool *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), 0);
    return orm_pool_read_setting (self, &self->idle.length);
}

/**
 * orm_connection_pool_get_waiting_count:
 * @self: An #OrmConnectionPool
 *
 * Returns: Acquirers queued for a connection
 */
guint
orm_connection_pool_get_waiting_count (OrmConnectionPool *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), 0);
    return orm_pool_read_setting (self, &self->waiters.length);
}

/**
 * orm_connection_pool_is_closed:
 * @self: An #OrmConnectionPool
 *
 * Returns: %TRUE once orm_connection_pool_close() has been called
 */
gboolean
orm_connection_pool_is_closed (OrmConnectionPool *self)
{
    gboolean closed;

    g_return_val_if_fail (ORM_IS_CONNECTION_POOL (self), TRUE);

    g_mutex_lock (&self->lock);
    closed = self->closed;
    g_mutex_unlock (&self->lock);

    return closed;
}

/* ------------------------------------------------------------------ */
/* GObject                                                            */
/* ------------------------------------------------------------------ */

/*
 * Every SQLite connection to ":memory:" is its own private database, so a
 * pool of them would hand each borrower a different, empty schema.
 */
static gboolean
orm_pool_engine_is_private_memory (OrmEngine *engine)
{
    const gchar *url;

    if (orm_engine_get_dialect_type (engine) != ORM_DIALECT_SQLITE)
        return FALSE;

    url = orm_engine_get_url (engine);

    return url != NULL && g_str_has_suffix (url, ":memory:");
}

static gboolean
orm_connection_pool_initable_init (GInitable     *initable,
                                   GCancellable  *cancellable,
                                   GError       **error)
{
    OrmConnectionPool *self = ORM_CONNECTION_POOL (initable);

    if (self->engine == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                     "OrmConnectionPool needs an engine");
        return FALSE;
    }

    if (self->max_size > 1 && orm_pool_engine_is_private_memory (self->engine))
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                     "An in-memory SQLite database exists once per connection, "
                     "so a pool of %u would give each borrower a different "
                     "empty database; use a file, or max-size 1",
                     self->max_size);
        return FALSE;
    }

    return TRUE;
}

static void
orm_connection_pool_initable_iface_init (GInitableIface *iface)
{
    iface->init = orm_connection_pool_initable_init;
}

static void
orm_connection_pool_constructed (GObject *object)
{
    OrmConnectionPool *self = ORM_CONNECTION_POOL (object);

    G_OBJECT_CLASS (orm_connection_pool_parent_class)->constructed (object);

    /* "The thread that made the pool" is the useful default owner */
    if (self->main_context == NULL)
        self->main_context = g_main_context_ref_thread_default ();

    self->min_size = MIN (self->min_size, self->max_size);
    self->is_constructed = TRUE;

    orm_pool_restart_reaper (self);
}

static void
orm_connection_pool_dispose (GObject *object)
{
    OrmConnectionPool *self = ORM_CONNECTION_POOL (object);

    orm_connection_pool_close (self);

    G_OBJECT_CLASS (orm_connection_pool_parent_class)->dispose (object);
}

static void
orm_connection_pool_finalize (GObject *object)
{
    OrmConnectionPool *self = ORM_CONNECTION_POOL (object);

    /*
     * Anything still in @lent was never released.  Those references belong
     * to their borrowers, so there is nothing to drop -- only to report.
     */
    if (g_hash_table_size (self->lent) > 0)
        g_warning ("OrmConnectionPool finalized with %u connection(s) still "
                   "lent; release every acquired connection",
                   g_hash_table_size (self->lent));

    g_clear_pointer (&self->lent, g_hash_table_unref);
    g_clear_pointer (&self->init_sql, g_free);
    g_clear_pointer (&self->main_context, g_main_context_unref);
    g_clear_object (&self->engine);
    g_mutex_clear (&self->lock);

    G_OBJECT_CLASS (orm_connection_pool_parent_class)->finalize (object);
}

static void
orm_connection_pool_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
    OrmConnectionPool *self = ORM_CONNECTION_POOL (object);

    switch (prop_id)
    {
    case PROP_ENGINE:
        g_value_set_object (value, self->engine);
        break;
    case PROP_MAIN_CONTEXT:
        g_value_set_boxed (value, self->main_context);
        break;
    case PROP_MAX_SIZE:
        g_value_set_uint (value, self->max_size);
        break;
    case PROP_MIN_SIZE:
        g_value_set_uint (value, orm_connection_pool_get_min_size (self));
        break;
    case PROP_IDLE_TIMEOUT:
        g_value_set_uint (value, orm_connection_pool_get_idle_timeout (self));
        break;
    case PROP_ACQUIRE_TIMEOUT:
        g_value_set_uint (value, orm_connection_pool_get_acquire_timeout (self));
        break;
    case PROP_VALIDATION_INTERVAL:
        g_value_set_uint (value, orm_connection_pool_get_validation_interval (self));
        break;
    case PROP_INIT_SQL:
        g_value_take_string (value, orm_connection_pool_dup_init_sql (self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_connection_pool_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
    OrmConnectionPool *self = ORM_CONNECTION_POOL (object);

    switch (prop_id)
    {
    case PROP_ENGINE:
        self->engine = g_value_dup_object (value);
        break;
    case PROP_MAIN_CONTEXT:
        self->main_context = g_value_dup_boxed (value);
        break;
    case PROP_MAX_SIZE:
        self->max_size = g_value_get_uint (value);
        break;
    case PROP_MIN_SIZE:
        /* Before construction max-size may not be set yet: clamped in constructed() */
        if (self->is_constructed)
            orm_connection_pool_set_min_size (self, g_value_get_uint (value));
        else
            self->min_size = g_value_get_uint (value);
        break;
    case PROP_IDLE_TIMEOUT:
        if (self->is_constructed)
            orm_connection_pool_set_idle_timeout (self, g_value_get_uint (value));
        else
            self->idle_timeout = g_value_get_uint (value);
        break;
    case PROP_ACQUIRE_TIMEOUT:
        if (self->is_constructed)
            orm_connection_pool_set_acquire_timeout (self, g_value_get_uint (value));
        else
            self->acquire_timeout = g_value_get_uint (value);
        break;
    case PROP_VALIDATION_INTERVAL:
        if (self->is_constructed)
            orm_connection_pool_set_validation_interval (self, g_value_get_uint (value));
        else
            self->validation_interval = g_value_get_uint (value);
        break;
    case PROP_INIT_SQL:
        orm_connection_pool_set_init_sql (self, g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_connection_pool_class_init (OrmConnectionPoolClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->constructed = orm_connection_pool_constructed;
    object_class->dispose = orm_connection_pool_dispose;
    object_class->finalize = orm_connection_pool_finalize;
    object_class->get_property = orm_connection_pool_get_property;
    object_class->set_property = orm_connection_pool_set_property;

    /**
     * OrmConnectionPool:engine:
     *
     * The engine every pooled connection is opened from.
     */
    properties[PROP_ENGINE] =
        g_param_spec_object ("engine", NULL, NULL,
                             ORM_TYPE_ENGINE,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    /**
     * OrmConnectionPool:main-context:
     *
     * Where the idle reaper runs and pooled connections deliver their
     * signals.  %NULL at construction means the thread-default context
     * of the constructing thread.
     */
    properties[PROP_MAIN_CONTEXT] =
        g_param_spec_boxed ("main-context", NULL, NULL,
                            G_TYPE_MAIN_CONTEXT,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                            G_PARAM_STATIC_STRINGS);

    /**
     * OrmConnectionPool:max-size:
     *
     * The most connections open at once, lent and idle together.
     */
    properties[PROP_MAX_SIZE] =
        g_param_spec_uint ("max-size", NULL, NULL,
                           1, G_MAXUINT, ORM_CONNECTION_POOL_DEFAULT_MAX_SIZE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

    /**
     * OrmConnectionPool:min-size:
     *
     * Connections the idle reaper leaves open however long they idle.
     */
    properties[PROP_MIN_SIZE] =
        g_param_spec_uint ("min-size", NULL, NULL,
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                           G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

    /**
     * OrmConnectionPool:idle-timeout:
     *
     * Seconds an unused connection stays open, 0 for forever.
     */
    properties[PROP_IDLE_TIMEOUT] =
        g_param_spec_uint ("idle-timeout", NULL, NULL,
                           0, G_MAXUINT, ORM_CONNECTION_POOL_DEFAULT_IDLE_TIMEOUT,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                           G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

    /**
     * OrmConnectionPool:acquire-timeout:
     *
     * Milliseconds an acquirer waits on an exhausted pool, 0 for forever.
     */
    properties[PROP_ACQUIRE_TIMEOUT] =
        g_param_spec_uint ("acquire-timeout", NULL, NULL,
                           0, G_MAXUINT, ORM_CONNECTION_POOL_DEFAULT_ACQUIRE_TIMEOUT,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                           G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

    /**
     * OrmConnectionPool:validation-interval:
     *
     * Idle seconds after which a connection is pinged before being lent;
     * 0 pings every time, %G_MAXUINT never.
     */
    properties[PROP_VALIDATION_INTERVAL] =
        g_param_spec_uint ("validation-interval", NULL, NULL,
                           0, G_MAXUINT, ORM_CONNECTION_POOL_DEFAULT_VALIDATION_INTERVAL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                           G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

    /**
     * OrmConnectionPool:init-sql:
     *
     * SQL run once on every connection the pool opens.
     */
    properties[PROP_INIT_SQL] =
        g_param_spec_string ("init-sql", NULL, NULL,
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
orm_connection_pool_init (OrmConnectionPool *self)
{
    g_mutex_init (&self->lock);

    self->engine = NULL;
    self->main_context = NULL;
    self->max_size = ORM_CONNECTION_POOL_DEFAULT_MAX_SIZE;
    self->min_size = 0;
    self->idle_timeout = ORM_CONNECTION_POOL_DEFAULT_IDLE_TIMEOUT;
    self->acquire_timeout = ORM_CONNECTION_POOL_DEFAULT_ACQUIRE_TIMEOUT;
    self->validation_interval = ORM_CONNECTION_POOL_DEFAULT_VALIDATION_INTERVAL;
    self->init_sql = NULL;

    self->n_open = 0;
    g_queue_init (&self->idle);
    self->lent = g_hash_table_new (g_direct_hash, g_direct_equal);
    g_queue_init (&self->waiters);
    self->closed = FALSE;
    self->reaper = NULL;
    self->is_constructed = FALSE;
}

/**
 * orm_connection_pool_new:
 * @engine: The engine connections are opened from
 * @max_size: Most connections open at once, at least 1
 * @error: Return location for error
 *
 * Creates a pool bound to the calling thread's thread-default
 * #GMainContext.  No connection is opened until one is asked for.
 *
 * Use g_initable_new() with #OrmConnectionPool:main-context to bind it
 * elsewhere, or to set the other properties at construction.
 *
 * Returns: (transfer full) (nullable): A new #OrmConnectionPool, or %NULL on error
 */
OrmConnectionPool *
orm_connection_pool_new (OrmEngine  *engine,
                         guint       max_size,
                         GError    **error)
{
    g_return_val_if_fail (ORM_IS_ENGINE (engine), NULL);
    g_return_val_if_fail (max_size >= 1, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    return (OrmConnectionPool *) g_initable_new (ORM_TYPE_CONNECTION_POOL,
                                                 NULL, error,
                                                 "engine", engine,
                                                 "max-size", max_size,
                                                 NULL);
}
