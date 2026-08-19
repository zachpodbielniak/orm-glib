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
#include "../core/orm-enums.h"
#include "../core/orm-value.h"

G_BEGIN_DECLS

#define ORM_TYPE_CONNECTION (orm_connection_get_type ())

G_DECLARE_FINAL_TYPE (OrmConnection, orm_connection, ORM, CONNECTION, GObject)

/* Forward declarations */
typedef struct _OrmEngine OrmEngine;
typedef struct _OrmResult OrmResult;
typedef struct _OrmTransaction OrmTransaction;

/*
 * OrmConnection:
 *
 * Represents a connection to a database. Wraps the underlying
 * database connection (SQLite, PostgreSQL, MySQL).
 *
 * Connections should be obtained from an OrmEngine and closed
 * when no longer needed.
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

G_END_DECLS

#endif /* ORM_CONNECTION_H */
