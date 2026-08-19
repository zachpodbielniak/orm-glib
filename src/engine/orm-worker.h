/* orm-worker.h
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

#ifndef ORM_WORKER_H
#define ORM_WORKER_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/*
 * OrmWorker:
 *
 * One thread, one queue, jobs run in the order they were pushed.
 *
 * This is private plumbing and is deliberately not installed: it exists
 * because a database connection is confined to a single thread, so the
 * only way to keep the caller's thread free is to move the connection's
 * work onto a thread of its own and keep it there.  A thread pool would
 * be wrong -- two pool threads could run two statements on one
 * connection at once, which is exactly the thing the backends forbid.
 */

typedef struct _OrmWorker OrmWorker;

/*
 * OrmWorkerJobFunc:
 * @task_data: The data handed to orm_worker_push()
 * @task: (nullable): The task to complete, or %NULL for a bare job
 *
 * The body of an asynchronous job, run on the worker thread.
 */
typedef void (*OrmWorkerJobFunc) (gpointer  task_data,
                                  GTask    *task);

/*
 * OrmWorkerSyncFunc:
 * @data: The data handed to orm_worker_invoke_sync()
 *
 * The body of a synchronous job: the pushing thread blocks until it has
 * run, so @data may point at that thread's stack.
 */
typedef void (*OrmWorkerSyncFunc) (gpointer data);

G_GNUC_INTERNAL
OrmWorker * orm_worker_new (const gchar *name);

G_GNUC_INTERNAL
void orm_worker_shutdown (OrmWorker *self);

G_GNUC_INTERNAL
gboolean orm_worker_is_current (OrmWorker *self);

G_GNUC_INTERNAL
void orm_worker_push (OrmWorker        *self,
                      OrmWorkerJobFunc  run,
                      GTask            *task,
                      gpointer          data,
                      GDestroyNotify    data_free);

G_GNUC_INTERNAL
void orm_worker_invoke_sync (OrmWorker         *self,
                             OrmWorkerSyncFunc  func,
                             gpointer           data);

G_END_DECLS

#endif /* ORM_WORKER_H */
