/* orm-inspector.h
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

#ifndef ORM_INSPECTOR_H
#define ORM_INSPECTOR_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include "../engine/orm-connection.h"
#include "orm-schema-info.h"

G_BEGIN_DECLS

#define ORM_TYPE_INSPECTOR (orm_inspector_get_type ())

G_DECLARE_DERIVABLE_TYPE (OrmInspector, orm_inspector, ORM, INSPECTOR, GObject)

/*
 * OrmInspectorClass:
 * @list_schemas: Namespaces in the database
 * @list_relations: Tables and views in a schema
 * @get_columns: Columns of one relation
 * @get_indexes: Indexes on one relation
 * @get_foreign_keys: Foreign keys declared on one relation
 * @get_primary_key: Primary-key columns of one relation, in key order
 * @estimate_row_count: How many rows a relation holds
 *
 * Reads the shape of a database that already exists.
 *
 * Introspection is per-backend because there is no portable way to ask:
 * SQLite answers through `sqlite_master` and PRAGMAs, PostgreSQL through
 * `pg_catalog`, MySQL through `information_schema`. It is a separate
 * class from #OrmDialect for the same reason it is not a fourth dialect
 * interface -- a dialect is a stateless SQL-string generator, while this
 * owns a connection and runs a sequence of queries whose results feed
 * each other.
 *
 * A driver supplies its inspector, so an out-of-tree backend brings its
 * own with no changes here.
 *
 * @schema is %NULL for "the default one", which is the only meaningful
 * answer on SQLite and means `current_schema()` on PostgreSQL.
 */
struct _OrmInspectorClass
{
    GObjectClass parent_class;

    gchar **   (*list_schemas)       (OrmInspector  *self,
                                      GError       **error);
    GPtrArray *(*list_relations)     (OrmInspector  *self,
                                      const gchar   *schema,
                                      GError       **error);
    GPtrArray *(*get_columns)        (OrmInspector  *self,
                                      const gchar   *table,
                                      const gchar   *schema,
                                      GError       **error);
    GPtrArray *(*get_indexes)        (OrmInspector  *self,
                                      const gchar   *table,
                                      const gchar   *schema,
                                      GError       **error);
    GPtrArray *(*get_foreign_keys)   (OrmInspector  *self,
                                      const gchar   *table,
                                      const gchar   *schema,
                                      GError       **error);
    gchar **   (*get_primary_key)    (OrmInspector  *self,
                                      const gchar   *table,
                                      const gchar   *schema,
                                      GError       **error);
    gint64     (*estimate_row_count) (OrmInspector  *self,
                                      const gchar   *table,
                                      const gchar   *schema,
                                      gboolean      *is_estimate,
                                      GError       **error);

    /*< private >*/
    gpointer _reserved[8];
};

/*
 * orm_inspector_new:
 * @connection: An open #OrmConnection
 * @error: Return location for error
 *
 * Creates an inspector for @connection's backend.
 *
 * Returns: (transfer full) (nullable): A new #OrmInspector, or %NULL if
 *   the backend has no inspector
 */
OrmInspector * orm_inspector_new (OrmConnection  *connection,
                                  GError        **error);

/*
 * orm_inspector_get_connection:
 * @self: An #OrmInspector
 *
 * Returns: (transfer none): The connection being inspected
 */
OrmConnection * orm_inspector_get_connection (OrmInspector *self);

/*
 * orm_inspector_list_schemas:
 * @self: An #OrmInspector
 * @error: Return location for error
 *
 * Lists the schemas (namespaces) in the database.
 *
 * Returns: (transfer full) (array zero-terminated=1): The schema names
 */
gchar ** orm_inspector_list_schemas (OrmInspector  *self,
                                     GError       **error);

/*
 * orm_inspector_list_relations:
 * @self: An #OrmInspector
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Lists the tables and views in @schema.
 *
 * Returns: (transfer full) (element-type OrmTableInfo): The relations
 */
GPtrArray * orm_inspector_list_relations (OrmInspector  *self,
                                          const gchar   *schema,
                                          GError       **error);

/*
 * orm_inspector_list_tables:
 * @self: An #OrmInspector
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Lists table names only, skipping views.
 *
 * Returns: (transfer full) (array zero-terminated=1): The table names
 */
gchar ** orm_inspector_list_tables (OrmInspector  *self,
                                    const gchar   *schema,
                                    GError       **error);

/*
 * orm_inspector_list_views:
 * @self: An #OrmInspector
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Lists view names only.
 *
 * Returns: (transfer full) (array zero-terminated=1): The view names
 */
gchar ** orm_inspector_list_views (OrmInspector  *self,
                                   const gchar   *schema,
                                   GError       **error);

/*
 * orm_inspector_has_table:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Returns: %TRUE if @table exists
 */
gboolean orm_inspector_has_table (OrmInspector  *self,
                                  const gchar   *table,
                                  const gchar   *schema,
                                  GError       **error);

/*
 * orm_inspector_get_columns:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Gets the columns of @table, in table order.
 *
 * Returns: (transfer full) (element-type OrmColumnInfo): The columns
 */
GPtrArray * orm_inspector_get_columns (OrmInspector  *self,
                                       const gchar   *table,
                                       const gchar   *schema,
                                       GError       **error);

/*
 * orm_inspector_get_indexes:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Returns: (transfer full) (element-type OrmIndexInfo): The indexes
 */
GPtrArray * orm_inspector_get_indexes (OrmInspector  *self,
                                       const gchar   *table,
                                       const gchar   *schema,
                                       GError       **error);

/*
 * orm_inspector_get_foreign_keys:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Returns: (transfer full) (element-type OrmForeignKeyInfo): The foreign keys
 */
GPtrArray * orm_inspector_get_foreign_keys (OrmInspector  *self,
                                            const gchar   *table,
                                            const gchar   *schema,
                                            GError       **error);

/*
 * orm_inspector_get_primary_key:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Gets the primary-key columns in key order, which is not necessarily
 * table order for a composite key.
 *
 * An empty array means the table has no primary key -- worth checking
 * before offering to edit rows, since without one there is no reliable
 * way to name the row you mean.
 *
 * Returns: (transfer full) (array zero-terminated=1): The column names
 */
gchar ** orm_inspector_get_primary_key (OrmInspector  *self,
                                        const gchar   *table,
                                        const gchar   *schema,
                                        GError       **error);

/*
 * orm_inspector_estimate_row_count:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @is_estimate: (out) (optional): %TRUE if the number is the planner's
 *   estimate rather than an exact count
 * @error: Return location for error
 *
 * Counts the rows in @table, preferring the server's own statistics
 * where it keeps them: an exact COUNT(*) on a large table can take
 * minutes, which is too slow for a table listing.
 *
 * Returns: The row count, or -1 on error
 */
gint64 orm_inspector_estimate_row_count (OrmInspector  *self,
                                         const gchar   *table,
                                         const gchar   *schema,
                                         gboolean      *is_estimate,
                                         GError       **error);

/*
 * Asynchronous introspection.
 *
 * Reading a catalog is several queries whose results feed each other, so
 * it is the slowest read the library does on a large schema -- and it is
 * exactly what a database browser wants to do the moment it connects,
 * while still drawing.  Each of these runs the whole sequence on the
 * connection's worker thread, in turn with everything else queued there.
 */

/*
 * orm_inspector_list_schemas_async:
 * @self: An #OrmInspector
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the schemas have been read
 * @user_data: (closure): Data for @callback
 *
 * Lists the schemas in the database, without blocking.
 */
void orm_inspector_list_schemas_async (OrmInspector        *self,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data);

/*
 * orm_inspector_list_schemas_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_inspector_list_schemas_async().
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): The
 *   schema names, or %NULL on error
 */
gchar ** orm_inspector_list_schemas_finish (OrmInspector  *self,
                                            GAsyncResult  *result,
                                            GError       **error);

/*
 * orm_inspector_list_relations_async:
 * @self: An #OrmInspector
 * @schema: (nullable): The schema, or %NULL for the default
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the relations have been read
 * @user_data: (closure): Data for @callback
 *
 * Lists the tables and views in @schema, without blocking.
 */
void orm_inspector_list_relations_async (OrmInspector        *self,
                                         const gchar         *schema,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data);

/*
 * orm_inspector_list_relations_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_inspector_list_relations_async().
 *
 * Returns: (transfer full) (element-type OrmTableInfo) (nullable): The
 *   relations, or %NULL on error
 */
GPtrArray * orm_inspector_list_relations_finish (OrmInspector  *self,
                                                 GAsyncResult  *result,
                                                 GError       **error);

/*
 * orm_inspector_get_columns_async:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the columns have been read
 * @user_data: (closure): Data for @callback
 *
 * Gets the columns of @table, without blocking.
 */
void orm_inspector_get_columns_async (OrmInspector        *self,
                                      const gchar         *table,
                                      const gchar         *schema,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data);

/*
 * orm_inspector_get_columns_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_inspector_get_columns_async().
 *
 * Returns: (transfer full) (element-type OrmColumnInfo) (nullable): The
 *   columns, or %NULL on error
 */
GPtrArray * orm_inspector_get_columns_finish (OrmInspector  *self,
                                              GAsyncResult  *result,
                                              GError       **error);

/*
 * orm_inspector_get_indexes_async:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the indexes have been read
 * @user_data: (closure): Data for @callback
 *
 * Gets the indexes on @table, without blocking.
 */
void orm_inspector_get_indexes_async (OrmInspector        *self,
                                      const gchar         *table,
                                      const gchar         *schema,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data);

/*
 * orm_inspector_get_indexes_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_inspector_get_indexes_async().
 *
 * Returns: (transfer full) (element-type OrmIndexInfo) (nullable): The
 *   indexes, or %NULL on error
 */
GPtrArray * orm_inspector_get_indexes_finish (OrmInspector  *self,
                                              GAsyncResult  *result,
                                              GError       **error);

/*
 * orm_inspector_get_foreign_keys_async:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the foreign keys have been read
 * @user_data: (closure): Data for @callback
 *
 * Gets the foreign keys declared on @table, without blocking.
 */
void orm_inspector_get_foreign_keys_async (OrmInspector        *self,
                                           const gchar         *table,
                                           const gchar         *schema,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data);

/*
 * orm_inspector_get_foreign_keys_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_inspector_get_foreign_keys_async().
 *
 * Returns: (transfer full) (element-type OrmForeignKeyInfo) (nullable):
 *   The foreign keys, or %NULL on error
 */
GPtrArray * orm_inspector_get_foreign_keys_finish (OrmInspector  *self,
                                                   GAsyncResult  *result,
                                                   GError       **error);

/*
 * orm_inspector_get_primary_key_async:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the key has been read
 * @user_data: (closure): Data for @callback
 *
 * Gets the primary-key columns of @table, without blocking.
 */
void orm_inspector_get_primary_key_async (OrmInspector        *self,
                                          const gchar         *table,
                                          const gchar         *schema,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data);

/*
 * orm_inspector_get_primary_key_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_inspector_get_primary_key_async().
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): The
 *   column names in key order, or %NULL on error
 */
gchar ** orm_inspector_get_primary_key_finish (OrmInspector  *self,
                                               GAsyncResult  *result,
                                               GError       **error);

/*
 * orm_inspector_estimate_row_count_async:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the count is known
 * @user_data: (closure): Data for @callback
 *
 * Counts the rows in @table, without blocking.  This is the one worth
 * having asynchronous even on SQLite, where the count is always exact
 * and therefore always a full scan.
 */
void orm_inspector_estimate_row_count_async (OrmInspector        *self,
                                             const gchar         *table,
                                             const gchar         *schema,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data);

/*
 * orm_inspector_estimate_row_count_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @is_estimate: (out) (optional): %TRUE if the number is the planner's
 *   estimate rather than an exact count
 * @error: Return location for error
 *
 * Finishes orm_inspector_estimate_row_count_async().
 *
 * Returns: The row count, or -1 on error
 */
gint64 orm_inspector_estimate_row_count_finish (OrmInspector  *self,
                                                GAsyncResult  *result,
                                                gboolean      *is_estimate,
                                                GError       **error);

G_END_DECLS

#endif /* ORM_INSPECTOR_H */
