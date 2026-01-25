/* orm-serializable.c
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

#include "orm-serializable.h"
#include "../core/orm-value.h"
#include "../core/orm-enums.h"

G_DEFINE_INTERFACE (OrmSerializable, orm_serializable, G_TYPE_OBJECT)

/*
 * Default implementation of serialize_property.
 * Converts GValue to OrmValue based on the GValue type.
 */
static OrmValue *
orm_serializable_real_serialize_property (OrmSerializable *serializable,
                                          const gchar     *property_name,
                                          const GValue    *value,
                                          GParamSpec      *pspec)
{
    (void) serializable;
    (void) property_name;
    (void) pspec;

    if (value == NULL || !G_IS_VALUE (value))
    {
        return orm_value_new_null ();
    }

    return orm_value_from_gvalue (value);
}

/*
 * Default implementation of deserialize_property.
 * Converts OrmValue to GValue based on the target property type.
 * Uses type-aware conversion to handle type mismatches between
 * database values and GObject property types.
 */
static gboolean
orm_serializable_real_deserialize_property (OrmSerializable *serializable,
                                            const gchar     *property_name,
                                            GValue          *value,
                                            GParamSpec      *pspec,
                                            OrmValue        *db_value)
{
    (void) serializable;
    (void) property_name;

    if (db_value == NULL || orm_value_is_null (db_value))
    {
        /* Leave value uninitialized for NULL */
        return TRUE;
    }

    /*
     * Use type-aware conversion if we have a pspec, which tells us
     * what type the target GValue should be. This handles cases like
     * INTEGER from database mapping to G_TYPE_INT property.
     */
    if (pspec != NULL)
    {
        return orm_value_to_gvalue_with_type (db_value, value, pspec->value_type);
    }

    /* Fallback to standard conversion */
    return orm_value_to_gvalue (db_value, value);
}

/*
 * Default implementation of find_property.
 * Uses GObject class property lookup.
 */
static GParamSpec *
orm_serializable_real_find_property (OrmSerializable *serializable,
                                     const gchar     *name)
{
    GObjectClass *klass;

    g_return_val_if_fail (ORM_IS_SERIALIZABLE (serializable), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    klass = G_OBJECT_GET_CLASS (serializable);
    return g_object_class_find_property (klass, name);
}

/*
 * Default implementation of list_properties.
 * Returns all readable/writable properties.
 */
static GParamSpec **
orm_serializable_real_list_properties (OrmSerializable *serializable,
                                       guint           *n_pspecs)
{
    GObjectClass *klass;
    GParamSpec **all_pspecs;
    GParamSpec **filtered;
    guint n_all;
    guint n_filtered;
    guint i;

    g_return_val_if_fail (ORM_IS_SERIALIZABLE (serializable), NULL);
    g_return_val_if_fail (n_pspecs != NULL, NULL);

    klass = G_OBJECT_GET_CLASS (serializable);
    all_pspecs = g_object_class_list_properties (klass, &n_all);

    /* First pass: count serializable properties */
    n_filtered = 0;
    for (i = 0; i < n_all; i++)
    {
        GParamFlags flags = all_pspecs[i]->flags;

        /* Include properties that are readable and writable */
        if ((flags & G_PARAM_READABLE) && (flags & G_PARAM_WRITABLE))
        {
            /* Exclude construct-only properties for deserialization */
            if (!(flags & G_PARAM_CONSTRUCT_ONLY))
            {
                n_filtered++;
            }
        }
    }

    /* Second pass: copy filtered properties */
    filtered = g_new (GParamSpec *, n_filtered + 1);
    n_filtered = 0;
    for (i = 0; i < n_all; i++)
    {
        GParamFlags flags = all_pspecs[i]->flags;

        if ((flags & G_PARAM_READABLE) && (flags & G_PARAM_WRITABLE))
        {
            if (!(flags & G_PARAM_CONSTRUCT_ONLY))
            {
                filtered[n_filtered++] = all_pspecs[i];
            }
        }
    }
    filtered[n_filtered] = NULL;

    g_free (all_pspecs);

    *n_pspecs = n_filtered;
    return filtered;
}

/*
 * Default implementation of get_table_name.
 * Converts GType name to lowercase with underscores.
 */
static const gchar *
orm_serializable_real_get_table_name (OrmSerializable *serializable)
{
    static GHashTable *table_names = NULL;
    const gchar *type_name;
    gchar *table_name;
    GString *result;
    const gchar *p;

    g_return_val_if_fail (ORM_IS_SERIALIZABLE (serializable), "unknown");

    /* Cache table names to avoid repeated computation */
    if (table_names == NULL)
    {
        table_names = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, g_free);
    }

    type_name = G_OBJECT_TYPE_NAME (serializable);

    /* Check cache */
    table_name = g_hash_table_lookup (table_names, type_name);
    if (table_name != NULL)
    {
        return table_name;
    }

    /* Convert PascalCase to snake_case */
    result = g_string_new (NULL);
    for (p = type_name; *p != '\0'; p++)
    {
        if (g_ascii_isupper (*p))
        {
            if (result->len > 0)
            {
                g_string_append_c (result, '_');
            }
            g_string_append_c (result, g_ascii_tolower (*p));
        }
        else
        {
            g_string_append_c (result, *p);
        }
    }

    table_name = g_string_free (result, FALSE);
    g_hash_table_insert (table_names, g_strdup (type_name), table_name);

    return table_name;
}

/*
 * Default implementation of get_primary_key.
 * Returns "id" as the default primary key property.
 */
static const gchar *
orm_serializable_real_get_primary_key (OrmSerializable *serializable)
{
    (void) serializable;

    return "id";
}

static void
orm_serializable_default_init (OrmSerializableInterface *iface)
{
    iface->serialize_property = orm_serializable_real_serialize_property;
    iface->deserialize_property = orm_serializable_real_deserialize_property;
    iface->find_property = orm_serializable_real_find_property;
    iface->list_properties = orm_serializable_real_list_properties;
    iface->get_table_name = orm_serializable_real_get_table_name;
    iface->get_primary_key = orm_serializable_real_get_primary_key;
}

/**
 * orm_serializable_serialize_property:
 * @serializable: An #OrmSerializable
 * @property_name: The property name
 * @value: The property value
 * @pspec: The property specification
 *
 * Serializes a property to a database value.
 *
 * Returns: (transfer full) (nullable): The serialized value
 */
OrmValue *
orm_serializable_serialize_property (OrmSerializable *serializable,
                                     const gchar     *property_name,
                                     const GValue    *value,
                                     GParamSpec      *pspec)
{
    OrmSerializableInterface *iface;

    g_return_val_if_fail (ORM_IS_SERIALIZABLE (serializable), NULL);
    g_return_val_if_fail (property_name != NULL, NULL);

    iface = ORM_SERIALIZABLE_GET_IFACE (serializable);
    return iface->serialize_property (serializable, property_name, value, pspec);
}

/**
 * orm_serializable_deserialize_property:
 * @serializable: An #OrmSerializable
 * @property_name: The property name
 * @value: (out): The value to fill
 * @pspec: The property specification
 * @db_value: The database value
 *
 * Deserializes a database value to a property.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_serializable_deserialize_property (OrmSerializable *serializable,
                                       const gchar     *property_name,
                                       GValue          *value,
                                       GParamSpec      *pspec,
                                       OrmValue        *db_value)
{
    OrmSerializableInterface *iface;

    g_return_val_if_fail (ORM_IS_SERIALIZABLE (serializable), FALSE);
    g_return_val_if_fail (property_name != NULL, FALSE);
    g_return_val_if_fail (value != NULL, FALSE);

    iface = ORM_SERIALIZABLE_GET_IFACE (serializable);
    return iface->deserialize_property (serializable, property_name, value,
                                        pspec, db_value);
}

/**
 * orm_serializable_find_property:
 * @serializable: An #OrmSerializable
 * @name: The property name
 *
 * Finds a property by name.
 *
 * Returns: (transfer none) (nullable): The property specification
 */
GParamSpec *
orm_serializable_find_property (OrmSerializable *serializable,
                                const gchar     *name)
{
    OrmSerializableInterface *iface;

    g_return_val_if_fail (ORM_IS_SERIALIZABLE (serializable), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    iface = ORM_SERIALIZABLE_GET_IFACE (serializable);
    return iface->find_property (serializable, name);
}

/**
 * orm_serializable_list_properties:
 * @serializable: An #OrmSerializable
 * @n_pspecs: (out): Location to store property count
 *
 * Lists all serializable properties.
 *
 * Returns: (array length=n_pspecs) (transfer container): Array of properties
 */
GParamSpec **
orm_serializable_list_properties (OrmSerializable *serializable,
                                  guint           *n_pspecs)
{
    OrmSerializableInterface *iface;

    g_return_val_if_fail (ORM_IS_SERIALIZABLE (serializable), NULL);
    g_return_val_if_fail (n_pspecs != NULL, NULL);

    iface = ORM_SERIALIZABLE_GET_IFACE (serializable);
    return iface->list_properties (serializable, n_pspecs);
}

/**
 * orm_serializable_get_table_name:
 * @serializable: An #OrmSerializable
 *
 * Gets the database table name for this object.
 *
 * Returns: (transfer none): The table name
 */
const gchar *
orm_serializable_get_table_name (OrmSerializable *serializable)
{
    OrmSerializableInterface *iface;

    g_return_val_if_fail (ORM_IS_SERIALIZABLE (serializable), NULL);

    iface = ORM_SERIALIZABLE_GET_IFACE (serializable);
    return iface->get_table_name (serializable);
}

/**
 * orm_serializable_get_primary_key:
 * @serializable: An #OrmSerializable
 *
 * Gets the primary key property name.
 *
 * Returns: (transfer none): The primary key property name
 */
const gchar *
orm_serializable_get_primary_key (OrmSerializable *serializable)
{
    OrmSerializableInterface *iface;

    g_return_val_if_fail (ORM_IS_SERIALIZABLE (serializable), NULL);

    iface = ORM_SERIALIZABLE_GET_IFACE (serializable);
    return iface->get_primary_key (serializable);
}

/**
 * orm_serializable_get_property_value:
 * @serializable: An #OrmSerializable
 * @property_name: The property name
 *
 * Gets a property value as an OrmValue.
 *
 * Returns: (transfer full) (nullable): The property value
 */
OrmValue *
orm_serializable_get_property_value (OrmSerializable *serializable,
                                     const gchar     *property_name)
{
    GParamSpec *pspec;
    GValue value = G_VALUE_INIT;
    OrmValue *result;

    g_return_val_if_fail (ORM_IS_SERIALIZABLE (serializable), NULL);
    g_return_val_if_fail (property_name != NULL, NULL);

    pspec = orm_serializable_find_property (serializable, property_name);
    if (pspec == NULL)
    {
        return NULL;
    }

    g_value_init (&value, pspec->value_type);
    g_object_get_property (G_OBJECT (serializable), property_name, &value);

    result = orm_serializable_serialize_property (serializable, property_name,
                                                  &value, pspec);

    g_value_unset (&value);
    return result;
}

/**
 * orm_serializable_set_property_value:
 * @serializable: An #OrmSerializable
 * @property_name: The property name
 * @db_value: The value to set
 *
 * Sets a property from an OrmValue.
 *
 * Returns: %TRUE if the property was set
 */
gboolean
orm_serializable_set_property_value (OrmSerializable *serializable,
                                     const gchar     *property_name,
                                     OrmValue        *db_value)
{
    GParamSpec *pspec;
    GValue value = G_VALUE_INIT;
    gboolean success;

    g_return_val_if_fail (ORM_IS_SERIALIZABLE (serializable), FALSE);
    g_return_val_if_fail (property_name != NULL, FALSE);

    pspec = orm_serializable_find_property (serializable, property_name);
    if (pspec == NULL)
    {
        return FALSE;
    }

    g_value_init (&value, pspec->value_type);
    success = orm_serializable_deserialize_property (serializable, property_name,
                                                     &value, pspec, db_value);

    if (success)
    {
        g_object_set_property (G_OBJECT (serializable), property_name, &value);
    }

    g_value_unset (&value);
    return success;
}

/**
 * orm_serializable_get_primary_key_value:
 * @serializable: An #OrmSerializable
 *
 * Gets the primary key value of this object.
 *
 * Returns: (transfer full) (nullable): The primary key value
 */
OrmValue *
orm_serializable_get_primary_key_value (OrmSerializable *serializable)
{
    const gchar *pk_name;

    g_return_val_if_fail (ORM_IS_SERIALIZABLE (serializable), NULL);

    pk_name = orm_serializable_get_primary_key (serializable);
    return orm_serializable_get_property_value (serializable, pk_name);
}
