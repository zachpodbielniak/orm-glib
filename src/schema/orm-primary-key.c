/* orm-primary-key.c
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

#include "orm-primary-key.h"
#include <stdarg.h>

/*
 * OrmPrimaryKey - Primary key constraint for a table.
 *
 * A primary key can consist of one or more columns. This is a
 * table-level constraint.
 */
struct _OrmPrimaryKey
{
    GObject parent_instance;

    gchar *name;
    GList *columns; /* List of column names (gchar *) */
};

G_DEFINE_TYPE (OrmPrimaryKey, orm_primary_key, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_NAME,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
orm_primary_key_finalize (GObject *object)
{
    OrmPrimaryKey *self = ORM_PRIMARY_KEY (object);

    g_free (self->name);
    g_list_free_full (self->columns, g_free);

    G_OBJECT_CLASS (orm_primary_key_parent_class)->finalize (object);
}

static void
orm_primary_key_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
    OrmPrimaryKey *self = ORM_PRIMARY_KEY (object);

    switch (prop_id)
    {
    case PROP_NAME:
        g_value_set_string (value, self->name);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_primary_key_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
    OrmPrimaryKey *self = ORM_PRIMARY_KEY (object);

    switch (prop_id)
    {
    case PROP_NAME:
        g_free (self->name);
        self->name = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_primary_key_class_init (OrmPrimaryKeyClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_primary_key_finalize;
    object_class->get_property = orm_primary_key_get_property;
    object_class->set_property = orm_primary_key_set_property;

    /**
     * OrmPrimaryKey:name:
     *
     * The name of the primary key constraint.
     */
    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "The constraint name",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
orm_primary_key_init (OrmPrimaryKey *self)
{
    self->name = NULL;
    self->columns = NULL;
}

/**
 * orm_primary_key_new:
 * @name: (nullable): The constraint name
 *
 * Creates a new primary key constraint.
 *
 * Returns: (transfer full): A new #OrmPrimaryKey
 */
OrmPrimaryKey *
orm_primary_key_new (const gchar *name)
{
    return g_object_new (ORM_TYPE_PRIMARY_KEY,
                         "name", name,
                         NULL);
}

/**
 * orm_primary_key_new_with_columns:
 * @name: (nullable): The constraint name
 * @...: NULL-terminated list of column names
 *
 * Creates a new primary key constraint with the specified columns.
 *
 * Returns: (transfer full): A new #OrmPrimaryKey
 */
OrmPrimaryKey *
orm_primary_key_new_with_columns (const gchar *name,
                                  ...)
{
    OrmPrimaryKey *pk;
    va_list args;
    const gchar *column;

    pk = orm_primary_key_new (name);

    va_start (args, name);
    while ((column = va_arg (args, const gchar *)) != NULL)
    {
        orm_primary_key_add_column (pk, column);
    }
    va_end (args);

    return pk;
}

/**
 * orm_primary_key_get_name:
 * @self: An #OrmPrimaryKey
 *
 * Gets the constraint name.
 *
 * Returns: (transfer none) (nullable): The constraint name
 */
const gchar *
orm_primary_key_get_name (OrmPrimaryKey *self)
{
    g_return_val_if_fail (ORM_IS_PRIMARY_KEY (self), NULL);

    return self->name;
}

/**
 * orm_primary_key_set_name:
 * @self: An #OrmPrimaryKey
 * @name: (nullable): The constraint name
 *
 * Sets the constraint name.
 */
void
orm_primary_key_set_name (OrmPrimaryKey *self,
                          const gchar   *name)
{
    g_return_if_fail (ORM_IS_PRIMARY_KEY (self));

    if (g_strcmp0 (self->name, name) != 0)
    {
        g_free (self->name);
        self->name = g_strdup (name);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
    }
}

/**
 * orm_primary_key_add_column:
 * @self: An #OrmPrimaryKey
 * @column_name: The column name
 *
 * Adds a column to this primary key.
 */
void
orm_primary_key_add_column (OrmPrimaryKey *self,
                            const gchar   *column_name)
{
    g_return_if_fail (ORM_IS_PRIMARY_KEY (self));
    g_return_if_fail (column_name != NULL);

    self->columns = g_list_append (self->columns, g_strdup (column_name));
}

/**
 * orm_primary_key_get_columns:
 * @self: An #OrmPrimaryKey
 *
 * Gets the list of column names in this primary key.
 *
 * Returns: (element-type utf8) (transfer none): List of column names
 */
GList *
orm_primary_key_get_columns (OrmPrimaryKey *self)
{
    g_return_val_if_fail (ORM_IS_PRIMARY_KEY (self), NULL);

    return self->columns;
}

/**
 * orm_primary_key_get_column_count:
 * @self: An #OrmPrimaryKey
 *
 * Gets the number of columns in this primary key.
 *
 * Returns: The column count
 */
guint
orm_primary_key_get_column_count (OrmPrimaryKey *self)
{
    g_return_val_if_fail (ORM_IS_PRIMARY_KEY (self), 0);

    return g_list_length (self->columns);
}

/**
 * orm_primary_key_has_column:
 * @self: An #OrmPrimaryKey
 * @column_name: The column name to check
 *
 * Checks if a column is part of this primary key.
 *
 * Returns: %TRUE if the column is in the primary key
 */
gboolean
orm_primary_key_has_column (OrmPrimaryKey *self,
                            const gchar   *column_name)
{
    GList *l;

    g_return_val_if_fail (ORM_IS_PRIMARY_KEY (self), FALSE);
    g_return_val_if_fail (column_name != NULL, FALSE);

    for (l = self->columns; l != NULL; l = l->next)
    {
        if (g_strcmp0 ((const gchar *) l->data, column_name) == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}
