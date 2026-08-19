/* orm-connection.h
 *
 * Copyright 2025 Zach Pobiel
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

#ifndef ORM_CONNECTION_H
#define ORM_CONNECTION_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include "../core/orm-enums.h"
#include "../core/orm-value.h"

G_BEGIN_DECLS

#define ORM_TYPE_CONNECTION (orm_connection_get_type ())

G_DECLARE_FINAL_TYPE (OrmConnection, orm_connection, ORM, CONNECTION, GObject)

/* Forward declarations */
typedef struct _OrmEngine OrmEngine;
typedef struct _OrmResult OrmResult;
typedef struct _OrmRowStream OrmRowStream;
typedef struct _OrmTransaction OrmTransaction;

/*
 * OrmConnection:
 *
 * Represents a connection to a database. Wraps the underlying
 * database connection (SQLite, PostgreSQL, MySQL).
 *
 * Connections should be obtained from an OrmEngine and closed
 * when no longer needed.
 *
 * # Threading
 *
 * A connection belongs to whichever thread created it, and none of the
 * backends allow two statements at once on one handle.  The
 * asynchronous API does not change that -- it moves the work onto a
 * single worker thread the connection owns, so operations still run one
 * at a time and still in the order they were started.
 *
 * That worker appears on the first _async call and lives until the
 * connection is closed.  Once it exists, the synchronous calls run on it
 * too, waiting their turn behind whatever is queued rather than reaching
 * into the backend from the caller's thread.  Mixing the two is
 * therefore safe; a synchronous call simply blocks for as long as the
 * queue ahead of it takes.
 *
 * # Signals
 *
 * "state-changed" and "notice" are always emitted in the thread-default
 * #GMainContext of the thread that created the connection, never on the
 * worker, so a handler may touch a user interface directly.
 */

/*
 * orm_connection_new:
 * @engine: The engine that owns this connection
 * @error: Return location for error
 *
 * Creates a new connection. This is typically called by OrmEngine.
 *
 * Returns: (transfer full) (nullable): A new #OrmConnection, or %NULL on error
 */
OrmConnection * orm_connection_new (OrmEngine  *engine,
                                    GError    **error);

/*
 * orm_connection_close:
 * @self: An #OrmConnection
 *
 * Closes the connection. After calling this, the connection
 * cannot be used.
 */
void orm_connection_close (OrmConnection *self);

/*
 * orm_connection_is_open:
 * @self: An #OrmConnection
 *
 * Checks if the connection is open.
 *
 * Returns: %TRUE if open
 */
gboolean orm_connection_is_open (OrmConnection *self);

/*
 * orm_connection_execute:
 * @self: An #OrmConnection
 * @sql: SQL statement to execute
 * @error: Return location for error
 *
 * Executes a SQL statement that doesn't return results.
 *
 * Returns: %TRUE on success
 */
gboolean orm_connection_execute (OrmConnection  *self,
                                 const gchar    *sql,
                                 GError        **error);

/*
 * orm_connection_execute_with_params:
 * @self: An #OrmConnection
 * @sql: SQL statement with ? placeholders
 * @params: (element-type OrmValue): Parameter values
 * @error: Return location for error
 *
 * Executes a parameterized SQL statement.
 *
 * Returns: %TRUE on success
 */
gboolean orm_connection_execute_with_params (OrmConnection  *self,
                                             const gchar    *sql,
                                             GList          *params,
                                             GError        **error);

/*
 * orm_connection_query:
 * @self: An #OrmConnection
 * @sql: SQL query
 * @error: Return location for error
 *
 * Executes a SQL query and returns results.
 *
 * Returns: (transfer full) (nullable): Query results, or %NULL on error
 */
OrmResult * orm_connection_query (OrmConnection  *self,
                                  const gchar    *sql,
                                  GError        **error);

/*
 * orm_connection_query_with_params:
 * @self: An #OrmConnection
 * @sql: SQL query with ? placeholders
 * @params: (element-type OrmValue): Parameter values
 * @error: Return location for error
 *
 * Executes a parameterized SQL query.
 *
 * Returns: (transfer full) (nullable): Query results, or %NULL on error
 */
OrmResult * orm_connection_query_with_params (OrmConnection  *self,
                                              const gchar    *sql,
                                              GList          *params,
                                              GError        **error);

/*
 * orm_connection_begin_transaction:
 * @self: An #OrmConnection
 * @error: Return location for error
 *
 * Begins a new transaction.
 *
 * Returns: (transfer full) (nullable): A new #OrmTransaction, or %NULL on error
 */
OrmTransaction * orm_connection_begin_transaction (OrmConnection  *self,
                                                   GError        **error);

/*
 * orm_connection_begin_transaction_with_isolation:
 * @self: An #OrmConnection
 * @level: The isolation level for this transaction only
 * @error: Return location for error
 *
 * Begins a transaction at @level, leaving the session default untouched.
 *
 * Returns: (transfer full) (nullable): A new #OrmTransaction, or %NULL on error
 */
OrmTransaction * orm_connection_begin_transaction_with_isolation (OrmConnection      *self,
                                                                  OrmIsolationLevel   level,
                                                                  GError            **error);

/*
 * orm_connection_set_isolation_level:
 * @self: An #OrmConnection
 * @level: The isolation level to apply
 * @error: Return location for error
 *
 * Sets the session-wide transaction isolation level.  SQLite supports only
 * %ORM_ISOLATION_SERIALIZABLE and %ORM_ISOLATION_READ_UNCOMMITTED.
 *
 * Returns: %TRUE on success
 */
gboolean orm_connection_set_isolation_level (OrmConnection      *self,
                                             OrmIsolationLevel   level,
                                             GError            **error);

/*
 * orm_connection_get_isolation_level:
 * @self: An #OrmConnection
 *
 * Gets the isolation level this connection is known to be using.
 *
 * Returns: The current #OrmIsolationLevel
 */
OrmIsolationLevel orm_connection_get_isolation_level (OrmConnection *self);

/*
 * orm_connection_in_transaction:
 * @self: An #OrmConnection
 *
 * Checks if a transaction is active.
 *
 * Returns: %TRUE if in a transaction
 */
gboolean orm_connection_in_transaction (OrmConnection *self);

/*
 * orm_connection_get_last_insert_rowid:
 * @self: An #OrmConnection
 *
 * Gets the rowid of the last inserted row.
 *
 * Returns: The last insert rowid
 */
gint64 orm_connection_get_last_insert_rowid (OrmConnection *self);

/*
 * orm_connection_get_changes:
 * @self: An #OrmConnection
 *
 * Gets the number of rows changed by the last statement.
 *
 * Returns: Number of changed rows
 */
gint orm_connection_get_changes (OrmConnection *self);

/*
 * orm_connection_get_engine:
 * @self: An #OrmConnection
 *
 * Gets the engine that created this connection.
 *
 * Returns: (transfer none): The engine
 */
OrmEngine * orm_connection_get_engine (OrmConnection *self);

/*
 * orm_connection_get_state:
 * @self: An #OrmConnection
 *
 * Gets the connection's current state, the same value its
 * "state-changed" signal last reported.
 *
 * Returns: The current #OrmConnectionState
 */
OrmConnectionState orm_connection_get_state (OrmConnection *self);

/*
 * Asynchronous API.
 *
 * Cancelling is best-effort and backend-dependent, because "stop what
 * you are doing" is not something every client library offers:
 *
 * - SQLite and PostgreSQL can be interrupted, so the statement really
 *   does stop early and the connection is usable immediately after.
 * - MySQL cannot: its client library has no interrupt that is safe to
 *   call from another thread while a query is running.  The task still
 *   fails with %G_IO_ERROR_CANCELLED straight away, but the statement
 *   runs to completion in the background and its result is discarded, so
 *   the connection stays busy until the server is finished with it.
 *
 * Either way the connection remains open and usable after a cancelled
 * operation.
 */

/*
 * orm_connection_close_async:
 * @self: An #OrmConnection
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the connection is closed
 * @user_data: (closure): Data for @callback
 *
 * Closes the connection once everything already queued on it has run.
 */
void orm_connection_close_async (OrmConnection       *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data);

/*
 * orm_connection_close_finish:
 * @self: An #OrmConnection
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_connection_close_async().
 *
 * Returns: %TRUE on success
 */
gboolean orm_connection_close_finish (OrmConnection  *self,
                                      GAsyncResult   *result,
                                      GError        **error);

/*
 * orm_connection_execute_async:
 * @self: An #OrmConnection
 * @sql: SQL statement with placeholders
 * @params: (element-type OrmValue) (nullable) (transfer none): Parameter
 *   values, copied before the call returns
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the statement has run
 * @user_data: (closure): Data for @callback
 *
 * Executes a statement that returns no rows.
 */
void orm_connection_execute_async (OrmConnection       *self,
                                   const gchar         *sql,
                                   GList               *params,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data);

/*
 * orm_connection_execute_finish:
 * @self: An #OrmConnection
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_connection_execute_async().
 *
 * Returns: %TRUE on success
 */
gboolean orm_connection_execute_finish (OrmConnection  *self,
                                        GAsyncResult   *result,
                                        GError        **error);

/*
 * orm_connection_query_async:
 * @self: An #OrmConnection
 * @sql: SQL query with placeholders
 * @params: (element-type OrmValue) (nullable) (transfer none): Parameter
 *   values, copied before the call returns
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the result set is ready
 * @user_data: (closure): Data for @callback
 *
 * Runs a query and hands back an #OrmResult.
 *
 * What has happened by the time the callback runs is up to the backend:
 * PostgreSQL and MySQL have every row in memory, while SQLite has only
 * prepared the statement and steps it as the result is read.  Reading an
 * #OrmResult is synchronous either way, so on SQLite the row-by-row work
 * still blocks whoever calls orm_result_next().  Use
 * orm_connection_query_stream_async() to keep that off the calling
 * thread as well.
 */
void orm_connection_query_async (OrmConnection       *self,
                                 const gchar         *sql,
                                 GList               *params,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data);

/*
 * orm_connection_query_finish:
 * @self: An #OrmConnection
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_connection_query_async().
 *
 * Returns: (transfer full) (nullable): The result set, or %NULL on error
 */
OrmResult * orm_connection_query_finish (OrmConnection  *self,
                                         GAsyncResult   *result,
                                         GError        **error);

/*
 * orm_connection_query_stream_async:
 * @self: An #OrmConnection
 * @sql: SQL query with placeholders
 * @params: (element-type OrmValue) (nullable) (transfer none): Parameter
 *   values, copied before the call returns
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the stream is ready
 * @user_data: (closure): Data for @callback
 *
 * Runs a query and hands back an #OrmRowStream, which fetches rows a
 * batch at a time on the connection's worker thread.
 *
 * The query is issued with %ORM_QUERY_FLAGS_STREAMING.  Only SQLite acts
 * on that today -- PostgreSQL and MySQL still materialize the result
 * before the stream is created -- so on those two the stream is a way to
 * read rows without blocking, not a way to avoid holding them in memory.
 */
void orm_connection_query_stream_async (OrmConnection       *self,
                                        const gchar         *sql,
                                        GList               *params,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data);

/*
 * orm_connection_query_stream_finish:
 * @self: An #OrmConnection
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_connection_query_stream_async().
 *
 * Returns: (transfer full) (nullable): The row stream, or %NULL on error
 */
OrmRowStream * orm_connection_query_stream_finish (OrmConnection  *self,
                                                   GAsyncResult   *result,
                                                   GError        **error);

G_END_DECLS

#endif /* ORM_CONNECTION_H */
