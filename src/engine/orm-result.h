/* orm-result.h
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

#ifndef ORM_RESULT_H
#define ORM_RESULT_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-row.h"
#include "../core/orm-enums.h"
#include "../core/orm-value.h"

G_BEGIN_DECLS

#define ORM_TYPE_RESULT (orm_result_get_type ())

G_DECLARE_FINAL_TYPE (OrmResult, orm_result, ORM, RESULT, GObject)

/* Forward declarations */
typedef struct _OrmConnection OrmConnection;

/*
 * OrmResult:
 *
 * Represents the result of a SELECT query. Provides iteration
 * over rows and access to column metadata.
 */

/*
 * orm_result_next:
 * @self: An #OrmResult
 *
 * Advances to the next row. Must be called before accessing the first row.
 *
 * Returns: %TRUE if there is another row, %FALSE if end of results
 */
gboolean orm_result_next (OrmResult *self);

/*
 * orm_result_get_row:
 * @self: An #OrmResult
 *
 * Gets the current row. Call orm_result_next() first.
 *
 * Returns: (transfer none) (nullable): The current row, or %NULL
 */
OrmRow * orm_result_get_row (OrmResult *self);

/*
 * orm_result_get_column_count:
 * @self: An #OrmResult
 *
 * Gets the number of columns in the result.
 *
 * Returns: Number of columns
 */
gint orm_result_get_column_count (OrmResult *self);

/*
 * orm_result_get_column_name:
 * @self: An #OrmResult
 * @index: Column index
 *
 * Gets the name of a column.
 *
 * Returns: (transfer none) (nullable): Column name
 */
const gchar * orm_result_get_column_name (OrmResult *self,
                                          gint       index);

/*
 * orm_result_get_all:
 * @self: An #OrmResult
 *
 * Fetches all remaining rows into a list.
 * The result should not be iterated after calling this.
 *
 * Returns: (transfer full) (element-type OrmRow): List of rows
 */
GList * orm_result_get_all (OrmResult *self);

/*
 * orm_result_get_first:
 * @self: An #OrmResult
 *
 * Gets the first row and closes the result.
 *
 * Returns: (transfer full) (nullable): The first row, or %NULL if empty
 */
OrmRow * orm_result_get_first (OrmResult *self);

/*
 * orm_result_get_scalar:
 * @self: An #OrmResult
 *
 * Gets the value of the first column of the first row.
 * Useful for COUNT(*) and similar queries.
 *
 * Returns: (transfer full) (nullable): The scalar value, or %NULL
 */
OrmValue * orm_result_get_scalar (OrmResult *self);

/*
 * orm_result_close:
 * @self: An #OrmResult
 *
 * Closes the result set and releases resources.
 * Called automatically when the object is destroyed.
 */
void orm_result_close (OrmResult *self);

/*
 * Internal: Create result from SQLite query.
 */
#ifdef ORM_ENABLE_SQLITE
OrmResult * orm_result_new_sqlite (OrmConnection  *connection,
                                   const gchar    *sql,
                                   GList          *params,
                                   GError        **error);
#endif

/*
 * Internal: Create result from PostgreSQL query.
 */
#ifdef ORM_ENABLE_POSTGRES
OrmResult * orm_result_new_postgres (OrmConnection  *connection,
                                     const gchar    *sql,
                                     GList          *params,
                                     GError        **error);
#endif

/*
 * Internal: Create result from MySQL query.
 */
#ifdef ORM_ENABLE_MYSQL
OrmResult * orm_result_new_mysql (OrmConnection  *connection,
                                  const gchar    *sql,
                                  GList          *params,
                                  GError        **error);
#endif

G_END_DECLS

#endif /* ORM_RESULT_H */
