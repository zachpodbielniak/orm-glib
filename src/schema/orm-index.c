/* orm-index.c
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

#include "orm-index.h"
#include <stdarg.h>

/*
 * OrmIndex - Index definition for a table.
 *
 * An index can consist of one or more columns and can optionally
 * be a unique index.
 */
struct _OrmIndex
{
    GObject parent_instance;

    gchar    *name;
    gboolean  unique;
    GList    *columns; /* List of column names (gchar *) */
};

G_DEFINE_TYPE (OrmIndex, orm_index, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_NAME,
    PROP_UNIQUE,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
orm_index_finalize (GObject *object)
{
    OrmIndex *self = ORM_INDEX (object);

    g_free (self->name);
    g_list_free_full (self->columns, g_free);

    G_OBJECT_CLASS (orm_index_parent_class)->finalize (object);
}

static void
orm_index_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
    OrmIndex *self = ORM_INDEX (object);

    switch (prop_id)
    {
    case PROP_NAME:
        g_value_set_string (value, self->name);
        break;
    case PROP_UNIQUE:
        g_value_set_boolean (value, self->unique);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_index_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
    OrmIndex *self = ORM_INDEX (object);

    switch (prop_id)
    {
    case PROP_NAME:
        g_free (self->name);
        self->name = g_value_dup_string (value);
        break;
    case PROP_UNIQUE:
        self->unique = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_index_class_init (OrmIndexClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_index_finalize;
    object_class->get_property = orm_index_get_property;
    object_class->set_property = orm_index_set_property;

    /**
     * OrmIndex:name:
     *
     * The name of the index.
     */
    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "The index name",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    /**
     * OrmIndex:unique:
     *
     * Whether this is a unique index.
     */
    properties[PROP_UNIQUE] =
        g_param_spec_boolean ("unique",
                              "Unique",
                              "Whether this is a unique index",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
orm_index_init (OrmIndex *self)
{
    self->name = NULL;
    self->unique = FALSE;
    self->columns = NULL;
}

/**
 * orm_index_new:
 * @name: The index name
 *
 * Creates a new index definition.
 *
 * Returns: (transfer full): A new #OrmIndex
 */
OrmIndex *
orm_index_new (const gchar *name)
{
    g_return_val_if_fail (name != NULL, NULL);

    return g_object_new (ORM_TYPE_INDEX,
                         "name", name,
                         NULL);
}

/**
 * orm_index_new_with_columns:
 * @name: The index name
 * @...: NULL-terminated list of column names
 *
 * Creates a new index with the specified columns.
 *
 * Returns: (transfer full): A new #OrmIndex
 */
OrmIndex *
orm_index_new_with_columns (const gchar *name,
                            ...)
{
    OrmIndex *idx;
    va_list args;
    const gchar *column;

    idx = orm_index_new (name);

    va_start (args, name);
    while ((column = va_arg (args, const gchar *)) != NULL)
    {
        orm_index_add_column (idx, column);
    }
    va_end (args);

    return idx;
}

/**
 * orm_index_get_name:
 * @self: An #OrmIndex
 *
 * Gets the index name.
 *
 * Returns: (transfer none): The index name
 */
const gchar *
orm_index_get_name (OrmIndex *self)
{
    g_return_val_if_fail (ORM_IS_INDEX (self), NULL);

    return self->name;
}

/**
 * orm_index_get_unique:
 * @self: An #OrmIndex
 *
 * Gets whether this is a unique index.
 *
 * Returns: %TRUE if unique, %FALSE otherwise
 */
gboolean
orm_index_get_unique (OrmIndex *self)
{
    g_return_val_if_fail (ORM_IS_INDEX (self), FALSE);

    return self->unique;
}

/**
 * orm_index_set_unique:
 * @self: An #OrmIndex
 * @unique: Whether this is a unique index
 *
 * Sets whether this is a unique index.
 */
void
orm_index_set_unique (OrmIndex *self,
                      gboolean  unique)
{
    g_return_if_fail (ORM_IS_INDEX (self));

    if (self->unique != unique)
    {
        self->unique = unique;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UNIQUE]);
    }
}

/**
 * orm_index_add_column:
 * @self: An #OrmIndex
 * @column_name: The column name
 *
 * Adds a column to this index.
 */
void
orm_index_add_column (OrmIndex    *self,
                      const gchar *column_name)
{
    g_return_if_fail (ORM_IS_INDEX (self));
    g_return_if_fail (column_name != NULL);

    self->columns = g_list_append (self->columns, g_strdup (column_name));
}

/**
 * orm_index_get_columns:
 * @self: An #OrmIndex
 *
 * Gets the list of column names in this index.
 *
 * Returns: (element-type utf8) (transfer none): List of column names
 */
GList *
orm_index_get_columns (OrmIndex *self)
{
    g_return_val_if_fail (ORM_IS_INDEX (self), NULL);

    return self->columns;
}

/**
 * orm_index_get_column_count:
 * @self: An #OrmIndex
 *
 * Gets the number of columns in this index.
 *
 * Returns: The column count
 */
guint
orm_index_get_column_count (OrmIndex *self)
{
    g_return_val_if_fail (ORM_IS_INDEX (self), 0);

    return g_list_length (self->columns);
}
