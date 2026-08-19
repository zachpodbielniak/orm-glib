/* orm-driver-result.c
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

#include "orm-driver-result.h"
#include "../core/orm-error.h"

/*
 * OrmDriverResult - abstract cursor over one query's results.
 *
 * Every method here is a thin forward to the subclass.  The base class
 * holds no state of its own: what a result *is* differs completely
 * between a SQLite statement being stepped, a fully materialized libpq
 * result, and a MySQL result set, and pretending otherwise would mean a
 * union in the base and a switch in every method -- which is the shape
 * this abstraction exists to remove.
 */

typedef struct
{
    gpointer padding;
} OrmDriverResultPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (OrmDriverResult, orm_driver_result, G_TYPE_OBJECT)

/*
 * Default: the backend cannot say what type a column holds.  WP3 fills
 * this in per backend; until then callers get a truthful "no idea"
 * rather than a confident wrong answer.
 */
static OrmValueType
orm_driver_result_real_get_column_value_type (OrmDriverResult *self,
                                              gint             index)
{
    return ORM_VALUE_NULL;
}

static const gchar *
orm_driver_result_real_get_column_type_name (OrmDriverResult *self,
                                             gint             index)
{
    return NULL;
}

static void
orm_driver_result_real_close (OrmDriverResult *self)
{
}

static void
orm_driver_result_class_init (OrmDriverResultClass *klass)
{
    klass->get_column_value_type = orm_driver_result_real_get_column_value_type;
    klass->get_column_type_name = orm_driver_result_real_get_column_type_name;
    klass->close = orm_driver_result_real_close;
}

static void
orm_driver_result_init (OrmDriverResult *self)
{
}

/**
 * orm_driver_result_get_column_count:
 * @self: An #OrmDriverResult
 *
 * Gets the number of columns in the result.
 *
 * Returns: The column count, or 0
 */
gint
orm_driver_result_get_column_count (OrmDriverResult *self)
{
    OrmDriverResultClass *klass;

    g_return_val_if_fail (ORM_IS_DRIVER_RESULT (self), 0);

    klass = ORM_DRIVER_RESULT_GET_CLASS (self);
    g_return_val_if_fail (klass->get_column_count != NULL, 0);

    return klass->get_column_count (self);
}

/**
 * orm_driver_result_get_column_name:
 * @self: An #OrmDriverResult
 * @index: Zero-based column index
 *
 * Gets the name of a column.
 *
 * Returns: (transfer none) (nullable): The column name
 */
const gchar *
orm_driver_result_get_column_name (OrmDriverResult *self,
                                   gint             index)
{
    OrmDriverResultClass *klass;

    g_return_val_if_fail (ORM_IS_DRIVER_RESULT (self), NULL);

    klass = ORM_DRIVER_RESULT_GET_CLASS (self);
    g_return_val_if_fail (klass->get_column_name != NULL, NULL);

    return klass->get_column_name (self, index);
}

/**
 * orm_driver_result_get_column_value_type:
 * @self: An #OrmDriverResult
 * @index: Zero-based column index
 *
 * Gets the value type a column's values decode to.
 *
 * Returns: The #OrmValueType, or %ORM_VALUE_NULL when unknown
 */
OrmValueType
orm_driver_result_get_column_value_type (OrmDriverResult *self,
                                         gint             index)
{
    g_return_val_if_fail (ORM_IS_DRIVER_RESULT (self), ORM_VALUE_NULL);

    return ORM_DRIVER_RESULT_GET_CLASS (self)->get_column_value_type (self, index);
}

/**
 * orm_driver_result_get_column_type_name:
 * @self: An #OrmDriverResult
 * @index: Zero-based column index
 *
 * Gets the backend's own name for a column's declared type.
 *
 * Returns: (transfer none) (nullable): The type name, or %NULL
 */
const gchar *
orm_driver_result_get_column_type_name (OrmDriverResult *self,
                                        gint             index)
{
    g_return_val_if_fail (ORM_IS_DRIVER_RESULT (self), NULL);

    return ORM_DRIVER_RESULT_GET_CLASS (self)->get_column_type_name (self, index);
}

/**
 * orm_driver_result_fetch_row:
 * @self: An #OrmDriverResult
 * @error: Return location for error
 *
 * Fetches the next row from the cursor.
 *
 * A %NULL return with @error unset means the results are exhausted; a
 * %NULL with @error set means the fetch failed.  Callers must check
 * @error rather than treating every %NULL as end-of-results.
 *
 * Returns: (transfer full) (nullable): The next #OrmRow, or %NULL
 */
OrmRow *
orm_driver_result_fetch_row (OrmDriverResult  *self,
                             GError          **error)
{
    OrmDriverResultClass *klass;

    g_return_val_if_fail (ORM_IS_DRIVER_RESULT (self), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    klass = ORM_DRIVER_RESULT_GET_CLASS (self);
    g_return_val_if_fail (klass->fetch_row != NULL, NULL);

    return klass->fetch_row (self, error);
}

/**
 * orm_driver_result_close:
 * @self: An #OrmDriverResult
 *
 * Releases the backend cursor.  Safe to call more than once.
 */
void
orm_driver_result_close (OrmDriverResult *self)
{
    g_return_if_fail (ORM_IS_DRIVER_RESULT (self));

    ORM_DRIVER_RESULT_GET_CLASS (self)->close (self);
}
