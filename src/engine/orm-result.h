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
 * orm_result_get_error:
 * @self: An #OrmResult
 *
 * Gets the error that ended iteration, if one did.  orm_result_next()
 * returns %FALSE for both a genuine end of results and a failed fetch;
 * this is how a caller tells them apart.
 *
 * Returns: (transfer none) (nullable): The error, or %NULL
 */
const GError * orm_result_get_error (OrmResult *self);

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
 * orm_result_get_column_type:
 * @self: An #OrmResult
 * @index: Zero-based column index
 *
 * Gets the value type a column's values decode to, from the column's
 * declared type rather than from any particular row -- so a column of
 * integers still reports %ORM_VALUE_INTEGER on the row where it is NULL.
 *
 * %ORM_VALUE_NULL means the backend could not say, which is the honest
 * answer for a computed column.
 *
 * Returns: The #OrmValueType for the column
 */
OrmValueType orm_result_get_column_type (OrmResult *self,
                                         gint       index);

/*
 * orm_result_get_column_type_name:
 * @self: An #OrmResult
 * @index: Zero-based column index
 *
 * Gets the backend's own name for a column's declared type, such as
 * "VARCHAR(255)" or "BIGINT".  %NULL when the backend has none to give,
 * which happens for computed columns.
 *
 * Returns: (transfer none) (nullable): The type name, or %NULL
 */
const gchar * orm_result_get_column_type_name (OrmResult *self,
                                               gint       index);

/*
 * orm_result_get_column_name:
 * @self: An #OrmResult
 * @index: Zero-based column index
 *
 * Returns: (transfer none) (nullable): The column name
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
#endif

/*
 * Internal: Create result from PostgreSQL query.
 */
#ifdef ORM_ENABLE_POSTGRES
#endif

/*
 * Internal: Create result from MySQL query.
 */
#ifdef ORM_ENABLE_MYSQL
#endif

G_END_DECLS

#endif /* ORM_RESULT_H */
