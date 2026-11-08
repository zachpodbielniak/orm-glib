/* orm-connection-pool.h
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

#ifndef ORM_CONNECTION_POOL_H
#define ORM_CONNECTION_POOL_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include "orm-engine.h"
#include "orm-connection.h"
#include "orm-result.h"

G_BEGIN_DECLS

#define ORM_TYPE_CONNECTION_POOL (orm_connection_pool_get_type ())

G_DECLARE_FINAL_TYPE (OrmConnectionPool, orm_connection_pool, ORM, CONNECTION_POOL, GObject)

/* Defaults, also the property defaults */
#define ORM_CONNECTION_POOL_DEFAULT_MAX_SIZE            (10)
#define ORM_CONNECTION_POOL_DEFAULT_IDLE_TIMEOUT        (300)
#define ORM_CONNECTION_POOL_DEFAULT_ACQUIRE_TIMEOUT     (30000)
#define ORM_CONNECTION_POOL_DEFAULT_VALIDATION_INTERVAL (30)

/*
 * OrmConnectionPool:
 *
 * A bounded set of open #OrmConnection objects for one #OrmEngine, lent
 * out one borrower at a time.
 *
 * A connection still runs one statement at a time on its own worker
 * thread; the pool is how an application gets several of them, and so
 * several threads of database work, without opening and closing a
 * connection per request.
 *
 * # Leasing
 *
 * orm_connection_pool_acquire() and orm_connection_pool_acquire_async()
 * lend a connection; orm_connection_pool_release() hands it back.  While
 * lent, the connection belongs to the borrower exclusively.  On release
 * the pool closes any result set or row stream left open on it, rolls
 * back any transaction left open (including one begun with raw SQL) and
 * restores the backend's default isolation level before anyone else sees
 * it.  A lent connection dropped with g_object_unref() instead is
 * noticed: the pool warns and recovers its slot.
 *
 * When every connection is lent and the pool is at #OrmConnectionPool:max-size,
 * acquirers queue in arrival order and are served as connections come
 * back, failing with %G_IO_ERROR_TIMED_OUT after
 * #OrmConnectionPool:acquire-timeout milliseconds.
 *
 * The one-shot helpers -- orm_connection_pool_execute_async(),
 * orm_connection_pool_query_async() and their synchronous forms --
 * acquire, run one statement and release, so independent statements
 * fan out across the pool's connections.  A query's rows are read in full
 * before its result is handed over, so results hold no connection and
 * can be kept for as long as needed.
 *
 * # Main loop integration
 *
 * The pool is bound to a #GMainContext, the thread-default one at
 * construction unless #OrmConnectionPool:main-context says otherwise.
 * That context:
 *
 * - runs the idle reaper, which closes connections unused for
 *   #OrmConnectionPool:idle-timeout seconds, down to
 *   #OrmConnectionPool:min-size;
 * - receives the "state-changed" and "notice" signals of every
 *   connection the pool opens.
 *
 * Asynchronous callbacks, as everywhere in GIO, arrive in the
 * thread-default context of the thread that started the call.
 *
 * All methods are thread-safe.
 */

/*
 * orm_connection_pool_new:
 * @engine: The engine connections are opened from
 * @max_size: Most connections open at once, at least 1
 * @error: Return location for error
 *
 * Creates a pool.  No connection is opened until one is asked for.
 *
 * Fails with %ORM_ERROR_NOT_SUPPORTED for an in-memory SQLite engine
 * with @max_size above 1: every SQLite connection to ":memory:" is a
 * separate, empty database.
 *
 * Returns: (transfer full) (nullable): A new #OrmConnectionPool, or %NULL on error
 */
OrmConnectionPool * orm_connection_pool_new (OrmEngine  *engine,
                                             guint       max_size,
                                             GError    **error);

/*
 * orm_connection_pool_get_engine:
 * @self: An #OrmConnectionPool
 *
 * Returns: (transfer none): The engine connections are opened from
 */
OrmEngine * orm_connection_pool_get_engine (OrmConnectionPool *self);

/*
 * orm_connection_pool_get_main_context:
 * @self: An #OrmConnectionPool
 *
 * Returns: (transfer none): The context the pool's housekeeping runs in
 */
GMainContext * orm_connection_pool_get_main_context (OrmConnectionPool *self);

/*
 * orm_connection_pool_get_max_size:
 * @self: An #OrmConnectionPool
 *
 * Returns: The most connections the pool keeps open at once
 */
guint orm_connection_pool_get_max_size (OrmConnectionPool *self);

/*
 * orm_connection_pool_get_min_size:
 * @self: An #OrmConnectionPool
 *
 * Returns: The number of connections the idle reaper never goes below
 */
guint orm_connection_pool_get_min_size (OrmConnectionPool *self);

/*
 * orm_connection_pool_set_min_size:
 * @self: An #OrmConnectionPool
 * @min_size: Connections the idle reaper leaves open, clamped to max-size
 *
 * Sets the floor for idle reaping.  Does not open connections.
 */
void orm_connection_pool_set_min_size (OrmConnectionPool *self,
                                       guint              min_size);

/*
 * orm_connection_pool_get_idle_timeout:
 * @self: An #OrmConnectionPool
 *
 * Returns: Seconds an idle connection is kept, or 0 for forever
 */
guint orm_connection_pool_get_idle_timeout (OrmConnectionPool *self);

/*
 * orm_connection_pool_set_idle_timeout:
 * @self: An #OrmConnectionPool
 * @seconds: Seconds an idle connection is kept, or 0 for forever
 *
 * Sets how long an unused connection stays open.
 */
void orm_connection_pool_set_idle_timeout (OrmConnectionPool *self,
                                           guint              seconds);

/*
 * orm_connection_pool_get_acquire_timeout:
 * @self: An #OrmConnectionPool
 *
 * Returns: Milliseconds an acquirer waits for a free connection, or 0 for forever
 */
guint orm_connection_pool_get_acquire_timeout (OrmConnectionPool *self);

/*
 * orm_connection_pool_set_acquire_timeout:
 * @self: An #OrmConnectionPool
 * @milliseconds: How long an acquirer waits, or 0 for forever
 *
 * Sets how long acquiring waits when the pool is exhausted.  Applies to
 * acquisitions started afterwards.
 */
void orm_connection_pool_set_acquire_timeout (OrmConnectionPool *self,
                                              guint              milliseconds);

/*
 * orm_connection_pool_get_validation_interval:
 * @self: An #OrmConnectionPool
 *
 * Returns: Seconds a connection may sit idle before it is checked on acquire
 */
guint orm_connection_pool_get_validation_interval (OrmConnectionPool *self);

/*
 * orm_connection_pool_set_validation_interval:
 * @self: An #OrmConnectionPool
 * @seconds: Idle seconds after which a connection is checked before
 *   being lent; 0 checks every time, %G_MAXUINT never
 *
 * Sets when an idle connection is pinged before it is handed out.
 */
void orm_connection_pool_set_validation_interval (OrmConnectionPool *self,
                                                  guint              seconds);

/*
 * orm_connection_pool_dup_init_sql:
 * @self: An #OrmConnectionPool
 *
 * Returns: (transfer full) (nullable): The SQL run on each new connection
 */
gchar * orm_connection_pool_dup_init_sql (OrmConnectionPool *self);

/*
 * orm_connection_pool_set_init_sql:
 * @self: An #OrmConnectionPool
 * @sql: (nullable): SQL run once on every connection the pool opens
 *
 * Sets per-connection setup, such as "PRAGMA busy_timeout = 5000" or
 * "SET search_path TO app".  Connections already open are not touched.
 */
void orm_connection_pool_set_init_sql (OrmConnectionPool *self,
                                       const gchar       *sql);

/*
 * orm_connection_pool_get_size:
 * @self: An #OrmConnectionPool
 *
 * Returns: Connections open or opening, idle and lent alike
 */
guint orm_connection_pool_get_size (OrmConnectionPool *self);

/*
 * orm_connection_pool_get_idle_count:
 * @self: An #OrmConnectionPool
 *
 * Returns: Connections open and waiting to be lent
 */
guint orm_connection_pool_get_idle_count (OrmConnectionPool *self);

/*
 * orm_connection_pool_get_waiting_count:
 * @self: An #OrmConnectionPool
 *
 * Returns: Acquirers queued for a connection
 */
guint orm_connection_pool_get_waiting_count (OrmConnectionPool *self);

/*
 * orm_connection_pool_is_closed:
 * @self: An #OrmConnectionPool
 *
 * Returns: %TRUE once orm_connection_pool_close() has been called
 */
gboolean orm_connection_pool_is_closed (OrmConnectionPool *self);

/*
 * orm_connection_pool_acquire:
 * @self: An #OrmConnectionPool
 * @cancellable: (nullable): A #GCancellable
 * @error: Return location for error
 *
 * Borrows a connection, blocking while the pool is exhausted.
 *
 * Returns: (transfer full) (nullable): A connection to hand back with
 *   orm_connection_pool_release(), or %NULL on error
 */
OrmConnection * orm_connection_pool_acquire (OrmConnectionPool  *self,
                                             GCancellable       *cancellable,
                                             GError            **error);

/*
 * orm_connection_pool_acquire_async:
 * @self: An #OrmConnectionPool
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when a connection is available
 * @user_data: (closure): Data for @callback
 *
 * Borrows a connection without blocking.  Opening or validating one
 * happens on a GIO worker thread.
 */
void orm_connection_pool_acquire_async (OrmConnectionPool   *self,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data);

/*
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
OrmConnection * orm_connection_pool_acquire_finish (OrmConnectionPool  *self,
                                                    GAsyncResult       *result,
                                                    GError            **error);

/*
 * orm_connection_pool_release:
 * @self: An #OrmConnectionPool
 * @connection: (transfer full): A connection acquired from @self
 *
 * Hands a connection back.  Callable from any thread.
 */
void orm_connection_pool_release (OrmConnectionPool *self,
                                  OrmConnection     *connection);

/*
 * orm_connection_pool_close:
 * @self: An #OrmConnectionPool
 *
 * Closes idle connections, fails queued acquirers with
 * %ORM_ERROR_CONNECTION_CLOSED, and closes lent connections as they come
 * back.  Safe to call more than once.
 */
void orm_connection_pool_close (OrmConnectionPool *self);

/*
 * orm_connection_pool_execute:
 * @self: An #OrmConnectionPool
 * @sql: SQL statement with placeholders
 * @params: (element-type OrmValue) (nullable): Parameter values
 * @error: Return location for error
 *
 * Runs one statement that returns no rows on a pooled connection.
 *
 * Returns: %TRUE on success
 */
gboolean orm_connection_pool_execute (OrmConnectionPool  *self,
                                      const gchar        *sql,
                                      GList              *params,
                                      GError            **error);

/*
 * orm_connection_pool_query:
 * @self: An #OrmConnectionPool
 * @sql: SQL query with placeholders
 * @params: (element-type OrmValue) (nullable): Parameter values
 * @error: Return location for error
 *
 * Runs one query on a pooled connection and reads every row before
 * releasing it, so the result holds no connection and can be kept.
 *
 * Returns: (transfer full) (nullable): The result set, or %NULL on error
 */
OrmResult * orm_connection_pool_query (OrmConnectionPool  *self,
                                       const gchar        *sql,
                                       GList              *params,
                                       GError            **error);

/*
 * orm_connection_pool_execute_async:
 * @self: An #OrmConnectionPool
 * @sql: SQL statement with placeholders
 * @params: (element-type OrmValue) (nullable) (transfer none): Parameter
 *   values, copied before the call returns
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the statement has run
 * @user_data: (closure): Data for @callback
 *
 * Acquires, runs one statement that returns no rows, and releases.
 */
void orm_connection_pool_execute_async (OrmConnectionPool   *self,
                                        const gchar         *sql,
                                        GList               *params,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data);

/*
 * orm_connection_pool_execute_finish:
 * @self: An #OrmConnectionPool
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_connection_pool_execute_async().
 *
 * Returns: %TRUE on success
 */
gboolean orm_connection_pool_execute_finish (OrmConnectionPool  *self,
                                             GAsyncResult       *result,
                                             GError            **error);

/*
 * orm_connection_pool_query_async:
 * @self: An #OrmConnectionPool
 * @sql: SQL query with placeholders
 * @params: (element-type OrmValue) (nullable) (transfer none): Parameter
 *   values, copied before the call returns
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the result set is ready
 * @user_data: (closure): Data for @callback
 *
 * Acquires and runs one query, reading every row on the connection's
 * worker thread and releasing the connection before @callback runs.  The
 * #OrmResult holds no connection and can be kept.
 */
void orm_connection_pool_query_async (OrmConnectionPool   *self,
                                      const gchar         *sql,
                                      GList               *params,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data);

/*
 * orm_connection_pool_query_finish:
 * @self: An #OrmConnectionPool
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_connection_pool_query_async().
 *
 * Returns: (transfer full) (nullable): The result set, or %NULL on error
 */
OrmResult * orm_connection_pool_query_finish (OrmConnectionPool  *self,
                                              GAsyncResult       *result,
                                              GError            **error);

G_END_DECLS

#endif /* ORM_CONNECTION_POOL_H */
