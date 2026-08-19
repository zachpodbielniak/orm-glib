/* orm-serializable.h
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

#ifndef ORM_SERIALIZABLE_H
#define ORM_SERIALIZABLE_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-types.h"

G_BEGIN_DECLS

#define ORM_TYPE_SERIALIZABLE (orm_serializable_get_type ())

G_DECLARE_INTERFACE (OrmSerializable, orm_serializable, ORM, SERIALIZABLE, GObject)

/**
 * OrmSerializableInterface:
 * @g_iface: Parent interface
 * @serialize_property: Serialize a property to a database value
 * @deserialize_property: Deserialize a database value to a property
 * @find_property: Find a property by name
 * @list_properties: List all serializable properties
 * @get_table_name: Get the database table name
 * @get_primary_key: Get the primary key property name
 *
 * Interface for GObjects that can be serialized to and from a database.
 * This is modeled after the YamlSerializable interface pattern.
 *
 * Implementors should override the virtual methods to customize
 * serialization behavior. Default implementations are provided that
 * use GObject property introspection.
 */
struct _OrmSerializableInterface
{
    GTypeInterface g_iface;

    /**
     * OrmSerializableInterface::serialize_property:
     * @serializable: The serializable object
     * @property_name: The property name
     * @value: The GValue containing the property value
     * @pspec: The property specification
     *
     * Converts a GObject property value to an OrmValue for database storage.
     * The default implementation uses the GValue type to create an appropriate
     * OrmValue.
     *
     * Returns: (transfer full) (nullable): The serialized value, or %NULL
     *          if the property should not be serialized
     */
    OrmValue *      (*serialize_property)   (OrmSerializable *serializable,
                                             const gchar     *property_name,
                                             const GValue    *value,
                                             GParamSpec      *pspec);

    /**
     * OrmSerializableInterface::deserialize_property:
     * @serializable: The serializable object
     * @property_name: The property name
     * @value: (out): The GValue to fill
     * @pspec: The property specification
     * @db_value: The database value to deserialize
     *
     * Converts a database value to a GObject property value.
     * The default implementation uses the GValue type to extract
     * the appropriate value from the OrmValue.
     *
     * Returns: %TRUE if deserialization succeeded, %FALSE otherwise
     */
    gboolean        (*deserialize_property) (OrmSerializable *serializable,
                                             const gchar     *property_name,
                                             GValue          *value,
                                             GParamSpec      *pspec,
                                             OrmValue        *db_value);

    /**
     * OrmSerializableInterface::find_property:
     * @serializable: The serializable object
     * @name: The property name to find
     *
     * Finds a property by name. The default implementation uses
     * g_object_class_find_property().
     *
     * Returns: (transfer none) (nullable): The property specification,
     *          or %NULL if not found
     */
    GParamSpec *    (*find_property)        (OrmSerializable *serializable,
                                             const gchar     *name);

    /**
     * OrmSerializableInterface::list_properties:
     * @serializable: The serializable object
     * @n_pspecs: (out): Location to store the number of properties
     *
     * Lists all serializable properties. The default implementation uses
     * g_object_class_list_properties() and filters out non-serializable
     * properties.
     *
     * Returns: (array length=n_pspecs) (transfer container): Array of
     *          property specifications. Free with g_free().
     */
    GParamSpec **   (*list_properties)      (OrmSerializable *serializable,
                                             guint           *n_pspecs);

    /**
     * OrmSerializableInterface::get_table_name:
     * @serializable: The serializable object
     *
     * Gets the database table name for this object type. The default
     * implementation uses the GType name in lowercase with underscores.
     *
     * Returns: (transfer none): The table name
     */
    const gchar *   (*get_table_name)       (OrmSerializable *serializable);

    /**
     * OrmSerializableInterface::get_primary_key:
     * @serializable: The serializable object
     *
     * Gets the property name of the primary key. The default implementation
     * returns "id".
     *
     * Returns: (transfer none): The primary key property name
     */
    const gchar *   (*get_primary_key)      (OrmSerializable *serializable);

    /* Reserved for future expansion */
    gpointer _reserved[8];
};

/* Interface methods */
OrmValue *      orm_serializable_serialize_property     (OrmSerializable *serializable,
                                                         const gchar     *property_name,
                                                         const GValue    *value,
                                                         GParamSpec      *pspec);

gboolean        orm_serializable_deserialize_property   (OrmSerializable *serializable,
                                                         const gchar     *property_name,
                                                         GValue          *value,
                                                         GParamSpec      *pspec,
                                                         OrmValue        *db_value);

GParamSpec *    orm_serializable_find_property          (OrmSerializable *serializable,
                                                         const gchar     *name);

GParamSpec **   orm_serializable_list_properties        (OrmSerializable *serializable,
                                                         guint           *n_pspecs);

const gchar *   orm_serializable_get_table_name         (OrmSerializable *serializable);

const gchar *   orm_serializable_get_primary_key        (OrmSerializable *serializable);

/* Convenience functions */

/*
 * orm_serializable_get_property_value:
 * @serializable: The serializable object
 * @property_name: The property name
 *
 * Gets a property value as an OrmValue.
 *
 * Returns: (transfer full) (nullable): The property value as OrmValue
 */
OrmValue *      orm_serializable_get_property_value     (OrmSerializable *serializable,
                                                         const gchar     *property_name);

/*
 * orm_serializable_set_property_value:
 * @serializable: The serializable object
 * @property_name: The property name
 * @db_value: The value to set
 *
 * Sets a property value from an OrmValue.
 *
 * Returns: %TRUE if the property was set successfully
 */
gboolean        orm_serializable_set_property_value     (OrmSerializable *serializable,
                                                         const gchar     *property_name,
                                                         OrmValue        *db_value);

/*
 * orm_serializable_get_primary_key_value:
 * @serializable: The serializable object
 *
 * Gets the primary key value of this object.
 *
 * Returns: (transfer full) (nullable): The primary key value
 */
OrmValue *      orm_serializable_get_primary_key_value  (OrmSerializable *serializable);

G_END_DECLS

#endif /* ORM_SERIALIZABLE_H */
