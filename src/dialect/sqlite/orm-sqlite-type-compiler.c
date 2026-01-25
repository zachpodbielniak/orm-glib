/* orm-sqlite-type-compiler.c
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

#include "orm-sqlite-type-compiler.h"
#include "../../types/orm-integer.h"
#include "../../types/orm-string.h"
#include "../../types/orm-text.h"
#include "../../types/orm-boolean.h"
#include "../../types/orm-float.h"
#include "../../types/orm-datetime.h"
#include "../../types/orm-blob.h"

/*
 * OrmSqliteTypeCompiler - SQLite type compiler implementation.
 *
 * SQLite has very loose type affinity. We use explicit type names
 * for clarity, but SQLite will apply affinity rules regardless.
 *
 * Type affinity mappings:
 * - INTEGER affinity: for integers, booleans
 * - TEXT affinity: for strings, text, datetime (stored as ISO8601)
 * - REAL affinity: for floats, doubles
 * - BLOB affinity: for binary data
 */
struct _OrmSqliteTypeCompiler
{
    GObject parent_instance;
};

static void orm_sqlite_type_compiler_iface_init (OrmTypeCompilerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (OrmSqliteTypeCompiler, orm_sqlite_type_compiler, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_TYPE_COMPILER,
                                                orm_sqlite_type_compiler_iface_init))

/*
 * Compile a generic OrmSqlType to its SQLite representation.
 * Dispatches to specific type handlers based on the type class.
 */
static gchar *
orm_sqlite_type_compiler_compile_type (OrmTypeCompiler *compiler,
                                       OrmSqlType      *type)
{
    (void) compiler;

    /* Use the type's own get_name with SQLite dialect */
    return g_strdup (orm_sql_type_get_name (type, ORM_DIALECT_SQLITE));
}

static const gchar *
orm_sqlite_type_compiler_compile_integer (OrmTypeCompiler *self)
{
    (void) self;
    return "INTEGER";
}

static const gchar *
orm_sqlite_type_compiler_compile_bigint (OrmTypeCompiler *self)
{
    (void) self;
    /* SQLite uses INTEGER for all integer types (64-bit by default) */
    return "INTEGER";
}

static gchar *
orm_sqlite_type_compiler_compile_string (OrmTypeCompiler *self,
                                         guint            length)
{
    (void) self;
    (void) length;
    /* SQLite ignores length constraints, always uses TEXT */
    return g_strdup ("TEXT");
}

static const gchar *
orm_sqlite_type_compiler_compile_text (OrmTypeCompiler *self)
{
    (void) self;
    return "TEXT";
}

static const gchar *
orm_sqlite_type_compiler_compile_boolean (OrmTypeCompiler *self)
{
    (void) self;
    /* SQLite has no native boolean, use INTEGER (0/1) */
    return "INTEGER";
}

static const gchar *
orm_sqlite_type_compiler_compile_float (OrmTypeCompiler *self)
{
    (void) self;
    return "REAL";
}

static const gchar *
orm_sqlite_type_compiler_compile_double (OrmTypeCompiler *self)
{
    (void) self;
    return "REAL";
}

static const gchar *
orm_sqlite_type_compiler_compile_datetime (OrmTypeCompiler *self)
{
    (void) self;
    /* SQLite stores datetime as TEXT in ISO8601 format */
    return "TEXT";
}

static const gchar *
orm_sqlite_type_compiler_compile_blob (OrmTypeCompiler *self)
{
    (void) self;
    return "BLOB";
}

static void
orm_sqlite_type_compiler_iface_init (OrmTypeCompilerInterface *iface)
{
    iface->compile_type = orm_sqlite_type_compiler_compile_type;
    iface->compile_integer = orm_sqlite_type_compiler_compile_integer;
    iface->compile_bigint = orm_sqlite_type_compiler_compile_bigint;
    iface->compile_string = orm_sqlite_type_compiler_compile_string;
    iface->compile_text = orm_sqlite_type_compiler_compile_text;
    iface->compile_boolean = orm_sqlite_type_compiler_compile_boolean;
    iface->compile_float = orm_sqlite_type_compiler_compile_float;
    iface->compile_double = orm_sqlite_type_compiler_compile_double;
    iface->compile_datetime = orm_sqlite_type_compiler_compile_datetime;
    iface->compile_blob = orm_sqlite_type_compiler_compile_blob;
}

static void
orm_sqlite_type_compiler_class_init (OrmSqliteTypeCompilerClass *klass)
{
    (void) klass;
}

static void
orm_sqlite_type_compiler_init (OrmSqliteTypeCompiler *self)
{
    (void) self;
}

/**
 * orm_sqlite_type_compiler_new:
 *
 * Creates a new SQLite type compiler.
 *
 * Returns: (transfer full): A new #OrmSqliteTypeCompiler
 */
OrmSqliteTypeCompiler *
orm_sqlite_type_compiler_new (void)
{
    return g_object_new (ORM_TYPE_SQLITE_TYPE_COMPILER, NULL);
}
