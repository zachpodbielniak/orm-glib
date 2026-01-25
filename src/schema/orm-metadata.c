/* orm-metadata.c
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

#include "orm-metadata.h"
#include "orm-table.h"

/*
 * OrmMetadata - Container for table definitions.
 *
 * Metadata serves as a registry for all table definitions in an
 * application. It can be used to generate DDL statements for creating
 * or dropping tables.
 */
struct _OrmMetadata
{
    GObject parent_instance;

    GHashTable *tables;     /* name -> OrmTable */
    GList      *table_list; /* Ordered list of tables */
};

G_DEFINE_TYPE (OrmMetadata, orm_metadata, G_TYPE_OBJECT)

static void
orm_metadata_finalize (GObject *object)
{
    OrmMetadata *self = ORM_METADATA (object);

    g_hash_table_unref (self->tables);
    g_list_free_full (self->table_list, g_object_unref);

    G_OBJECT_CLASS (orm_metadata_parent_class)->finalize (object);
}

static void
orm_metadata_class_init (OrmMetadataClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_metadata_finalize;
}

static void
orm_metadata_init (OrmMetadata *self)
{
    self->tables = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          g_free, NULL);
    self->table_list = NULL;
}

/**
 * orm_metadata_new:
 *
 * Creates a new metadata container for table definitions.
 *
 * Returns: (transfer full): A new #OrmMetadata
 */
OrmMetadata *
orm_metadata_new (void)
{
    return g_object_new (ORM_TYPE_METADATA, NULL);
}

/**
 * orm_metadata_add_table:
 * @self: An #OrmMetadata
 * @table: The table to add
 *
 * Adds a table definition to the metadata container.
 */
void
orm_metadata_add_table (OrmMetadata *self,
                        OrmTable    *table)
{
    const gchar *name;

    g_return_if_fail (ORM_IS_METADATA (self));
    g_return_if_fail (ORM_IS_TABLE (table));

    name = orm_table_get_name (table);
    g_return_if_fail (name != NULL);

    /* Check for duplicate */
    if (g_hash_table_contains (self->tables, name))
    {
        g_warning ("Table '%s' already exists in metadata", name);
        return;
    }

    /* Add to hash table and list */
    g_hash_table_insert (self->tables, g_strdup (name), table);
    self->table_list = g_list_append (self->table_list, g_object_ref (table));
}

/**
 * orm_metadata_get_table:
 * @self: An #OrmMetadata
 * @name: The table name
 *
 * Gets a table by name.
 *
 * Returns: (transfer none) (nullable): The table, or %NULL if not found
 */
OrmTable *
orm_metadata_get_table (OrmMetadata *self,
                        const gchar *name)
{
    g_return_val_if_fail (ORM_IS_METADATA (self), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_hash_table_lookup (self->tables, name);
}

/**
 * orm_metadata_get_tables:
 * @self: An #OrmMetadata
 *
 * Gets all tables in this metadata container, in the order they were added.
 *
 * Returns: (element-type OrmTable) (transfer none): List of tables
 */
GList *
orm_metadata_get_tables (OrmMetadata *self)
{
    g_return_val_if_fail (ORM_IS_METADATA (self), NULL);

    return self->table_list;
}

/**
 * orm_metadata_get_table_count:
 * @self: An #OrmMetadata
 *
 * Gets the number of tables in this metadata container.
 *
 * Returns: The table count
 */
guint
orm_metadata_get_table_count (OrmMetadata *self)
{
    g_return_val_if_fail (ORM_IS_METADATA (self), 0);

    return g_hash_table_size (self->tables);
}

/**
 * orm_metadata_has_table:
 * @self: An #OrmMetadata
 * @name: The table name
 *
 * Checks if a table exists in this metadata container.
 *
 * Returns: %TRUE if the table exists
 */
gboolean
orm_metadata_has_table (OrmMetadata *self,
                        const gchar *name)
{
    g_return_val_if_fail (ORM_IS_METADATA (self), FALSE);
    g_return_val_if_fail (name != NULL, FALSE);

    return g_hash_table_contains (self->tables, name);
}

/**
 * orm_metadata_remove_table:
 * @self: An #OrmMetadata
 * @name: The table name
 *
 * Removes a table from this metadata container.
 */
void
orm_metadata_remove_table (OrmMetadata *self,
                           const gchar *name)
{
    OrmTable *table;
    GList *link;

    g_return_if_fail (ORM_IS_METADATA (self));
    g_return_if_fail (name != NULL);

    table = g_hash_table_lookup (self->tables, name);
    if (table == NULL)
    {
        return;
    }

    /* Remove from list */
    link = g_list_find (self->table_list, table);
    if (link != NULL)
    {
        g_object_unref (link->data);
        self->table_list = g_list_delete_link (self->table_list, link);
    }

    /* Remove from hash table */
    g_hash_table_remove (self->tables, name);
}

/**
 * orm_metadata_create_all:
 * @self: An #OrmMetadata
 * @connection: The database connection
 * @error: Return location for a #GError
 *
 * Creates all tables in the database. Tables are created in the order
 * they were added to handle foreign key dependencies.
 *
 * Note: This is a placeholder that will be fully implemented when
 * the engine layer is complete.
 */
void
orm_metadata_create_all (OrmMetadata   *self,
                         OrmConnection *connection,
                         GError       **error)
{
    g_return_if_fail (ORM_IS_METADATA (self));
    g_return_if_fail (connection != NULL);

    /* TODO: Implement when engine layer is complete */
    (void) error;
    g_warning ("orm_metadata_create_all: Not yet implemented");
}

/**
 * orm_metadata_drop_all:
 * @self: An #OrmMetadata
 * @connection: The database connection
 * @error: Return location for a #GError
 *
 * Drops all tables in the database. Tables are dropped in reverse order
 * to handle foreign key dependencies.
 *
 * Note: This is a placeholder that will be fully implemented when
 * the engine layer is complete.
 */
void
orm_metadata_drop_all (OrmMetadata   *self,
                       OrmConnection *connection,
                       GError       **error)
{
    g_return_if_fail (ORM_IS_METADATA (self));
    g_return_if_fail (connection != NULL);

    /* TODO: Implement when engine layer is complete */
    (void) error;
    g_warning ("orm_metadata_drop_all: Not yet implemented");
}

/**
 * orm_metadata_reflect:
 * @self: An #OrmMetadata
 * @connection: The database connection
 * @error: Return location for a #GError
 *
 * Loads table definitions from the database schema. This is useful
 * for working with existing databases.
 *
 * Note: This is a placeholder that will be fully implemented when
 * the engine layer is complete.
 */
void
orm_metadata_reflect (OrmMetadata   *self,
                      OrmConnection *connection,
                      GError       **error)
{
    g_return_if_fail (ORM_IS_METADATA (self));
    g_return_if_fail (connection != NULL);

    /* TODO: Implement when engine layer is complete */
    (void) error;
    g_warning ("orm_metadata_reflect: Not yet implemented");
}

/**
 * orm_metadata_clear:
 * @self: An #OrmMetadata
 *
 * Removes all table definitions from this metadata container.
 */
void
orm_metadata_clear (OrmMetadata *self)
{
    g_return_if_fail (ORM_IS_METADATA (self));

    g_hash_table_remove_all (self->tables);
    g_list_free_full (self->table_list, g_object_unref);
    self->table_list = NULL;
}
