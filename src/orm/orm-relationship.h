/* orm-relationship.h
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

#ifndef ORM_RELATIONSHIP_H
#define ORM_RELATIONSHIP_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-enums.h"

G_BEGIN_DECLS

#define ORM_TYPE_RELATIONSHIP (orm_relationship_get_type ())

G_DECLARE_FINAL_TYPE (OrmRelationship, orm_relationship, ORM, RELATIONSHIP, GObject)

/* OrmRelationshipType is defined in orm-enums.h */

/**
 * OrmLoadStrategy:
 * @ORM_LOAD_LAZY: Load related objects on first access
 * @ORM_LOAD_EAGER: Load related objects immediately with parent
 * @ORM_LOAD_SELECT: Load with separate SELECT (subquery)
 * @ORM_LOAD_JOIN: Load with JOIN (single query)
 *
 * Strategies for loading related objects.
 */
typedef enum {
    ORM_LOAD_LAZY,
    ORM_LOAD_EAGER,
    ORM_LOAD_SELECT,
    ORM_LOAD_JOIN
} OrmLoadStrategy;

GType orm_load_strategy_get_type (void) G_GNUC_CONST;

#define ORM_TYPE_LOAD_STRATEGY (orm_load_strategy_get_type ())

/**
 * OrmCascade:
 * @ORM_CASCADE_NONE: No cascade behavior
 * @ORM_CASCADE_SAVE: Cascade save/update operations
 * @ORM_CASCADE_DELETE: Cascade delete operations
 * @ORM_CASCADE_ALL: Cascade all operations
 *
 * Cascade behavior for relationship operations.
 */
typedef enum {
    ORM_CASCADE_NONE   = 0,
    ORM_CASCADE_SAVE   = 1 << 0,
    ORM_CASCADE_DELETE = 1 << 1,
    ORM_CASCADE_ALL    = ORM_CASCADE_SAVE | ORM_CASCADE_DELETE
} OrmCascade;

GType orm_cascade_get_type (void) G_GNUC_CONST;

#define ORM_TYPE_CASCADE (orm_cascade_get_type ())

/**
 * OrmRelationship:
 *
 * Defines a relationship between two entity types.
 * Specifies how objects are linked via foreign keys and how
 * related objects should be loaded and cascaded.
 */

/*
 * orm_relationship_new:
 * @name: The relationship name (property name on parent)
 * @rel_type: The type of relationship
 * @source_type: The source entity GType
 * @target_type: The target entity GType
 *
 * Creates a new relationship.
 *
 * Returns: (transfer full): A new #OrmRelationship
 */
OrmRelationship *   orm_relationship_new                (const gchar          *name,
                                                         OrmRelationshipType   rel_type,
                                                         GType                 source_type,
                                                         GType                 target_type);

/*
 * orm_relationship_get_name:
 * @self: An #OrmRelationship
 *
 * Gets the relationship name.
 *
 * Returns: (transfer none): The relationship name
 */
const gchar *       orm_relationship_get_name           (OrmRelationship *self);

/*
 * orm_relationship_get_relationship_type:
 * @self: An #OrmRelationship
 *
 * Gets the type of relationship.
 *
 * Returns: The relationship type
 */
OrmRelationshipType orm_relationship_get_relationship_type (OrmRelationship *self);

/*
 * orm_relationship_get_source_type:
 * @self: An #OrmRelationship
 *
 * Gets the source entity GType.
 *
 * Returns: The source GType
 */
GType               orm_relationship_get_source_type    (OrmRelationship *self);

/*
 * orm_relationship_get_target_type:
 * @self: An #OrmRelationship
 *
 * Gets the target entity GType.
 *
 * Returns: The target GType
 */
GType               orm_relationship_get_target_type    (OrmRelationship *self);

/*
 * orm_relationship_set_foreign_key:
 * @self: An #OrmRelationship
 * @local_column: The local column name
 * @remote_column: The remote column name
 *
 * Sets the foreign key columns for this relationship.
 */
void                orm_relationship_set_foreign_key    (OrmRelationship *self,
                                                         const gchar     *local_column,
                                                         const gchar     *remote_column);

/*
 * orm_relationship_get_local_column:
 * @self: An #OrmRelationship
 *
 * Gets the local foreign key column.
 *
 * Returns: (transfer none) (nullable): The local column name
 */
const gchar *       orm_relationship_get_local_column   (OrmRelationship *self);

/*
 * orm_relationship_get_remote_column:
 * @self: An #OrmRelationship
 *
 * Gets the remote foreign key column.
 *
 * Returns: (transfer none) (nullable): The remote column name
 */
const gchar *       orm_relationship_get_remote_column  (OrmRelationship *self);

/*
 * orm_relationship_set_join_table:
 * @self: An #OrmRelationship
 * @table_name: The join table name
 * @source_column: The column referencing source
 * @target_column: The column referencing target
 *
 * Sets the join table for many-to-many relationships.
 */
void                orm_relationship_set_join_table     (OrmRelationship *self,
                                                         const gchar     *table_name,
                                                         const gchar     *source_column,
                                                         const gchar     *target_column);

/*
 * orm_relationship_get_join_table:
 * @self: An #OrmRelationship
 *
 * Gets the join table name for many-to-many relationships.
 *
 * Returns: (transfer none) (nullable): The join table name
 */
const gchar *       orm_relationship_get_join_table     (OrmRelationship *self);

/*
 * orm_relationship_set_load_strategy:
 * @self: An #OrmRelationship
 * @strategy: The load strategy
 *
 * Sets how related objects should be loaded.
 */
void                orm_relationship_set_load_strategy  (OrmRelationship *self,
                                                         OrmLoadStrategy  strategy);

/*
 * orm_relationship_get_load_strategy:
 * @self: An #OrmRelationship
 *
 * Gets the load strategy.
 *
 * Returns: The load strategy
 */
OrmLoadStrategy     orm_relationship_get_load_strategy  (OrmRelationship *self);

/*
 * orm_relationship_set_cascade:
 * @self: An #OrmRelationship
 * @cascade: The cascade flags
 *
 * Sets the cascade behavior.
 */
void                orm_relationship_set_cascade        (OrmRelationship *self,
                                                         OrmCascade       cascade);

/*
 * orm_relationship_get_cascade:
 * @self: An #OrmRelationship
 *
 * Gets the cascade behavior.
 *
 * Returns: The cascade flags
 */
OrmCascade          orm_relationship_get_cascade        (OrmRelationship *self);

/*
 * orm_relationship_set_back_populates:
 * @self: An #OrmRelationship
 * @property_name: The property on the related object
 *
 * Sets the back-reference property name for bidirectional relationships.
 */
void                orm_relationship_set_back_populates (OrmRelationship *self,
                                                         const gchar     *property_name);

/*
 * orm_relationship_get_back_populates:
 * @self: An #OrmRelationship
 *
 * Gets the back-reference property name.
 *
 * Returns: (transfer none) (nullable): The property name
 */
const gchar *       orm_relationship_get_back_populates (OrmRelationship *self);

G_END_DECLS

#endif /* ORM_RELATIONSHIP_H */
