/* orm-row.c
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

#include "orm-row.h"

#include <string.h>

/*
 * OrmRow - A single result row.
 *
 * Stores column names and values for a row from a query result.
 */

struct _OrmRow
{
    GObject parent_instance;

    GPtrArray  *column_names;   /* (element-type utf8) */
    GPtrArray  *values;         /* (element-type OrmValue) */
    GHashTable *name_to_index;  /* Column name -> index lookup */
};

G_DEFINE_TYPE (OrmRow, orm_row, G_TYPE_OBJECT)

static void
orm_row_finalize (GObject *object)
{
    OrmRow *self = ORM_ROW (object);

    g_clear_pointer (&self->column_names, g_ptr_array_unref);
    g_clear_pointer (&self->values, g_ptr_array_unref);
    g_clear_pointer (&self->name_to_index, g_hash_table_unref);

    G_OBJECT_CLASS (orm_row_parent_class)->finalize (object);
}

static void
orm_row_class_init (OrmRowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_row_finalize;
}

static void
orm_row_init (OrmRow *self)
{
    self->column_names = NULL;
    self->values = NULL;
    self->name_to_index = NULL;
}

/**
 * orm_row_new:
 * @column_names: (element-type utf8) (transfer full): Column names
 * @values: (element-type OrmValue) (transfer full): Column values
 *
 * Creates a new row from column names and values.
 * Takes ownership of the arrays.
 *
 * Returns: (transfer full): A new #OrmRow
 */
OrmRow *
orm_row_new (GPtrArray *column_names,
             GPtrArray *values)
{
    OrmRow *self;
    guint i;

    g_return_val_if_fail (column_names != NULL, NULL);
    g_return_val_if_fail (values != NULL, NULL);
    g_return_val_if_fail (column_names->len == values->len, NULL);

    self = g_object_new (ORM_TYPE_ROW, NULL);
    self->column_names = column_names;
    self->values = values;

    /* Build name -> index lookup table */
    self->name_to_index = g_hash_table_new (g_str_hash, g_str_equal);
    for (i = 0; i < column_names->len; i++)
    {
        const gchar *name = g_ptr_array_index (column_names, i);
        g_hash_table_insert (self->name_to_index,
                             (gpointer) name,
                             GINT_TO_POINTER (i));
    }

    return self;
}

/**
 * orm_row_get_column_count:
 * @self: An #OrmRow
 *
 * Gets the number of columns in the row.
 *
 * Returns: Number of columns
 */
gint
orm_row_get_column_count (OrmRow *self)
{
    g_return_val_if_fail (ORM_IS_ROW (self), 0);
    return (gint) self->values->len;
}

/**
 * orm_row_get_column_name:
 * @self: An #OrmRow
 * @index: Column index (0-based)
 *
 * Gets the name of a column by index.
 *
 * Returns: (transfer none) (nullable): Column name
 */
const gchar *
orm_row_get_column_name (OrmRow *self,
                         gint    index)
{
    g_return_val_if_fail (ORM_IS_ROW (self), NULL);
    g_return_val_if_fail (index >= 0 && (guint) index < self->column_names->len, NULL);

    return g_ptr_array_index (self->column_names, index);
}

/**
 * orm_row_get_column_index:
 * @self: An #OrmRow
 * @name: Column name
 *
 * Gets the index of a column by name.
 *
 * Returns: Column index, or -1 if not found
 */
gint
orm_row_get_column_index (OrmRow      *self,
                          const gchar *name)
{
    gpointer value;

    g_return_val_if_fail (ORM_IS_ROW (self), -1);
    g_return_val_if_fail (name != NULL, -1);

    if (g_hash_table_lookup_extended (self->name_to_index, name, NULL, &value))
    {
        return GPOINTER_TO_INT (value);
    }

    return -1;
}

/**
 * orm_row_get_value:
 * @self: An #OrmRow
 * @index: Column index
 *
 * Gets a column value by index.
 *
 * Returns: (transfer none) (nullable): The value
 */
OrmValue *
orm_row_get_value (OrmRow *self,
                   gint    index)
{
    g_return_val_if_fail (ORM_IS_ROW (self), NULL);
    g_return_val_if_fail (index >= 0 && (guint) index < self->values->len, NULL);

    return g_ptr_array_index (self->values, index);
}

/**
 * orm_row_get_value_by_name:
 * @self: An #OrmRow
 * @name: Column name
 *
 * Gets a column value by name.
 *
 * Returns: (transfer none) (nullable): The value
 */
OrmValue *
orm_row_get_value_by_name (OrmRow      *self,
                           const gchar *name)
{
    gint index;

    g_return_val_if_fail (ORM_IS_ROW (self), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    index = orm_row_get_column_index (self, name);
    if (index < 0)
    {
        return NULL;
    }

    return orm_row_get_value (self, index);
}

/**
 * orm_row_get_integer:
 * @self: An #OrmRow
 * @index: Column index
 *
 * Gets a column value as integer.
 *
 * Returns: The integer value
 */
gint64
orm_row_get_integer (OrmRow *self,
                     gint    index)
{
    OrmValue *value;

    g_return_val_if_fail (ORM_IS_ROW (self), 0);

    value = orm_row_get_value (self, index);
    if (value == NULL || orm_value_is_null (value))
    {
        return 0;
    }

    return orm_value_get_integer (value);
}

/**
 * orm_row_get_float:
 * @self: An #OrmRow
 * @index: Column index
 *
 * Gets a column value as float.
 *
 * Returns: The float value
 */
gdouble
orm_row_get_float (OrmRow *self,
                   gint    index)
{
    OrmValue *value;

    g_return_val_if_fail (ORM_IS_ROW (self), 0.0);

    value = orm_row_get_value (self, index);
    if (value == NULL || orm_value_is_null (value))
    {
        return 0.0;
    }

    return orm_value_get_float (value);
}

/**
 * orm_row_get_string:
 * @self: An #OrmRow
 * @index: Column index
 *
 * Gets a column value as string.
 *
 * Returns: (transfer none) (nullable): The string value
 */
const gchar *
orm_row_get_string (OrmRow *self,
                    gint    index)
{
    OrmValue *value;

    g_return_val_if_fail (ORM_IS_ROW (self), NULL);

    value = orm_row_get_value (self, index);
    if (value == NULL || orm_value_is_null (value))
    {
        return NULL;
    }

    return orm_value_get_string (value);
}

/**
 * orm_row_get_boolean:
 * @self: An #OrmRow
 * @index: Column index
 *
 * Gets a column value as boolean.
 *
 * Returns: The boolean value
 */
gboolean
orm_row_get_boolean (OrmRow *self,
                     gint    index)
{
    OrmValue *value;

    g_return_val_if_fail (ORM_IS_ROW (self), FALSE);

    value = orm_row_get_value (self, index);
    if (value == NULL || orm_value_is_null (value))
    {
        return FALSE;
    }

    return orm_value_get_boolean (value);
}

/**
 * orm_row_is_null:
 * @self: An #OrmRow
 * @index: Column index
 *
 * Checks if a column value is NULL.
 *
 * Returns: %TRUE if NULL
 */
gboolean
orm_row_is_null (OrmRow *self,
                 gint    index)
{
    OrmValue *value;

    g_return_val_if_fail (ORM_IS_ROW (self), TRUE);

    value = orm_row_get_value (self, index);
    if (value == NULL)
    {
        return TRUE;
    }

    return orm_value_is_null (value);
}
