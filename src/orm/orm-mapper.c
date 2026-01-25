/* orm-mapper.c
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

#include "orm-mapper.h"
#include "orm-serializable.h"
#include "../schema/orm-column.h"

#include <string.h>

/*
 * OrmMapper - GType to table mapping.
 *
 * Maps a GType to a database table, containing property mappings
 * and relationship definitions. Used to generate table schemas
 * and to convert between GObjects and database rows.
 */

struct _OrmMapper
{
    GObject parent_instance;

    GType       gtype;
    gchar      *table_name;

    /* Property mappings: property_name -> OrmProperty */
    GHashTable *properties;
    /* Column to property lookup: column_name -> OrmProperty */
    GHashTable *column_to_property;
    /* Ordered list of properties */
    GList      *property_list;

    /* Relationship definitions: name -> OrmRelationship */
    GHashTable *relationships;
    GList      *relationship_list;

    /* Cached primary key property */
    OrmProperty *primary_key;
};

G_DEFINE_TYPE (OrmMapper, orm_mapper, G_TYPE_OBJECT)

static void
orm_mapper_finalize (GObject *object)
{
    OrmMapper *self = ORM_MAPPER (object);

    g_clear_pointer (&self->table_name, g_free);
    g_clear_pointer (&self->properties, g_hash_table_unref);
    g_clear_pointer (&self->column_to_property, g_hash_table_unref);
    g_clear_pointer (&self->property_list, g_list_free);
    g_clear_pointer (&self->relationships, g_hash_table_unref);
    g_clear_pointer (&self->relationship_list, g_list_free);

    G_OBJECT_CLASS (orm_mapper_parent_class)->finalize (object);
}

static void
orm_mapper_class_init (OrmMapperClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_mapper_finalize;
}

static void
orm_mapper_init (OrmMapper *self)
{
    self->gtype = G_TYPE_NONE;
    self->table_name = NULL;
    self->properties = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, g_object_unref);
    self->column_to_property = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                       g_free, NULL);
    self->property_list = NULL;
    self->relationships = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, g_object_unref);
    self->relationship_list = NULL;
    self->primary_key = NULL;
}

/*
 * Convert a GType name to a table name.
 * Converts CamelCase to snake_case and lowercases.
 */
static gchar *
type_name_to_table_name (const gchar *type_name)
{
    GString *result;
    const gchar *p;
    gboolean prev_lower;

    result = g_string_new (NULL);
    prev_lower = FALSE;

    for (p = type_name; *p != '\0'; p++)
    {
        if (g_ascii_isupper (*p))
        {
            if (prev_lower && result->len > 0)
            {
                g_string_append_c (result, '_');
            }
            g_string_append_c (result, g_ascii_tolower (*p));
            prev_lower = FALSE;
        }
        else
        {
            g_string_append_c (result, *p);
            prev_lower = g_ascii_islower (*p);
        }
    }

    return g_string_free (result, FALSE);
}

/**
 * orm_mapper_new:
 * @gtype: The GType to map
 * @table_name: (nullable): The table name (defaults to lowercase type name)
 *
 * Creates a new mapper for the given GType.
 *
 * Returns: (transfer full): A new #OrmMapper
 */
OrmMapper *
orm_mapper_new (GType        gtype,
                const gchar *table_name)
{
    OrmMapper *self;

    g_return_val_if_fail (gtype != G_TYPE_NONE, NULL);

    self = g_object_new (ORM_TYPE_MAPPER, NULL);
    self->gtype = gtype;

    if (table_name != NULL)
    {
        self->table_name = g_strdup (table_name);
    }
    else
    {
        self->table_name = type_name_to_table_name (g_type_name (gtype));
    }

    return self;
}

/**
 * orm_mapper_new_from_serializable:
 * @gtype: A GType implementing OrmSerializable
 *
 * Creates a mapper by introspecting an OrmSerializable type.
 * Automatically maps all serializable properties.
 *
 * Returns: (transfer full): A new #OrmMapper
 */
OrmMapper *
orm_mapper_new_from_serializable (GType gtype)
{
    OrmMapper *self;
    GObjectClass *klass;
    GParamSpec **pspecs;
    guint n_pspecs;
    guint i;
    const gchar *primary_key_name;
    gpointer instance;

    g_return_val_if_fail (g_type_is_a (gtype, ORM_TYPE_SERIALIZABLE), NULL);

    /* Create a temporary instance to get table name and primary key */
    instance = g_object_new (gtype, NULL);

    self = orm_mapper_new (gtype,
                           orm_serializable_get_table_name (ORM_SERIALIZABLE (instance)));
    primary_key_name = orm_serializable_get_primary_key (ORM_SERIALIZABLE (instance));

    g_object_unref (instance);

    /* Get all properties from the class */
    klass = g_type_class_ref (gtype);
    pspecs = g_object_class_list_properties (klass, &n_pspecs);

    for (i = 0; i < n_pspecs; i++)
    {
        GParamSpec *pspec = pspecs[i];
        OrmPropertyFlags flags = ORM_PROPERTY_NONE;
        OrmProperty *prop;

        /* Skip non-readable or non-writable properties */
        if (!(pspec->flags & G_PARAM_READABLE) ||
            !(pspec->flags & G_PARAM_WRITABLE))
        {
            continue;
        }

        /* Check if this is the primary key */
        if (g_strcmp0 (pspec->name, primary_key_name) == 0)
        {
            flags |= ORM_PROPERTY_PRIMARY_KEY;
            flags |= ORM_PROPERTY_AUTO_INCREMENT;
        }

        /* Create property mapping */
        prop = orm_property_new_from_pspec (pspec, NULL, flags);
        orm_mapper_add_property (self, prop);
        g_object_unref (prop);
    }

    g_free (pspecs);
    g_type_class_unref (klass);

    return self;
}

/**
 * orm_mapper_get_gtype:
 * @self: An #OrmMapper
 *
 * Gets the GType this mapper handles.
 *
 * Returns: The GType
 */
GType
orm_mapper_get_gtype (OrmMapper *self)
{
    g_return_val_if_fail (ORM_IS_MAPPER (self), G_TYPE_NONE);
    return self->gtype;
}

/**
 * orm_mapper_get_table_name:
 * @self: An #OrmMapper
 *
 * Gets the database table name.
 *
 * Returns: (transfer none): The table name
 */
const gchar *
orm_mapper_get_table_name (OrmMapper *self)
{
    g_return_val_if_fail (ORM_IS_MAPPER (self), NULL);
    return self->table_name;
}

/**
 * orm_mapper_add_property:
 * @self: An #OrmMapper
 * @property: (transfer none): The property mapping to add
 *
 * Adds a property mapping.
 */
void
orm_mapper_add_property (OrmMapper   *self,
                         OrmProperty *property)
{
    const gchar *prop_name;
    const gchar *col_name;

    g_return_if_fail (ORM_IS_MAPPER (self));
    g_return_if_fail (ORM_IS_PROPERTY (property));

    prop_name = orm_property_get_property_name (property);
    col_name = orm_property_get_column_name (property);

    /* Add to hash tables */
    g_hash_table_insert (self->properties,
                         g_strdup (prop_name),
                         g_object_ref (property));
    g_hash_table_insert (self->column_to_property,
                         g_strdup (col_name),
                         property);

    /* Add to ordered list */
    self->property_list = g_list_append (self->property_list, property);

    /* Track primary key */
    if (orm_property_is_primary_key (property))
    {
        self->primary_key = property;
    }
}

/**
 * orm_mapper_get_property:
 * @self: An #OrmMapper
 * @property_name: The property name
 *
 * Gets a property mapping by name.
 *
 * Returns: (transfer none) (nullable): The property mapping
 */
OrmProperty *
orm_mapper_get_property (OrmMapper   *self,
                         const gchar *property_name)
{
    g_return_val_if_fail (ORM_IS_MAPPER (self), NULL);
    g_return_val_if_fail (property_name != NULL, NULL);

    return g_hash_table_lookup (self->properties, property_name);
}

/**
 * orm_mapper_get_property_by_column:
 * @self: An #OrmMapper
 * @column_name: The column name
 *
 * Gets a property mapping by column name.
 *
 * Returns: (transfer none) (nullable): The property mapping
 */
OrmProperty *
orm_mapper_get_property_by_column (OrmMapper   *self,
                                   const gchar *column_name)
{
    g_return_val_if_fail (ORM_IS_MAPPER (self), NULL);
    g_return_val_if_fail (column_name != NULL, NULL);

    return g_hash_table_lookup (self->column_to_property, column_name);
}

/**
 * orm_mapper_get_properties:
 * @self: An #OrmMapper
 *
 * Gets all property mappings.
 *
 * Returns: (transfer none) (element-type OrmProperty): List of properties
 */
GList *
orm_mapper_get_properties (OrmMapper *self)
{
    g_return_val_if_fail (ORM_IS_MAPPER (self), NULL);
    return self->property_list;
}

/**
 * orm_mapper_get_primary_key_property:
 * @self: An #OrmMapper
 *
 * Gets the primary key property mapping.
 *
 * Returns: (transfer none) (nullable): The primary key property
 */
OrmProperty *
orm_mapper_get_primary_key_property (OrmMapper *self)
{
    g_return_val_if_fail (ORM_IS_MAPPER (self), NULL);
    return self->primary_key;
}

/**
 * orm_mapper_add_relationship:
 * @self: An #OrmMapper
 * @relationship: (transfer none): The relationship to add
 *
 * Adds a relationship definition.
 */
void
orm_mapper_add_relationship (OrmMapper       *self,
                             OrmRelationship *relationship)
{
    const gchar *name;

    g_return_if_fail (ORM_IS_MAPPER (self));
    g_return_if_fail (ORM_IS_RELATIONSHIP (relationship));

    name = orm_relationship_get_name (relationship);

    g_hash_table_insert (self->relationships,
                         g_strdup (name),
                         g_object_ref (relationship));
    self->relationship_list = g_list_append (self->relationship_list, relationship);
}

/**
 * orm_mapper_get_relationship:
 * @self: An #OrmMapper
 * @name: The relationship name
 *
 * Gets a relationship by name.
 *
 * Returns: (transfer none) (nullable): The relationship
 */
OrmRelationship *
orm_mapper_get_relationship (OrmMapper   *self,
                             const gchar *name)
{
    g_return_val_if_fail (ORM_IS_MAPPER (self), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_hash_table_lookup (self->relationships, name);
}

/**
 * orm_mapper_get_relationships:
 * @self: An #OrmMapper
 *
 * Gets all relationships.
 *
 * Returns: (transfer none) (element-type OrmRelationship): List of relationships
 */
GList *
orm_mapper_get_relationships (OrmMapper *self)
{
    g_return_val_if_fail (ORM_IS_MAPPER (self), NULL);
    return self->relationship_list;
}

/**
 * orm_mapper_to_table:
 * @self: An #OrmMapper
 *
 * Creates an OrmTable schema from this mapper.
 *
 * Returns: (transfer full): A new #OrmTable
 */
OrmTable *
orm_mapper_to_table (OrmMapper *self)
{
    OrmTable *table;
    GList *l;

    g_return_val_if_fail (ORM_IS_MAPPER (self), NULL);

    table = orm_table_new (self->table_name, NULL);

    for (l = self->property_list; l != NULL; l = l->next)
    {
        OrmProperty *prop = ORM_PROPERTY (l->data);
        OrmColumn *column = orm_property_to_column (prop);
        orm_table_add_column (table, column);
        g_object_unref (column);
    }

    return table;
}

/**
 * orm_mapper_get_column_names:
 * @self: An #OrmMapper
 *
 * Gets all column names in insertion order.
 *
 * Returns: (transfer full) (element-type utf8): List of column names
 */
GList *
orm_mapper_get_column_names (OrmMapper *self)
{
    GList *names = NULL;
    GList *l;

    g_return_val_if_fail (ORM_IS_MAPPER (self), NULL);

    for (l = self->property_list; l != NULL; l = l->next)
    {
        OrmProperty *prop = ORM_PROPERTY (l->data);
        names = g_list_append (names,
                               g_strdup (orm_property_get_column_name (prop)));
    }

    return names;
}

/**
 * orm_mapper_get_insert_columns:
 * @self: An #OrmMapper
 *
 * Gets column names for INSERT (excludes auto-increment).
 *
 * Returns: (transfer full) (element-type utf8): List of column names
 */
GList *
orm_mapper_get_insert_columns (OrmMapper *self)
{
    GList *names = NULL;
    GList *l;

    g_return_val_if_fail (ORM_IS_MAPPER (self), NULL);

    for (l = self->property_list; l != NULL; l = l->next)
    {
        OrmProperty *prop = ORM_PROPERTY (l->data);

        /* Skip auto-increment columns on insert */
        if (orm_property_is_auto_increment (prop))
        {
            continue;
        }

        names = g_list_append (names,
                               g_strdup (orm_property_get_column_name (prop)));
    }

    return names;
}
