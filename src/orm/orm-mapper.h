/* orm-mapper.h
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

#ifndef ORM_MAPPER_H
#define ORM_MAPPER_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-property.h"
#include "orm-relationship.h"
#include "../schema/orm-table.h"

G_BEGIN_DECLS

#define ORM_TYPE_MAPPER (orm_mapper_get_type ())

G_DECLARE_FINAL_TYPE (OrmMapper, orm_mapper, ORM, MAPPER, GObject)

/**
 * OrmMapper:
 *
 * Maps a GType to a database table.
 * Contains property mappings and relationship definitions.
 * Used to generate table schemas and convert between objects and rows.
 */

/**
 * orm_mapper_new:
 * @gtype: The GType to map
 * @table_name: (nullable): The table name (defaults to lowercase type name)
 *
 * Creates a new mapper for the given GType.
 *
 * Returns: (transfer full): A new #OrmMapper
 */
OrmMapper *         orm_mapper_new                      (GType        gtype,
                                                         const gchar *table_name);

/**
 * orm_mapper_new_from_serializable:
 * @gtype: A GType implementing OrmSerializable
 *
 * Creates a mapper by introspecting an OrmSerializable type.
 * Automatically maps all serializable properties.
 *
 * Returns: (transfer full): A new #OrmMapper
 */
OrmMapper *         orm_mapper_new_from_serializable    (GType gtype);

/**
 * orm_mapper_get_gtype:
 * @self: An #OrmMapper
 *
 * Gets the GType this mapper handles.
 *
 * Returns: The GType
 */
GType               orm_mapper_get_gtype                (OrmMapper *self);

/**
 * orm_mapper_get_table_name:
 * @self: An #OrmMapper
 *
 * Gets the database table name.
 *
 * Returns: (transfer none): The table name
 */
const gchar *       orm_mapper_get_table_name           (OrmMapper *self);

/**
 * orm_mapper_add_property:
 * @self: An #OrmMapper
 * @property: (transfer none): The property mapping to add
 *
 * Adds a property mapping.
 */
void                orm_mapper_add_property             (OrmMapper   *self,
                                                         OrmProperty *property);

/**
 * orm_mapper_get_property:
 * @self: An #OrmMapper
 * @property_name: The property name
 *
 * Gets a property mapping by name.
 *
 * Returns: (transfer none) (nullable): The property mapping
 */
OrmProperty *       orm_mapper_get_property             (OrmMapper   *self,
                                                         const gchar *property_name);

/**
 * orm_mapper_get_property_by_column:
 * @self: An #OrmMapper
 * @column_name: The column name
 *
 * Gets a property mapping by column name.
 *
 * Returns: (transfer none) (nullable): The property mapping
 */
OrmProperty *       orm_mapper_get_property_by_column   (OrmMapper   *self,
                                                         const gchar *column_name);

/**
 * orm_mapper_get_properties:
 * @self: An #OrmMapper
 *
 * Gets all property mappings.
 *
 * Returns: (transfer none) (element-type OrmProperty): List of properties
 */
GList *             orm_mapper_get_properties           (OrmMapper *self);

/**
 * orm_mapper_get_primary_key_property:
 * @self: An #OrmMapper
 *
 * Gets the primary key property mapping.
 *
 * Returns: (transfer none) (nullable): The primary key property
 */
OrmProperty *       orm_mapper_get_primary_key_property (OrmMapper *self);

/**
 * orm_mapper_add_relationship:
 * @self: An #OrmMapper
 * @relationship: (transfer none): The relationship to add
 *
 * Adds a relationship definition.
 */
void                orm_mapper_add_relationship         (OrmMapper       *self,
                                                         OrmRelationship *relationship);

/**
 * orm_mapper_get_relationship:
 * @self: An #OrmMapper
 * @name: The relationship name
 *
 * Gets a relationship by name.
 *
 * Returns: (transfer none) (nullable): The relationship
 */
OrmRelationship *   orm_mapper_get_relationship         (OrmMapper   *self,
                                                         const gchar *name);

/**
 * orm_mapper_get_relationships:
 * @self: An #OrmMapper
 *
 * Gets all relationships.
 *
 * Returns: (transfer none) (element-type OrmRelationship): List of relationships
 */
GList *             orm_mapper_get_relationships        (OrmMapper *self);

/**
 * orm_mapper_to_table:
 * @self: An #OrmMapper
 *
 * Creates an OrmTable schema from this mapper.
 *
 * Returns: (transfer full): A new #OrmTable
 */
OrmTable *          orm_mapper_to_table                 (OrmMapper *self);

/**
 * orm_mapper_get_column_names:
 * @self: An #OrmMapper
 *
 * Gets all column names in insertion order.
 *
 * Returns: (transfer full) (element-type utf8): List of column names
 */
GList *             orm_mapper_get_column_names         (OrmMapper *self);

/**
 * orm_mapper_get_insert_columns:
 * @self: An #OrmMapper
 *
 * Gets column names for INSERT (excludes auto-increment).
 *
 * Returns: (transfer full) (element-type utf8): List of column names
 */
GList *             orm_mapper_get_insert_columns       (OrmMapper *self);

G_END_DECLS

#endif /* ORM_MAPPER_H */
