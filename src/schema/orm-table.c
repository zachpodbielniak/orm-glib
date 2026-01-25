/* orm-table.c
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

#include "orm-table.h"
#include "orm-column.h"
#include "orm-primary-key.h"
#include "orm-foreign-key.h"
#include "orm-index.h"
#include "orm-metadata.h"
#include "../types/orm-sql-type.h"

/*
 * OrmTable - Represents a database table schema.
 *
 * A table contains columns, constraints (primary key, foreign keys),
 * and indexes. It can be associated with a metadata container.
 */
struct _OrmTable
{
    GObject parent_instance;

    gchar         *name;
    gchar         *schema;
    OrmMetadata   *metadata;    /* Weak reference */

    GHashTable    *columns;     /* name -> OrmColumn */
    GList         *column_list; /* Ordered list of columns */

    OrmPrimaryKey *primary_key;
    GList         *foreign_keys;
    GList         *indexes;
};

G_DEFINE_TYPE (OrmTable, orm_table, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_NAME,
    PROP_SCHEMA,
    PROP_METADATA,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
orm_table_finalize (GObject *object)
{
    OrmTable *self = ORM_TABLE (object);

    g_free (self->name);
    g_free (self->schema);
    g_hash_table_unref (self->columns);
    g_list_free_full (self->column_list, g_object_unref);
    g_clear_object (&self->primary_key);
    g_list_free_full (self->foreign_keys, g_object_unref);
    g_list_free_full (self->indexes, g_object_unref);

    G_OBJECT_CLASS (orm_table_parent_class)->finalize (object);
}

static void
orm_table_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
    OrmTable *self = ORM_TABLE (object);

    switch (prop_id)
    {
    case PROP_NAME:
        g_value_set_string (value, self->name);
        break;
    case PROP_SCHEMA:
        g_value_set_string (value, self->schema);
        break;
    case PROP_METADATA:
        g_value_set_object (value, self->metadata);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_table_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
    OrmTable *self = ORM_TABLE (object);

    switch (prop_id)
    {
    case PROP_NAME:
        g_free (self->name);
        self->name = g_value_dup_string (value);
        break;
    case PROP_SCHEMA:
        g_free (self->schema);
        self->schema = g_value_dup_string (value);
        break;
    case PROP_METADATA:
        /* Weak reference */
        self->metadata = g_value_get_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_table_class_init (OrmTableClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_table_finalize;
    object_class->get_property = orm_table_get_property;
    object_class->set_property = orm_table_set_property;

    /**
     * OrmTable:name:
     *
     * The name of the table.
     */
    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "The table name",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    /**
     * OrmTable:schema:
     *
     * The schema this table belongs to (e.g., "public" in PostgreSQL).
     */
    properties[PROP_SCHEMA] =
        g_param_spec_string ("schema",
                             "Schema",
                             "The schema this table belongs to",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    /**
     * OrmTable:metadata:
     *
     * The metadata container this table belongs to.
     */
    properties[PROP_METADATA] =
        g_param_spec_object ("metadata",
                             "Metadata",
                             "The metadata container",
                             G_TYPE_OBJECT,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
orm_table_init (OrmTable *self)
{
    self->name = NULL;
    self->schema = NULL;
    self->metadata = NULL;
    self->columns = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free, NULL);
    self->column_list = NULL;
    self->primary_key = NULL;
    self->foreign_keys = NULL;
    self->indexes = NULL;
}

/**
 * orm_table_new:
 * @name: The table name
 * @metadata: (nullable): The metadata container
 *
 * Creates a new table definition.
 *
 * Returns: (transfer full): A new #OrmTable
 */
OrmTable *
orm_table_new (const gchar *name,
               OrmMetadata *metadata)
{
    g_return_val_if_fail (name != NULL, NULL);

    return g_object_new (ORM_TYPE_TABLE,
                         "name", name,
                         "metadata", metadata,
                         NULL);
}

/**
 * orm_table_get_name:
 * @self: An #OrmTable
 *
 * Gets the table name.
 *
 * Returns: (transfer none): The table name
 */
const gchar *
orm_table_get_name (OrmTable *self)
{
    g_return_val_if_fail (ORM_IS_TABLE (self), NULL);

    return self->name;
}

/**
 * orm_table_get_schema:
 * @self: An #OrmTable
 *
 * Gets the schema this table belongs to.
 *
 * Returns: (transfer none) (nullable): The schema name
 */
const gchar *
orm_table_get_schema (OrmTable *self)
{
    g_return_val_if_fail (ORM_IS_TABLE (self), NULL);

    return self->schema;
}

/**
 * orm_table_set_schema:
 * @self: An #OrmTable
 * @schema: (nullable): The schema name
 *
 * Sets the schema this table belongs to.
 */
void
orm_table_set_schema (OrmTable    *self,
                      const gchar *schema)
{
    g_return_if_fail (ORM_IS_TABLE (self));

    if (g_strcmp0 (self->schema, schema) != 0)
    {
        g_free (self->schema);
        self->schema = g_strdup (schema);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SCHEMA]);
    }
}

/**
 * orm_table_get_metadata:
 * @self: An #OrmTable
 *
 * Gets the metadata container this table belongs to.
 *
 * Returns: (transfer none) (nullable): The metadata container
 */
OrmMetadata *
orm_table_get_metadata (OrmTable *self)
{
    g_return_val_if_fail (ORM_IS_TABLE (self), NULL);

    return self->metadata;
}

/**
 * orm_table_add_column:
 * @self: An #OrmTable
 * @column: The column to add
 *
 * Adds a column to this table.
 */
void
orm_table_add_column (OrmTable  *self,
                      OrmColumn *column)
{
    const gchar *name;

    g_return_if_fail (ORM_IS_TABLE (self));
    g_return_if_fail (ORM_IS_COLUMN (column));

    name = orm_column_get_name (column);
    g_return_if_fail (name != NULL);

    /* Check for duplicate */
    if (g_hash_table_contains (self->columns, name))
    {
        g_warning ("Column '%s' already exists in table '%s'",
                   name, self->name);
        return;
    }

    /* Add to hash table and list */
    g_hash_table_insert (self->columns, g_strdup (name), column);
    self->column_list = g_list_append (self->column_list,
                                       g_object_ref (column));

    /* Set back-reference */
    orm_column_set_table (column, self);
}

/**
 * orm_table_get_column:
 * @self: An #OrmTable
 * @name: The column name
 *
 * Gets a column by name.
 *
 * Returns: (transfer none) (nullable): The column, or %NULL if not found
 */
OrmColumn *
orm_table_get_column (OrmTable    *self,
                      const gchar *name)
{
    g_return_val_if_fail (ORM_IS_TABLE (self), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_hash_table_lookup (self->columns, name);
}

/**
 * orm_table_get_columns:
 * @self: An #OrmTable
 *
 * Gets all columns in this table, in the order they were added.
 *
 * Returns: (element-type OrmColumn) (transfer none): List of columns
 */
GList *
orm_table_get_columns (OrmTable *self)
{
    g_return_val_if_fail (ORM_IS_TABLE (self), NULL);

    return self->column_list;
}

/**
 * orm_table_get_column_count:
 * @self: An #OrmTable
 *
 * Gets the number of columns in this table.
 *
 * Returns: The column count
 */
guint
orm_table_get_column_count (OrmTable *self)
{
    g_return_val_if_fail (ORM_IS_TABLE (self), 0);

    return g_hash_table_size (self->columns);
}

/**
 * orm_table_set_primary_key:
 * @self: An #OrmTable
 * @primary_key: The primary key constraint
 *
 * Sets the primary key constraint for this table.
 */
void
orm_table_set_primary_key (OrmTable      *self,
                           OrmPrimaryKey *primary_key)
{
    g_return_if_fail (ORM_IS_TABLE (self));

    g_clear_object (&self->primary_key);
    if (primary_key != NULL)
    {
        self->primary_key = g_object_ref (primary_key);
    }
}

/**
 * orm_table_get_primary_key:
 * @self: An #OrmTable
 *
 * Gets the primary key constraint.
 *
 * Returns: (transfer none) (nullable): The primary key constraint
 */
OrmPrimaryKey *
orm_table_get_primary_key (OrmTable *self)
{
    g_return_val_if_fail (ORM_IS_TABLE (self), NULL);

    return self->primary_key;
}

/**
 * orm_table_add_foreign_key:
 * @self: An #OrmTable
 * @foreign_key: The foreign key constraint
 *
 * Adds a foreign key constraint to this table.
 */
void
orm_table_add_foreign_key (OrmTable      *self,
                           OrmForeignKey *foreign_key)
{
    g_return_if_fail (ORM_IS_TABLE (self));
    g_return_if_fail (ORM_IS_FOREIGN_KEY (foreign_key));

    self->foreign_keys = g_list_append (self->foreign_keys,
                                        g_object_ref (foreign_key));
}

/**
 * orm_table_get_foreign_keys:
 * @self: An #OrmTable
 *
 * Gets all foreign key constraints.
 *
 * Returns: (element-type OrmForeignKey) (transfer none): List of foreign keys
 */
GList *
orm_table_get_foreign_keys (OrmTable *self)
{
    g_return_val_if_fail (ORM_IS_TABLE (self), NULL);

    return self->foreign_keys;
}

/**
 * orm_table_add_index:
 * @self: An #OrmTable
 * @index: The index to add
 *
 * Adds an index to this table.
 */
void
orm_table_add_index (OrmTable *self,
                     OrmIndex *index)
{
    g_return_if_fail (ORM_IS_TABLE (self));
    g_return_if_fail (ORM_IS_INDEX (index));

    self->indexes = g_list_append (self->indexes, g_object_ref (index));
}

/**
 * orm_table_get_indexes:
 * @self: An #OrmTable
 *
 * Gets all indexes on this table.
 *
 * Returns: (element-type OrmIndex) (transfer none): List of indexes
 */
GList *
orm_table_get_indexes (OrmTable *self)
{
    g_return_val_if_fail (ORM_IS_TABLE (self), NULL);

    return self->indexes;
}

/**
 * orm_table_add_column_full:
 * @self: An #OrmTable
 * @name: Column name
 * @type: SQL type
 * @primary_key: Whether this is a primary key
 * @nullable: Whether NULL values are allowed
 * @unique: Whether values must be unique
 * @autoincrement: Whether the column auto-increments
 * @default_value: (nullable): Default value expression
 *
 * Convenience function to add a column with all options.
 *
 * Returns: (transfer none): The newly added column
 */
OrmColumn *
orm_table_add_column_full (OrmTable    *self,
                           const gchar *name,
                           OrmSqlType  *type,
                           gboolean     primary_key,
                           gboolean     nullable,
                           gboolean     unique,
                           gboolean     autoincrement,
                           const gchar *default_value)
{
    g_autoptr(OrmColumn) column = NULL;

    g_return_val_if_fail (ORM_IS_TABLE (self), NULL);
    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (ORM_IS_SQL_TYPE (type), NULL);

    column = orm_column_new (name, type);
    orm_column_set_primary_key (column, primary_key);
    orm_column_set_nullable (column, nullable);
    orm_column_set_unique (column, unique);
    orm_column_set_autoincrement (column, autoincrement);
    if (default_value != NULL)
    {
        orm_column_set_default (column, default_value);
    }

    orm_table_add_column (self, column);

    return orm_table_get_column (self, name);
}

/**
 * orm_table_get_full_name:
 * @self: An #OrmTable
 *
 * Gets the fully qualified table name (schema.table).
 *
 * Returns: (transfer full): The full name, must be freed with g_free()
 */
gchar *
orm_table_get_full_name (OrmTable *self)
{
    g_return_val_if_fail (ORM_IS_TABLE (self), NULL);

    if (self->schema != NULL && self->schema[0] != '\0')
    {
        return g_strdup_printf ("%s.%s", self->schema, self->name);
    }
    else
    {
        return g_strdup (self->name);
    }
}
