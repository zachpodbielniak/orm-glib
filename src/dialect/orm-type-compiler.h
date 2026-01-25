/* orm-type-compiler.h
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

#ifndef ORM_TYPE_COMPILER_H
#define ORM_TYPE_COMPILER_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../types/orm-sql-type.h"

G_BEGIN_DECLS

#define ORM_TYPE_TYPE_COMPILER (orm_type_compiler_get_type ())

G_DECLARE_INTERFACE (OrmTypeCompiler, orm_type_compiler, ORM, TYPE_COMPILER, GObject)

/**
 * OrmTypeCompilerInterface:
 * @g_iface: Parent interface
 * @compile_type: Compile an OrmSqlType to SQL type string
 * @compile_integer: Compile integer type
 * @compile_bigint: Compile big integer type
 * @compile_string: Compile string/varchar type
 * @compile_text: Compile text type
 * @compile_boolean: Compile boolean type
 * @compile_float: Compile float type
 * @compile_double: Compile double type
 * @compile_datetime: Compile datetime type
 * @compile_blob: Compile blob/binary type
 *
 * Interface for compiling ORM types to SQL type strings.
 * Each dialect provides its own implementation to handle
 * database-specific type names and syntax.
 */
struct _OrmTypeCompilerInterface
{
    GTypeInterface g_iface;

    /* Generic type compilation */
    gchar *         (*compile_type)     (OrmTypeCompiler *self,
                                         OrmSqlType      *type);

    /* Specific type compilation methods */
    const gchar *   (*compile_integer)  (OrmTypeCompiler *self);
    const gchar *   (*compile_bigint)   (OrmTypeCompiler *self);
    gchar *         (*compile_string)   (OrmTypeCompiler *self,
                                         guint            length);
    const gchar *   (*compile_text)     (OrmTypeCompiler *self);
    const gchar *   (*compile_boolean)  (OrmTypeCompiler *self);
    const gchar *   (*compile_float)    (OrmTypeCompiler *self);
    const gchar *   (*compile_double)   (OrmTypeCompiler *self);
    const gchar *   (*compile_datetime) (OrmTypeCompiler *self);
    const gchar *   (*compile_blob)     (OrmTypeCompiler *self);

    /* Reserved for future expansion */
    gpointer _reserved[8];
};

/* Generic compilation */
gchar *         orm_type_compiler_compile_type      (OrmTypeCompiler *self,
                                                     OrmSqlType      *type);

/* Specific type compilation */
const gchar *   orm_type_compiler_compile_integer   (OrmTypeCompiler *self);
const gchar *   orm_type_compiler_compile_bigint    (OrmTypeCompiler *self);
gchar *         orm_type_compiler_compile_string    (OrmTypeCompiler *self,
                                                     guint            length);
const gchar *   orm_type_compiler_compile_text      (OrmTypeCompiler *self);
const gchar *   orm_type_compiler_compile_boolean   (OrmTypeCompiler *self);
const gchar *   orm_type_compiler_compile_float     (OrmTypeCompiler *self);
const gchar *   orm_type_compiler_compile_double    (OrmTypeCompiler *self);
const gchar *   orm_type_compiler_compile_datetime  (OrmTypeCompiler *self);
const gchar *   orm_type_compiler_compile_blob      (OrmTypeCompiler *self);

G_END_DECLS

#endif /* ORM_TYPE_COMPILER_H */
