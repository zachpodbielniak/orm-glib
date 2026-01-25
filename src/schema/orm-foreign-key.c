/* orm-foreign-key.c
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

#include "orm-foreign-key.h"

/*
 * OrmForeignKey - Foreign key constraint for a table.
 *
 * A foreign key defines a relationship between two tables by
 * referencing columns in another table.
 */
struct _OrmForeignKey
{
    GObject parent_instance;

    gchar *name;
    gchar *ref_table;

    GList *local_columns;  /* List of local column names */
    GList *ref_columns;    /* List of referenced column names */

    OrmForeignKeyAction on_delete;
    OrmForeignKeyAction on_update;
};

G_DEFINE_TYPE (OrmForeignKey, orm_foreign_key, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_NAME,
    PROP_REF_TABLE,
    PROP_ON_DELETE,
    PROP_ON_UPDATE,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
orm_foreign_key_finalize (GObject *object)
{
    OrmForeignKey *self = ORM_FOREIGN_KEY (object);

    g_free (self->name);
    g_free (self->ref_table);
    g_list_free_full (self->local_columns, g_free);
    g_list_free_full (self->ref_columns, g_free);

    G_OBJECT_CLASS (orm_foreign_key_parent_class)->finalize (object);
}

static void
orm_foreign_key_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
    OrmForeignKey *self = ORM_FOREIGN_KEY (object);

    switch (prop_id)
    {
    case PROP_NAME:
        g_value_set_string (value, self->name);
        break;
    case PROP_REF_TABLE:
        g_value_set_string (value, self->ref_table);
        break;
    case PROP_ON_DELETE:
        g_value_set_enum (value, self->on_delete);
        break;
    case PROP_ON_UPDATE:
        g_value_set_enum (value, self->on_update);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_foreign_key_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
    OrmForeignKey *self = ORM_FOREIGN_KEY (object);

    switch (prop_id)
    {
    case PROP_NAME:
        g_free (self->name);
        self->name = g_value_dup_string (value);
        break;
    case PROP_REF_TABLE:
        g_free (self->ref_table);
        self->ref_table = g_value_dup_string (value);
        break;
    case PROP_ON_DELETE:
        self->on_delete = g_value_get_enum (value);
        break;
    case PROP_ON_UPDATE:
        self->on_update = g_value_get_enum (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_foreign_key_class_init (OrmForeignKeyClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_foreign_key_finalize;
    object_class->get_property = orm_foreign_key_get_property;
    object_class->set_property = orm_foreign_key_set_property;

    /**
     * OrmForeignKey:name:
     *
     * The name of the foreign key constraint.
     */
    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "The constraint name",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    /**
     * OrmForeignKey:ref-table:
     *
     * The name of the referenced table.
     */
    properties[PROP_REF_TABLE] =
        g_param_spec_string ("ref-table",
                             "Referenced Table",
                             "The name of the referenced table",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    /**
     * OrmForeignKey:on-delete:
     *
     * The action to take when the referenced row is deleted.
     */
    properties[PROP_ON_DELETE] =
        g_param_spec_enum ("on-delete",
                           "On Delete",
                           "Action when referenced row is deleted",
                           ORM_TYPE_FOREIGN_KEY_ACTION,
                           ORM_FK_NO_ACTION,
                           G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS);

    /**
     * OrmForeignKey:on-update:
     *
     * The action to take when the referenced row is updated.
     */
    properties[PROP_ON_UPDATE] =
        g_param_spec_enum ("on-update",
                           "On Update",
                           "Action when referenced row is updated",
                           ORM_TYPE_FOREIGN_KEY_ACTION,
                           ORM_FK_NO_ACTION,
                           G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
orm_foreign_key_init (OrmForeignKey *self)
{
    self->name = NULL;
    self->ref_table = NULL;
    self->local_columns = NULL;
    self->ref_columns = NULL;
    self->on_delete = ORM_FK_NO_ACTION;
    self->on_update = ORM_FK_NO_ACTION;
}

/**
 * orm_foreign_key_new:
 * @name: (nullable): The constraint name
 * @ref_table: The referenced table name
 *
 * Creates a new foreign key constraint.
 *
 * Returns: (transfer full): A new #OrmForeignKey
 */
OrmForeignKey *
orm_foreign_key_new (const gchar *name,
                     const gchar *ref_table)
{
    g_return_val_if_fail (ref_table != NULL, NULL);

    return g_object_new (ORM_TYPE_FOREIGN_KEY,
                         "name", name,
                         "ref-table", ref_table,
                         NULL);
}

/**
 * orm_foreign_key_get_name:
 * @self: An #OrmForeignKey
 *
 * Gets the constraint name.
 *
 * Returns: (transfer none) (nullable): The constraint name
 */
const gchar *
orm_foreign_key_get_name (OrmForeignKey *self)
{
    g_return_val_if_fail (ORM_IS_FOREIGN_KEY (self), NULL);

    return self->name;
}

/**
 * orm_foreign_key_set_name:
 * @self: An #OrmForeignKey
 * @name: (nullable): The constraint name
 *
 * Sets the constraint name.
 */
void
orm_foreign_key_set_name (OrmForeignKey *self,
                          const gchar   *name)
{
    g_return_if_fail (ORM_IS_FOREIGN_KEY (self));

    if (g_strcmp0 (self->name, name) != 0)
    {
        g_free (self->name);
        self->name = g_strdup (name);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
    }
}

/**
 * orm_foreign_key_get_ref_table:
 * @self: An #OrmForeignKey
 *
 * Gets the referenced table name.
 *
 * Returns: (transfer none): The referenced table name
 */
const gchar *
orm_foreign_key_get_ref_table (OrmForeignKey *self)
{
    g_return_val_if_fail (ORM_IS_FOREIGN_KEY (self), NULL);

    return self->ref_table;
}

/**
 * orm_foreign_key_add_column:
 * @self: An #OrmForeignKey
 * @local_column: The local column name
 * @ref_column: The referenced column name
 *
 * Adds a column mapping to this foreign key.
 */
void
orm_foreign_key_add_column (OrmForeignKey *self,
                            const gchar   *local_column,
                            const gchar   *ref_column)
{
    g_return_if_fail (ORM_IS_FOREIGN_KEY (self));
    g_return_if_fail (local_column != NULL);
    g_return_if_fail (ref_column != NULL);

    self->local_columns = g_list_append (self->local_columns,
                                         g_strdup (local_column));
    self->ref_columns = g_list_append (self->ref_columns,
                                       g_strdup (ref_column));
}

/**
 * orm_foreign_key_get_local_columns:
 * @self: An #OrmForeignKey
 *
 * Gets the list of local column names.
 *
 * Returns: (element-type utf8) (transfer none): List of local column names
 */
GList *
orm_foreign_key_get_local_columns (OrmForeignKey *self)
{
    g_return_val_if_fail (ORM_IS_FOREIGN_KEY (self), NULL);

    return self->local_columns;
}

/**
 * orm_foreign_key_get_ref_columns:
 * @self: An #OrmForeignKey
 *
 * Gets the list of referenced column names.
 *
 * Returns: (element-type utf8) (transfer none): List of referenced column names
 */
GList *
orm_foreign_key_get_ref_columns (OrmForeignKey *self)
{
    g_return_val_if_fail (ORM_IS_FOREIGN_KEY (self), NULL);

    return self->ref_columns;
}

/**
 * orm_foreign_key_get_column_count:
 * @self: An #OrmForeignKey
 *
 * Gets the number of column mappings in this foreign key.
 *
 * Returns: The column count
 */
guint
orm_foreign_key_get_column_count (OrmForeignKey *self)
{
    g_return_val_if_fail (ORM_IS_FOREIGN_KEY (self), 0);

    return g_list_length (self->local_columns);
}

/**
 * orm_foreign_key_get_on_delete:
 * @self: An #OrmForeignKey
 *
 * Gets the ON DELETE action.
 *
 * Returns: The ON DELETE action
 */
OrmForeignKeyAction
orm_foreign_key_get_on_delete (OrmForeignKey *self)
{
    g_return_val_if_fail (ORM_IS_FOREIGN_KEY (self), ORM_FK_NO_ACTION);

    return self->on_delete;
}

/**
 * orm_foreign_key_set_on_delete:
 * @self: An #OrmForeignKey
 * @action: The ON DELETE action
 *
 * Sets the ON DELETE action.
 */
void
orm_foreign_key_set_on_delete (OrmForeignKey      *self,
                               OrmForeignKeyAction action)
{
    g_return_if_fail (ORM_IS_FOREIGN_KEY (self));

    if (self->on_delete != action)
    {
        self->on_delete = action;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ON_DELETE]);
    }
}

/**
 * orm_foreign_key_get_on_update:
 * @self: An #OrmForeignKey
 *
 * Gets the ON UPDATE action.
 *
 * Returns: The ON UPDATE action
 */
OrmForeignKeyAction
orm_foreign_key_get_on_update (OrmForeignKey *self)
{
    g_return_val_if_fail (ORM_IS_FOREIGN_KEY (self), ORM_FK_NO_ACTION);

    return self->on_update;
}

/**
 * orm_foreign_key_set_on_update:
 * @self: An #OrmForeignKey
 * @action: The ON UPDATE action
 *
 * Sets the ON UPDATE action.
 */
void
orm_foreign_key_set_on_update (OrmForeignKey      *self,
                               OrmForeignKeyAction action)
{
    g_return_if_fail (ORM_IS_FOREIGN_KEY (self));

    if (self->on_update != action)
    {
        self->on_update = action;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ON_UPDATE]);
    }
}
