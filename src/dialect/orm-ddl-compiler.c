/* orm-ddl-compiler.c
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

#include "orm-ddl-compiler.h"

/*
 * OrmDdlCompiler interface implementation.
 *
 * Provides default method wrappers for the DDL compiler interface.
 * Concrete implementations (SQLite, PostgreSQL, MySQL) provide the
 * actual DDL generation logic.
 */

G_DEFINE_INTERFACE (OrmDdlCompiler, orm_ddl_compiler, G_TYPE_OBJECT)

static void
orm_ddl_compiler_default_init (OrmDdlCompilerInterface *iface)
{
    (void) iface;
}

/**
 * orm_ddl_compiler_compile_create_table:
 * @self: An #OrmDdlCompiler
 * @table: The table definition
 * @if_not_exists: Whether to add IF NOT EXISTS clause
 *
 * Generates a CREATE TABLE statement for the given table.
 *
 * Returns: (transfer full): The SQL CREATE TABLE statement
 */
gchar *
orm_ddl_compiler_compile_create_table (OrmDdlCompiler *self,
                                       OrmTable       *table,
                                       gboolean        if_not_exists)
{
    OrmDdlCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_DDL_COMPILER (self), NULL);
    g_return_val_if_fail (ORM_IS_TABLE (table), NULL);

    iface = ORM_DDL_COMPILER_GET_IFACE (self);
    g_return_val_if_fail (iface->compile_create_table != NULL, NULL);

    return iface->compile_create_table (self, table, if_not_exists);
}

/**
 * orm_ddl_compiler_compile_drop_table:
 * @self: An #OrmDdlCompiler
 * @table: The table to drop
 * @if_exists: Whether to add IF EXISTS clause
 * @cascade: Whether to add CASCADE clause
 *
 * Generates a DROP TABLE statement for the given table.
 *
 * Returns: (transfer full): The SQL DROP TABLE statement
 */
gchar *
orm_ddl_compiler_compile_drop_table (OrmDdlCompiler *self,
                                     OrmTable       *table,
                                     gboolean        if_exists,
                                     gboolean        cascade)
{
    OrmDdlCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_DDL_COMPILER (self), NULL);
    g_return_val_if_fail (ORM_IS_TABLE (table), NULL);

    iface = ORM_DDL_COMPILER_GET_IFACE (self);
    g_return_val_if_fail (iface->compile_drop_table != NULL, NULL);

    return iface->compile_drop_table (self, table, if_exists, cascade);
}

/**
 * orm_ddl_compiler_compile_column_definition:
 * @self: An #OrmDdlCompiler
 * @column: The column to define
 *
 * Generates the column definition clause (name, type, constraints).
 *
 * Returns: (transfer full): The column definition SQL
 */
gchar *
orm_ddl_compiler_compile_column_definition (OrmDdlCompiler *self,
                                            OrmColumn      *column)
{
    OrmDdlCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_DDL_COMPILER (self), NULL);
    g_return_val_if_fail (ORM_IS_COLUMN (column), NULL);

    iface = ORM_DDL_COMPILER_GET_IFACE (self);
    g_return_val_if_fail (iface->compile_column_definition != NULL, NULL);

    return iface->compile_column_definition (self, column);
}

/**
 * orm_ddl_compiler_compile_primary_key:
 * @self: An #OrmDdlCompiler
 * @pk: The primary key constraint
 *
 * Generates the PRIMARY KEY constraint clause.
 *
 * Returns: (transfer full): The PRIMARY KEY SQL
 */
gchar *
orm_ddl_compiler_compile_primary_key (OrmDdlCompiler *self,
                                      OrmPrimaryKey  *pk)
{
    OrmDdlCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_DDL_COMPILER (self), NULL);
    g_return_val_if_fail (ORM_IS_PRIMARY_KEY (pk), NULL);

    iface = ORM_DDL_COMPILER_GET_IFACE (self);
    g_return_val_if_fail (iface->compile_primary_key != NULL, NULL);

    return iface->compile_primary_key (self, pk);
}

/**
 * orm_ddl_compiler_compile_foreign_key:
 * @self: An #OrmDdlCompiler
 * @fk: The foreign key constraint
 *
 * Generates the FOREIGN KEY constraint clause.
 *
 * Returns: (transfer full): The FOREIGN KEY SQL
 */
gchar *
orm_ddl_compiler_compile_foreign_key (OrmDdlCompiler *self,
                                      OrmForeignKey  *fk)
{
    OrmDdlCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_DDL_COMPILER (self), NULL);
    g_return_val_if_fail (ORM_IS_FOREIGN_KEY (fk), NULL);

    iface = ORM_DDL_COMPILER_GET_IFACE (self);
    g_return_val_if_fail (iface->compile_foreign_key != NULL, NULL);

    return iface->compile_foreign_key (self, fk);
}

/**
 * orm_ddl_compiler_compile_create_index:
 * @self: An #OrmDdlCompiler
 * @index: The index definition
 * @table: The table the index belongs to
 * @if_not_exists: Whether to add IF NOT EXISTS clause
 *
 * Generates a CREATE INDEX statement.
 *
 * Returns: (transfer full): The CREATE INDEX SQL
 */
gchar *
orm_ddl_compiler_compile_create_index (OrmDdlCompiler *self,
                                       OrmIndex       *index,
                                       OrmTable       *table,
                                       gboolean        if_not_exists)
{
    OrmDdlCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_DDL_COMPILER (self), NULL);
    g_return_val_if_fail (ORM_IS_INDEX (index), NULL);
    g_return_val_if_fail (ORM_IS_TABLE (table), NULL);

    iface = ORM_DDL_COMPILER_GET_IFACE (self);
    g_return_val_if_fail (iface->compile_create_index != NULL, NULL);

    return iface->compile_create_index (self, index, table, if_not_exists);
}

/**
 * orm_ddl_compiler_compile_drop_index:
 * @self: An #OrmDdlCompiler
 * @index: The index to drop
 * @table: The table the index belongs to
 * @if_exists: Whether to add IF EXISTS clause
 *
 * Generates a DROP INDEX statement.
 *
 * Returns: (transfer full): The DROP INDEX SQL
 */
gchar *
orm_ddl_compiler_compile_drop_index (OrmDdlCompiler *self,
                                     OrmIndex       *index,
                                     OrmTable       *table,
                                     gboolean        if_exists)
{
    OrmDdlCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_DDL_COMPILER (self), NULL);
    g_return_val_if_fail (ORM_IS_INDEX (index), NULL);
    g_return_val_if_fail (ORM_IS_TABLE (table), NULL);

    iface = ORM_DDL_COMPILER_GET_IFACE (self);
    g_return_val_if_fail (iface->compile_drop_index != NULL, NULL);

    return iface->compile_drop_index (self, index, table, if_exists);
}
