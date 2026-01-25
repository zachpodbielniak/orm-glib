/* orm-ddl-compiler.h
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

#ifndef ORM_DDL_COMPILER_H
#define ORM_DDL_COMPILER_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../schema/orm-table.h"
#include "../schema/orm-column.h"
#include "../schema/orm-primary-key.h"
#include "../schema/orm-foreign-key.h"
#include "../schema/orm-index.h"

G_BEGIN_DECLS

#define ORM_TYPE_DDL_COMPILER (orm_ddl_compiler_get_type ())

G_DECLARE_INTERFACE (OrmDdlCompiler, orm_ddl_compiler, ORM, DDL_COMPILER, GObject)

/**
 * OrmDdlCompilerInterface:
 * @g_iface: Parent interface
 * @compile_create_table: Generate CREATE TABLE statement
 * @compile_drop_table: Generate DROP TABLE statement
 * @compile_column_definition: Generate column definition clause
 * @compile_primary_key: Generate PRIMARY KEY constraint
 * @compile_foreign_key: Generate FOREIGN KEY constraint
 * @compile_create_index: Generate CREATE INDEX statement
 * @compile_drop_index: Generate DROP INDEX statement
 *
 * Interface for compiling DDL (Data Definition Language) statements.
 * Each dialect implements this to handle database-specific DDL syntax.
 */
struct _OrmDdlCompilerInterface
{
    GTypeInterface g_iface;

    /* Table operations */
    gchar *         (*compile_create_table)     (OrmDdlCompiler *self,
                                                 OrmTable       *table,
                                                 gboolean        if_not_exists);
    gchar *         (*compile_drop_table)       (OrmDdlCompiler *self,
                                                 OrmTable       *table,
                                                 gboolean        if_exists,
                                                 gboolean        cascade);

    /* Column definition */
    gchar *         (*compile_column_definition)(OrmDdlCompiler *self,
                                                 OrmColumn      *column);

    /* Constraints */
    gchar *         (*compile_primary_key)      (OrmDdlCompiler *self,
                                                 OrmPrimaryKey  *pk);
    gchar *         (*compile_foreign_key)      (OrmDdlCompiler *self,
                                                 OrmForeignKey  *fk);

    /* Index operations */
    gchar *         (*compile_create_index)     (OrmDdlCompiler *self,
                                                 OrmIndex       *index,
                                                 OrmTable       *table,
                                                 gboolean        if_not_exists);
    gchar *         (*compile_drop_index)       (OrmDdlCompiler *self,
                                                 OrmIndex       *index,
                                                 OrmTable       *table,
                                                 gboolean        if_exists);

    /* Reserved for future expansion */
    gpointer _reserved[8];
};

/* Table operations */
gchar *     orm_ddl_compiler_compile_create_table       (OrmDdlCompiler *self,
                                                         OrmTable       *table,
                                                         gboolean        if_not_exists);
gchar *     orm_ddl_compiler_compile_drop_table         (OrmDdlCompiler *self,
                                                         OrmTable       *table,
                                                         gboolean        if_exists,
                                                         gboolean        cascade);

/* Column definition */
gchar *     orm_ddl_compiler_compile_column_definition  (OrmDdlCompiler *self,
                                                         OrmColumn      *column);

/* Constraints */
gchar *     orm_ddl_compiler_compile_primary_key        (OrmDdlCompiler *self,
                                                         OrmPrimaryKey  *pk);
gchar *     orm_ddl_compiler_compile_foreign_key        (OrmDdlCompiler *self,
                                                         OrmForeignKey  *fk);

/* Index operations */
gchar *     orm_ddl_compiler_compile_create_index       (OrmDdlCompiler *self,
                                                         OrmIndex       *index,
                                                         OrmTable       *table,
                                                         gboolean        if_not_exists);
gchar *     orm_ddl_compiler_compile_drop_index         (OrmDdlCompiler *self,
                                                         OrmIndex       *index,
                                                         OrmTable       *table,
                                                         gboolean        if_exists);

G_END_DECLS

#endif /* ORM_DDL_COMPILER_H */
