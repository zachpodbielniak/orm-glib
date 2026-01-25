/* orm-mysql-type-compiler.c
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

#include "orm-mysql-type-compiler.h"
#include "../../types/orm-integer.h"
#include "../../types/orm-string.h"
#include "../../types/orm-text.h"
#include "../../types/orm-boolean.h"
#include "../../types/orm-float.h"
#include "../../types/orm-datetime.h"
#include "../../types/orm-blob.h"

/*
 * OrmMysqlTypeCompiler - MySQL type compiler implementation.
 *
 * MySQL has a rich type system:
 * - INT/INTEGER: 32-bit signed integer
 * - BIGINT: 64-bit signed integer
 * - SMALLINT: 16-bit signed integer
 * - TINYINT: 8-bit signed integer (TINYINT(1) for boolean)
 * - VARCHAR(n): Variable-length string (1-65535)
 * - TEXT: Variable-length string (up to 65535 bytes)
 * - FLOAT: 32-bit floating point
 * - DOUBLE: 64-bit floating point
 * - DATETIME: Date and time without timezone
 * - BLOB: Binary data (up to 65535 bytes)
 */
struct _OrmMysqlTypeCompiler
{
    GObject parent_instance;
};

static void orm_mysql_type_compiler_iface_init (OrmTypeCompilerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (OrmMysqlTypeCompiler, orm_mysql_type_compiler, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_TYPE_COMPILER,
                                                orm_mysql_type_compiler_iface_init))

/*
 * Compile a generic OrmSqlType to its MySQL representation.
 */
static gchar *
orm_mysql_type_compiler_compile_type (OrmTypeCompiler *compiler,
                                       OrmSqlType      *type)
{
    (void) compiler;

    /* Use the type's own get_name with MySQL dialect */
    return g_strdup (orm_sql_type_get_name (type, ORM_DIALECT_MYSQL));
}

static const gchar *
orm_mysql_type_compiler_compile_integer (OrmTypeCompiler *self)
{
    (void) self;
    return "INT";
}

static const gchar *
orm_mysql_type_compiler_compile_bigint (OrmTypeCompiler *self)
{
    (void) self;
    return "BIGINT";
}

static gchar *
orm_mysql_type_compiler_compile_string (OrmTypeCompiler *self,
                                         guint            length)
{
    (void) self;

    if (length == 0)
    {
        /* No length specified, default to VARCHAR(255) */
        return g_strdup ("VARCHAR(255)");
    }

    if (length > 65535)
    {
        /* MySQL VARCHAR max is 65535, use TEXT for longer */
        return g_strdup ("TEXT");
    }

    return g_strdup_printf ("VARCHAR(%u)", length);
}

static const gchar *
orm_mysql_type_compiler_compile_text (OrmTypeCompiler *self)
{
    (void) self;
    return "TEXT";
}

static const gchar *
orm_mysql_type_compiler_compile_boolean (OrmTypeCompiler *self)
{
    (void) self;
    /* MySQL uses TINYINT(1) for boolean values */
    return "TINYINT(1)";
}

static const gchar *
orm_mysql_type_compiler_compile_float (OrmTypeCompiler *self)
{
    (void) self;
    return "FLOAT";
}

static const gchar *
orm_mysql_type_compiler_compile_double (OrmTypeCompiler *self)
{
    (void) self;
    return "DOUBLE";
}

static const gchar *
orm_mysql_type_compiler_compile_datetime (OrmTypeCompiler *self)
{
    (void) self;
    /* Use DATETIME for date/time storage */
    return "DATETIME";
}

static const gchar *
orm_mysql_type_compiler_compile_blob (OrmTypeCompiler *self)
{
    (void) self;
    return "BLOB";
}

static void
orm_mysql_type_compiler_iface_init (OrmTypeCompilerInterface *iface)
{
    iface->compile_type = orm_mysql_type_compiler_compile_type;
    iface->compile_integer = orm_mysql_type_compiler_compile_integer;
    iface->compile_bigint = orm_mysql_type_compiler_compile_bigint;
    iface->compile_string = orm_mysql_type_compiler_compile_string;
    iface->compile_text = orm_mysql_type_compiler_compile_text;
    iface->compile_boolean = orm_mysql_type_compiler_compile_boolean;
    iface->compile_float = orm_mysql_type_compiler_compile_float;
    iface->compile_double = orm_mysql_type_compiler_compile_double;
    iface->compile_datetime = orm_mysql_type_compiler_compile_datetime;
    iface->compile_blob = orm_mysql_type_compiler_compile_blob;
}

static void
orm_mysql_type_compiler_class_init (OrmMysqlTypeCompilerClass *klass)
{
    (void) klass;
}

static void
orm_mysql_type_compiler_init (OrmMysqlTypeCompiler *self)
{
    (void) self;
}

/**
 * orm_mysql_type_compiler_new:
 *
 * Creates a new MySQL type compiler.
 *
 * Returns: (transfer full): A new #OrmMysqlTypeCompiler
 */
OrmMysqlTypeCompiler *
orm_mysql_type_compiler_new (void)
{
    return g_object_new (ORM_TYPE_MYSQL_TYPE_COMPILER, NULL);
}
