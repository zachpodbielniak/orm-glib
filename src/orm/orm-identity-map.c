/* orm-identity-map.c
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

#include "orm-identity-map.h"

/*
 * OrmIdentityMap - Identity map pattern implementation.
 *
 * Tracks object identity within a session to ensure only one instance
 * of each database row exists in memory. Objects are keyed by a
 * composite key of (GType, primary_key_value).
 */

/*
 * IdentityKey - Composite key for identity map.
 */
typedef struct
{
    GType     gtype;
    OrmValue *primary_key;
} IdentityKey;

static IdentityKey *
identity_key_new (GType     gtype,
                  OrmValue *primary_key)
{
    IdentityKey *key = g_new (IdentityKey, 1);
    key->gtype = gtype;
    key->primary_key = orm_value_copy (primary_key);
    return key;
}

static void
identity_key_free (gpointer data)
{
    IdentityKey *key = data;
    if (key != NULL)
    {
        orm_value_free (key->primary_key);
        g_free (key);
    }
}

static guint
identity_key_hash (gconstpointer data)
{
    const IdentityKey *key = data;
    guint hash;

    /* Combine GType hash with primary key hash */
    hash = g_direct_hash (GSIZE_TO_POINTER (key->gtype));

    /* Hash the primary key based on its type */
    switch (orm_value_get_value_type (key->primary_key))
    {
    case ORM_VALUE_INTEGER:
        hash ^= g_int64_hash (&(gint64){orm_value_get_integer (key->primary_key)});
        break;
    case ORM_VALUE_STRING:
        hash ^= g_str_hash (orm_value_get_string (key->primary_key));
        break;
    default:
        /* For other types, use a simple combination */
        hash ^= orm_value_get_value_type (key->primary_key);
        break;
    }

    return hash;
}

static gboolean
identity_key_equal (gconstpointer a,
                    gconstpointer b)
{
    const IdentityKey *key_a = a;
    const IdentityKey *key_b = b;

    if (key_a->gtype != key_b->gtype)
    {
        return FALSE;
    }

    /* Compare primary key values */
    if (orm_value_get_value_type (key_a->primary_key) !=
        orm_value_get_value_type (key_b->primary_key))
    {
        return FALSE;
    }

    switch (orm_value_get_value_type (key_a->primary_key))
    {
    case ORM_VALUE_INTEGER:
        return orm_value_get_integer (key_a->primary_key) ==
               orm_value_get_integer (key_b->primary_key);
    case ORM_VALUE_STRING:
        return g_strcmp0 (orm_value_get_string (key_a->primary_key),
                          orm_value_get_string (key_b->primary_key)) == 0;
    case ORM_VALUE_NULL:
        return TRUE;  /* Both NULL */
    default:
        return FALSE;
    }
}

struct _OrmIdentityMap
{
    GObject parent_instance;

    /* Main storage: IdentityKey -> GObject* */
    GHashTable *objects;

    /* Reverse lookup: GObject* -> IdentityKey* (for remove_object) */
    GHashTable *object_to_key;
};

G_DEFINE_TYPE (OrmIdentityMap, orm_identity_map, G_TYPE_OBJECT)

static void
orm_identity_map_finalize (GObject *object)
{
    OrmIdentityMap *self = ORM_IDENTITY_MAP (object);

    g_clear_pointer (&self->objects, g_hash_table_unref);
    g_clear_pointer (&self->object_to_key, g_hash_table_unref);

    G_OBJECT_CLASS (orm_identity_map_parent_class)->finalize (object);
}

static void
orm_identity_map_class_init (OrmIdentityMapClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_identity_map_finalize;
}

static void
orm_identity_map_init (OrmIdentityMap *self)
{
    self->objects = g_hash_table_new_full (identity_key_hash,
                                            identity_key_equal,
                                            identity_key_free,
                                            g_object_unref);
    self->object_to_key = g_hash_table_new (g_direct_hash, g_direct_equal);
}

/**
 * orm_identity_map_new:
 *
 * Creates a new identity map.
 *
 * Returns: (transfer full): A new #OrmIdentityMap
 */
OrmIdentityMap *
orm_identity_map_new (void)
{
    return g_object_new (ORM_TYPE_IDENTITY_MAP, NULL);
}

/**
 * orm_identity_map_add:
 * @self: An #OrmIdentityMap
 * @gtype: The GType of the object
 * @primary_key: The primary key value
 * @object: The object instance
 *
 * Adds an object to the identity map.
 * The map takes a reference to the object.
 */
void
orm_identity_map_add (OrmIdentityMap *self,
                      GType           gtype,
                      OrmValue       *primary_key,
                      GObject        *object)
{
    IdentityKey *key;

    g_return_if_fail (ORM_IS_IDENTITY_MAP (self));
    g_return_if_fail (gtype != G_TYPE_NONE);
    g_return_if_fail (primary_key != NULL);
    g_return_if_fail (G_IS_OBJECT (object));

    key = identity_key_new (gtype, primary_key);

    /* Store in main hash table */
    g_hash_table_insert (self->objects, key, g_object_ref (object));

    /* Store reverse lookup (don't free key here, it's owned by objects table) */
    g_hash_table_insert (self->object_to_key, object, key);
}

/**
 * orm_identity_map_get:
 * @self: An #OrmIdentityMap
 * @gtype: The GType of the object
 * @primary_key: The primary key value
 *
 * Gets an object from the identity map.
 *
 * Returns: (transfer none) (nullable): The object, or %NULL if not found
 */
GObject *
orm_identity_map_get (OrmIdentityMap *self,
                      GType           gtype,
                      OrmValue       *primary_key)
{
    IdentityKey lookup_key;

    g_return_val_if_fail (ORM_IS_IDENTITY_MAP (self), NULL);
    g_return_val_if_fail (gtype != G_TYPE_NONE, NULL);
    g_return_val_if_fail (primary_key != NULL, NULL);

    /* Use stack-allocated key for lookup */
    lookup_key.gtype = gtype;
    lookup_key.primary_key = primary_key;

    return g_hash_table_lookup (self->objects, &lookup_key);
}

/**
 * orm_identity_map_contains:
 * @self: An #OrmIdentityMap
 * @gtype: The GType of the object
 * @primary_key: The primary key value
 *
 * Checks if an object is in the identity map.
 *
 * Returns: %TRUE if the object exists
 */
gboolean
orm_identity_map_contains (OrmIdentityMap *self,
                           GType           gtype,
                           OrmValue       *primary_key)
{
    IdentityKey lookup_key;

    g_return_val_if_fail (ORM_IS_IDENTITY_MAP (self), FALSE);
    g_return_val_if_fail (gtype != G_TYPE_NONE, FALSE);
    g_return_val_if_fail (primary_key != NULL, FALSE);

    lookup_key.gtype = gtype;
    lookup_key.primary_key = primary_key;

    return g_hash_table_contains (self->objects, &lookup_key);
}

/**
 * orm_identity_map_remove:
 * @self: An #OrmIdentityMap
 * @gtype: The GType of the object
 * @primary_key: The primary key value
 *
 * Removes an object from the identity map.
 *
 * Returns: %TRUE if the object was removed
 */
gboolean
orm_identity_map_remove (OrmIdentityMap *self,
                         GType           gtype,
                         OrmValue       *primary_key)
{
    IdentityKey lookup_key;
    GObject *object;

    g_return_val_if_fail (ORM_IS_IDENTITY_MAP (self), FALSE);
    g_return_val_if_fail (gtype != G_TYPE_NONE, FALSE);
    g_return_val_if_fail (primary_key != NULL, FALSE);

    lookup_key.gtype = gtype;
    lookup_key.primary_key = primary_key;

    /* Get the object first to remove from reverse lookup */
    object = g_hash_table_lookup (self->objects, &lookup_key);
    if (object != NULL)
    {
        g_hash_table_remove (self->object_to_key, object);
    }

    return g_hash_table_remove (self->objects, &lookup_key);
}

/**
 * orm_identity_map_remove_object:
 * @self: An #OrmIdentityMap
 * @object: The object to remove
 *
 * Removes an object from the identity map by reference.
 *
 * Returns: %TRUE if the object was removed
 */
gboolean
orm_identity_map_remove_object (OrmIdentityMap *self,
                                GObject        *object)
{
    IdentityKey *key;

    g_return_val_if_fail (ORM_IS_IDENTITY_MAP (self), FALSE);
    g_return_val_if_fail (G_IS_OBJECT (object), FALSE);

    key = g_hash_table_lookup (self->object_to_key, object);
    if (key == NULL)
    {
        return FALSE;
    }

    g_hash_table_remove (self->object_to_key, object);
    return g_hash_table_remove (self->objects, key);
}

/**
 * orm_identity_map_clear:
 * @self: An #OrmIdentityMap
 *
 * Removes all objects from the identity map.
 */
void
orm_identity_map_clear (OrmIdentityMap *self)
{
    g_return_if_fail (ORM_IS_IDENTITY_MAP (self));

    g_hash_table_remove_all (self->objects);
    g_hash_table_remove_all (self->object_to_key);
}

/*
 * Helper for collecting objects of a specific type.
 */
typedef struct
{
    GType  filter_type;
    GList *result;
} CollectData;

static void
collect_objects (gpointer key,
                 gpointer value,
                 gpointer user_data)
{
    IdentityKey *ident_key = key;
    GObject *object = value;
    CollectData *data = user_data;

    if (data->filter_type == G_TYPE_NONE ||
        g_type_is_a (ident_key->gtype, data->filter_type))
    {
        data->result = g_list_prepend (data->result, object);
    }
}

/**
 * orm_identity_map_get_all:
 * @self: An #OrmIdentityMap
 * @gtype: The GType to filter by (or G_TYPE_NONE for all)
 *
 * Gets all objects in the identity map of the given type.
 *
 * Returns: (transfer container) (element-type GObject): List of objects
 */
GList *
orm_identity_map_get_all (OrmIdentityMap *self,
                          GType           gtype)
{
    CollectData data;

    g_return_val_if_fail (ORM_IS_IDENTITY_MAP (self), NULL);

    data.filter_type = gtype;
    data.result = NULL;

    g_hash_table_foreach (self->objects, collect_objects, &data);

    return data.result;
}

/**
 * orm_identity_map_size:
 * @self: An #OrmIdentityMap
 *
 * Gets the number of objects in the identity map.
 *
 * Returns: The number of objects
 */
guint
orm_identity_map_size (OrmIdentityMap *self)
{
    g_return_val_if_fail (ORM_IS_IDENTITY_MAP (self), 0);
    return g_hash_table_size (self->objects);
}
