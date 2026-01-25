/* orm-row.h
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

#ifndef ORM_ROW_H
#define ORM_ROW_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-value.h"

G_BEGIN_DECLS

#define ORM_TYPE_ROW (orm_row_get_type ())

G_DECLARE_FINAL_TYPE (OrmRow, orm_row, ORM, ROW, GObject)

/*
 * OrmRow:
 *
 * Represents a single row from a query result.
 * Provides access to column values by index or name.
 */

/*
 * orm_row_get_column_count:
 * @self: An #OrmRow
 *
 * Gets the number of columns in the row.
 *
 * Returns: Number of columns
 */
gint orm_row_get_column_count (OrmRow *self);

/*
 * orm_row_get_column_name:
 * @self: An #OrmRow
 * @index: Column index (0-based)
 *
 * Gets the name of a column by index.
 *
 * Returns: (transfer none) (nullable): Column name, or %NULL if invalid index
 */
const gchar * orm_row_get_column_name (OrmRow *self,
                                       gint    index);

/*
 * orm_row_get_column_index:
 * @self: An #OrmRow
 * @name: Column name
 *
 * Gets the index of a column by name.
 *
 * Returns: Column index, or -1 if not found
 */
gint orm_row_get_column_index (OrmRow      *self,
                               const gchar *name);

/*
 * orm_row_get_value:
 * @self: An #OrmRow
 * @index: Column index
 *
 * Gets a column value by index.
 *
 * Returns: (transfer none) (nullable): The value, or %NULL if invalid index
 */
OrmValue * orm_row_get_value (OrmRow *self,
                              gint    index);

/*
 * orm_row_get_value_by_name:
 * @self: An #OrmRow
 * @name: Column name
 *
 * Gets a column value by name.
 *
 * Returns: (transfer none) (nullable): The value, or %NULL if not found
 */
OrmValue * orm_row_get_value_by_name (OrmRow      *self,
                                      const gchar *name);

/*
 * Convenience getters for common types.
 */

/*
 * orm_row_get_integer:
 * @self: An #OrmRow
 * @index: Column index
 *
 * Gets a column value as integer.
 *
 * Returns: The integer value, or 0 if NULL or invalid
 */
gint64 orm_row_get_integer (OrmRow *self,
                            gint    index);

/*
 * orm_row_get_float:
 * @self: An #OrmRow
 * @index: Column index
 *
 * Gets a column value as float.
 *
 * Returns: The float value, or 0.0 if NULL or invalid
 */
gdouble orm_row_get_float (OrmRow *self,
                           gint    index);

/*
 * orm_row_get_string:
 * @self: An #OrmRow
 * @index: Column index
 *
 * Gets a column value as string.
 *
 * Returns: (transfer none) (nullable): The string value, or %NULL
 */
const gchar * orm_row_get_string (OrmRow *self,
                                  gint    index);

/*
 * orm_row_get_boolean:
 * @self: An #OrmRow
 * @index: Column index
 *
 * Gets a column value as boolean.
 *
 * Returns: The boolean value, or FALSE if NULL or invalid
 */
gboolean orm_row_get_boolean (OrmRow *self,
                              gint    index);

/*
 * orm_row_is_null:
 * @self: An #OrmRow
 * @index: Column index
 *
 * Checks if a column value is NULL.
 *
 * Returns: %TRUE if NULL
 */
gboolean orm_row_is_null (OrmRow *self,
                          gint    index);

/*
 * orm_row_new:
 * @column_names: (element-type utf8): Column names
 * @values: (element-type OrmValue): Column values
 *
 * Creates a new row. Internal use.
 *
 * Returns: (transfer full): A new #OrmRow
 */
OrmRow * orm_row_new (GPtrArray *column_names,
                      GPtrArray *values);

G_END_DECLS

#endif /* ORM_ROW_H */
