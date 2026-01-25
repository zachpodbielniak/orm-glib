/* orm-postgres-type-compiler.c
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

#include "orm-postgres-type-compiler.h"
#include "../../types/orm-integer.h"
#include "../../types/orm-string.h"
#include "../../types/orm-text.h"
#include "../../types/orm-boolean.h"
#include "../../types/orm-float.h"
#include "../../types/orm-datetime.h"
#include "../../types/orm-blob.h"

/*
 * OrmPostgresTypeCompiler - PostgreSQL type compiler implementation.
 *
 * PostgreSQL has a rich type system with strong typing:
 * - INTEGER: 32-bit signed integer (-2147483648 to 2147483647)
 * - BIGINT: 64-bit signed integer
 * - SMALLINT: 16-bit signed integer
 * - BOOLEAN: Native true/false type
 * - VARCHAR(n): Variable-length string with max length
 * - TEXT: Unlimited variable-length string
 * - REAL: 32-bit IEEE floating point (6 decimal digits precision)
 * - DOUBLE PRECISION: 64-bit IEEE floating point (15 decimal digits)
 * - TIMESTAMP: Date and time without timezone
 * - TIMESTAMPTZ: Date and time with timezone
 * - BYTEA: Binary data (byte array)
 */
struct _OrmPostgresTypeCompiler
{
    GObject parent_instance;
};

static void orm_postgres_type_compiler_iface_init (OrmTypeCompilerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (OrmPostgresTypeCompiler, orm_postgres_type_compiler, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_TYPE_COMPILER,
                                                orm_postgres_type_compiler_iface_init))

/*
 * Compile a generic OrmSqlType to its PostgreSQL representation.
 */
static gchar *
orm_postgres_type_compiler_compile_type (OrmTypeCompiler *compiler,
                                          OrmSqlType      *type)
{
    (void) compiler;

    /* Use the type's own get_name with PostgreSQL dialect */
    return g_strdup (orm_sql_type_get_name (type, ORM_DIALECT_POSTGRES));
}

static const gchar *
orm_postgres_type_compiler_compile_integer (OrmTypeCompiler *self)
{
    (void) self;
    return "INTEGER";
}

static const gchar *
orm_postgres_type_compiler_compile_bigint (OrmTypeCompiler *self)
{
    (void) self;
    return "BIGINT";
}

static gchar *
orm_postgres_type_compiler_compile_string (OrmTypeCompiler *self,
                                            guint            length)
{
    (void) self;

    if (length == 0)
    {
        /* No length specified, use TEXT */
        return g_strdup ("TEXT");
    }

    return g_strdup_printf ("VARCHAR(%u)", length);
}

static const gchar *
orm_postgres_type_compiler_compile_text (OrmTypeCompiler *self)
{
    (void) self;
    return "TEXT";
}

static const gchar *
orm_postgres_type_compiler_compile_boolean (OrmTypeCompiler *self)
{
    (void) self;
    /* PostgreSQL has native BOOLEAN type */
    return "BOOLEAN";
}

static const gchar *
orm_postgres_type_compiler_compile_float (OrmTypeCompiler *self)
{
    (void) self;
    return "REAL";
}

static const gchar *
orm_postgres_type_compiler_compile_double (OrmTypeCompiler *self)
{
    (void) self;
    return "DOUBLE PRECISION";
}

static const gchar *
orm_postgres_type_compiler_compile_datetime (OrmTypeCompiler *self)
{
    (void) self;
    /* Use TIMESTAMP WITH TIME ZONE for datetime storage */
    return "TIMESTAMPTZ";
}

static const gchar *
orm_postgres_type_compiler_compile_blob (OrmTypeCompiler *self)
{
    (void) self;
    /* PostgreSQL uses BYTEA for binary data */
    return "BYTEA";
}

static void
orm_postgres_type_compiler_iface_init (OrmTypeCompilerInterface *iface)
{
    iface->compile_type = orm_postgres_type_compiler_compile_type;
    iface->compile_integer = orm_postgres_type_compiler_compile_integer;
    iface->compile_bigint = orm_postgres_type_compiler_compile_bigint;
    iface->compile_string = orm_postgres_type_compiler_compile_string;
    iface->compile_text = orm_postgres_type_compiler_compile_text;
    iface->compile_boolean = orm_postgres_type_compiler_compile_boolean;
    iface->compile_float = orm_postgres_type_compiler_compile_float;
    iface->compile_double = orm_postgres_type_compiler_compile_double;
    iface->compile_datetime = orm_postgres_type_compiler_compile_datetime;
    iface->compile_blob = orm_postgres_type_compiler_compile_blob;
}

static void
orm_postgres_type_compiler_class_init (OrmPostgresTypeCompilerClass *klass)
{
    (void) klass;
}

static void
orm_postgres_type_compiler_init (OrmPostgresTypeCompiler *self)
{
    (void) self;
}

/**
 * orm_postgres_type_compiler_new:
 *
 * Creates a new PostgreSQL type compiler.
 *
 * Returns: (transfer full): A new #OrmPostgresTypeCompiler
 */
OrmPostgresTypeCompiler *
orm_postgres_type_compiler_new (void)
{
    return g_object_new (ORM_TYPE_POSTGRES_TYPE_COMPILER, NULL);
}
