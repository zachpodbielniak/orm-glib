/* orm-driver-result.h
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

#ifndef ORM_DRIVER_RESULT_H
#define ORM_DRIVER_RESULT_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-enums.h"
#include "../core/orm-value.h"
#include "../engine/orm-row.h"

G_BEGIN_DECLS

#define ORM_TYPE_DRIVER_RESULT (orm_driver_result_get_type ())

G_DECLARE_DERIVABLE_TYPE (OrmDriverResult, orm_driver_result, ORM, DRIVER_RESULT, GObject)

/*
 * OrmDriverResultClass:
 * @get_column_count: Number of columns in the result
 * @get_column_name: Name of column at an index
 * @get_column_value_type: The #OrmValueType values in a column decode to
 * @get_column_type_name: The backend's own name for a column's declared type
 * @fetch_row: Produce the next row, or %NULL at end of results or on error
 * @close: Release the backend cursor
 *
 * A cursor over one query's results, as the backend exposes it.
 *
 * Row production is a pull: @fetch_row returns the next row or %NULL, and
 * a %NULL with @error unset means the result is exhausted rather than
 * broken. That is deliberately a cursor rather than a materialized list,
 * because SQLite genuinely streams and forcing it to materialize would
 * throw that away; a backend that cannot stream simply buffers and hands
 * rows out one at a time, so callers see one shape either way.
 */
struct _OrmDriverResultClass
{
    GObjectClass parent_class;

    gint          (*get_column_count)      (OrmDriverResult *self);
    const gchar * (*get_column_name)       (OrmDriverResult *self,
                                            gint             index);
    OrmValueType  (*get_column_value_type) (OrmDriverResult *self,
                                            gint             index);
    const gchar * (*get_column_type_name)  (OrmDriverResult *self,
                                            gint             index);
    OrmRow *      (*fetch_row)             (OrmDriverResult  *self,
                                            GError          **error);
    void          (*close)                 (OrmDriverResult *self);

    /*< private >*/
    gpointer _reserved[8];
};

/*
 * orm_driver_result_get_column_count:
 * @self: An #OrmDriverResult
 *
 * Returns: The number of columns
 */
gint orm_driver_result_get_column_count (OrmDriverResult *self);

/*
 * orm_driver_result_get_column_name:
 * @self: An #OrmDriverResult
 * @index: Zero-based column index
 *
 * Returns: (transfer none) (nullable): The column name
 */
const gchar * orm_driver_result_get_column_name (OrmDriverResult *self,
                                                 gint             index);

/*
 * orm_driver_result_get_column_value_type:
 * @self: An #OrmDriverResult
 * @index: Zero-based column index
 *
 * Returns: The #OrmValueType for the column, or %ORM_VALUE_NULL when the
 *   backend cannot say
 */
OrmValueType orm_driver_result_get_column_value_type (OrmDriverResult *self,
                                                      gint             index);

/*
 * orm_driver_result_get_column_type_name:
 * @self: An #OrmDriverResult
 * @index: Zero-based column index
 *
 * Returns: (transfer none) (nullable): The backend's declared type name,
 *   or %NULL when it has none to give
 */
const gchar * orm_driver_result_get_column_type_name (OrmDriverResult *self,
                                                      gint             index);

/*
 * orm_driver_result_fetch_row:
 * @self: An #OrmDriverResult
 * @error: Return location for error
 *
 * Fetches the next row.
 *
 * Returns: (transfer full) (nullable): The next #OrmRow, or %NULL at the
 *   end of the results (with @error unset) or on failure (with @error set)
 */
OrmRow * orm_driver_result_fetch_row (OrmDriverResult  *self,
                                      GError          **error);

/*
 * orm_driver_result_close:
 * @self: An #OrmDriverResult
 *
 * Releases the backend cursor.  Idempotent.
 */
void orm_driver_result_close (OrmDriverResult *self);

G_END_DECLS

#endif /* ORM_DRIVER_RESULT_H */
