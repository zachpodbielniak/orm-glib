/* orm-relationship.c
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

#include "orm-relationship.h"

/*
 * OrmRelationship - Entity relationship definition.
 *
 * Defines relationships between entity types, including foreign key
 * configuration, loading strategy, and cascade behavior.
 */

struct _OrmRelationship
{
    GObject parent_instance;

    gchar               *name;
    OrmRelationshipType  rel_type;
    GType                source_type;
    GType                target_type;

    /* Foreign key configuration */
    gchar               *local_column;
    gchar               *remote_column;

    /* Many-to-many join table */
    gchar               *join_table;
    gchar               *join_source_column;
    gchar               *join_target_column;

    /* Loading and cascade options */
    OrmLoadStrategy      load_strategy;
    OrmCascade           cascade;

    /* Back-reference for bidirectional */
    gchar               *back_populates;
};

G_DEFINE_TYPE (OrmRelationship, orm_relationship, G_TYPE_OBJECT)

static void
orm_relationship_finalize (GObject *object)
{
    OrmRelationship *self = ORM_RELATIONSHIP (object);

    g_clear_pointer (&self->name, g_free);
    g_clear_pointer (&self->local_column, g_free);
    g_clear_pointer (&self->remote_column, g_free);
    g_clear_pointer (&self->join_table, g_free);
    g_clear_pointer (&self->join_source_column, g_free);
    g_clear_pointer (&self->join_target_column, g_free);
    g_clear_pointer (&self->back_populates, g_free);

    G_OBJECT_CLASS (orm_relationship_parent_class)->finalize (object);
}

static void
orm_relationship_class_init (OrmRelationshipClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_relationship_finalize;
}

static void
orm_relationship_init (OrmRelationship *self)
{
    self->name = NULL;
    self->rel_type = ORM_RELATIONSHIP_MANY_TO_ONE;
    self->source_type = G_TYPE_NONE;
    self->target_type = G_TYPE_NONE;
    self->local_column = NULL;
    self->remote_column = NULL;
    self->join_table = NULL;
    self->join_source_column = NULL;
    self->join_target_column = NULL;
    self->load_strategy = ORM_LOAD_LAZY;
    self->cascade = ORM_CASCADE_NONE;
    self->back_populates = NULL;
}

/**
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
OrmRelationship *
orm_relationship_new (const gchar          *name,
                      OrmRelationshipType   rel_type,
                      GType                 source_type,
                      GType                 target_type)
{
    OrmRelationship *self;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (source_type != G_TYPE_NONE, NULL);
    g_return_val_if_fail (target_type != G_TYPE_NONE, NULL);

    self = g_object_new (ORM_TYPE_RELATIONSHIP, NULL);
    self->name = g_strdup (name);
    self->rel_type = rel_type;
    self->source_type = source_type;
    self->target_type = target_type;

    return self;
}

/**
 * orm_relationship_get_name:
 * @self: An #OrmRelationship
 *
 * Gets the relationship name.
 *
 * Returns: (transfer none): The relationship name
 */
const gchar *
orm_relationship_get_name (OrmRelationship *self)
{
    g_return_val_if_fail (ORM_IS_RELATIONSHIP (self), NULL);
    return self->name;
}

/**
 * orm_relationship_get_relationship_type:
 * @self: An #OrmRelationship
 *
 * Gets the type of relationship.
 *
 * Returns: The relationship type
 */
OrmRelationshipType
orm_relationship_get_relationship_type (OrmRelationship *self)
{
    g_return_val_if_fail (ORM_IS_RELATIONSHIP (self), ORM_RELATIONSHIP_MANY_TO_ONE);
    return self->rel_type;
}

/**
 * orm_relationship_get_source_type:
 * @self: An #OrmRelationship
 *
 * Gets the source entity GType.
 *
 * Returns: The source GType
 */
GType
orm_relationship_get_source_type (OrmRelationship *self)
{
    g_return_val_if_fail (ORM_IS_RELATIONSHIP (self), G_TYPE_NONE);
    return self->source_type;
}

/**
 * orm_relationship_get_target_type:
 * @self: An #OrmRelationship
 *
 * Gets the target entity GType.
 *
 * Returns: The target GType
 */
GType
orm_relationship_get_target_type (OrmRelationship *self)
{
    g_return_val_if_fail (ORM_IS_RELATIONSHIP (self), G_TYPE_NONE);
    return self->target_type;
}

/**
 * orm_relationship_set_foreign_key:
 * @self: An #OrmRelationship
 * @local_column: The local column name
 * @remote_column: The remote column name
 *
 * Sets the foreign key columns for this relationship.
 */
void
orm_relationship_set_foreign_key (OrmRelationship *self,
                                  const gchar     *local_column,
                                  const gchar     *remote_column)
{
    g_return_if_fail (ORM_IS_RELATIONSHIP (self));

    g_free (self->local_column);
    g_free (self->remote_column);

    self->local_column = g_strdup (local_column);
    self->remote_column = g_strdup (remote_column);
}

/**
 * orm_relationship_get_local_column:
 * @self: An #OrmRelationship
 *
 * Gets the local foreign key column.
 *
 * Returns: (transfer none) (nullable): The local column name
 */
const gchar *
orm_relationship_get_local_column (OrmRelationship *self)
{
    g_return_val_if_fail (ORM_IS_RELATIONSHIP (self), NULL);
    return self->local_column;
}

/**
 * orm_relationship_get_remote_column:
 * @self: An #OrmRelationship
 *
 * Gets the remote foreign key column.
 *
 * Returns: (transfer none) (nullable): The remote column name
 */
const gchar *
orm_relationship_get_remote_column (OrmRelationship *self)
{
    g_return_val_if_fail (ORM_IS_RELATIONSHIP (self), NULL);
    return self->remote_column;
}

/**
 * orm_relationship_set_join_table:
 * @self: An #OrmRelationship
 * @table_name: The join table name
 * @source_column: The column referencing source
 * @target_column: The column referencing target
 *
 * Sets the join table for many-to-many relationships.
 */
void
orm_relationship_set_join_table (OrmRelationship *self,
                                 const gchar     *table_name,
                                 const gchar     *source_column,
                                 const gchar     *target_column)
{
    g_return_if_fail (ORM_IS_RELATIONSHIP (self));

    g_free (self->join_table);
    g_free (self->join_source_column);
    g_free (self->join_target_column);

    self->join_table = g_strdup (table_name);
    self->join_source_column = g_strdup (source_column);
    self->join_target_column = g_strdup (target_column);
}

/**
 * orm_relationship_get_join_table:
 * @self: An #OrmRelationship
 *
 * Gets the join table name for many-to-many relationships.
 *
 * Returns: (transfer none) (nullable): The join table name
 */
const gchar *
orm_relationship_get_join_table (OrmRelationship *self)
{
    g_return_val_if_fail (ORM_IS_RELATIONSHIP (self), NULL);
    return self->join_table;
}

/**
 * orm_relationship_set_load_strategy:
 * @self: An #OrmRelationship
 * @strategy: The load strategy
 *
 * Sets how related objects should be loaded.
 */
void
orm_relationship_set_load_strategy (OrmRelationship *self,
                                    OrmLoadStrategy  strategy)
{
    g_return_if_fail (ORM_IS_RELATIONSHIP (self));
    self->load_strategy = strategy;
}

/**
 * orm_relationship_get_load_strategy:
 * @self: An #OrmRelationship
 *
 * Gets the load strategy.
 *
 * Returns: The load strategy
 */
OrmLoadStrategy
orm_relationship_get_load_strategy (OrmRelationship *self)
{
    g_return_val_if_fail (ORM_IS_RELATIONSHIP (self), ORM_LOAD_LAZY);
    return self->load_strategy;
}

/**
 * orm_relationship_set_cascade:
 * @self: An #OrmRelationship
 * @cascade: The cascade flags
 *
 * Sets the cascade behavior.
 */
void
orm_relationship_set_cascade (OrmRelationship *self,
                              OrmCascade       cascade)
{
    g_return_if_fail (ORM_IS_RELATIONSHIP (self));
    self->cascade = cascade;
}

/**
 * orm_relationship_get_cascade:
 * @self: An #OrmRelationship
 *
 * Gets the cascade behavior.
 *
 * Returns: The cascade flags
 */
OrmCascade
orm_relationship_get_cascade (OrmRelationship *self)
{
    g_return_val_if_fail (ORM_IS_RELATIONSHIP (self), ORM_CASCADE_NONE);
    return self->cascade;
}

/**
 * orm_relationship_set_back_populates:
 * @self: An #OrmRelationship
 * @property_name: The property on the related object
 *
 * Sets the back-reference property name for bidirectional relationships.
 */
void
orm_relationship_set_back_populates (OrmRelationship *self,
                                     const gchar     *property_name)
{
    g_return_if_fail (ORM_IS_RELATIONSHIP (self));

    g_free (self->back_populates);
    self->back_populates = g_strdup (property_name);
}

/**
 * orm_relationship_get_back_populates:
 * @self: An #OrmRelationship
 *
 * Gets the back-reference property name.
 *
 * Returns: (transfer none) (nullable): The property name
 */
const gchar *
orm_relationship_get_back_populates (OrmRelationship *self)
{
    g_return_val_if_fail (ORM_IS_RELATIONSHIP (self), NULL);
    return self->back_populates;
}
