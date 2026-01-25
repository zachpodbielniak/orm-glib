/* orm-type-compiler.c
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

#include "orm-type-compiler.h"

/*
 * OrmTypeCompiler interface implementation.
 *
 * Provides default method wrappers for the type compiler interface.
 * Concrete implementations (SQLite, PostgreSQL, MySQL) provide the
 * actual type-to-SQL mappings.
 */

G_DEFINE_INTERFACE (OrmTypeCompiler, orm_type_compiler, G_TYPE_OBJECT)

static void
orm_type_compiler_default_init (OrmTypeCompilerInterface *iface)
{
    (void) iface;
}

/**
 * orm_type_compiler_compile_type:
 * @self: An #OrmTypeCompiler
 * @type: The SQL type to compile
 *
 * Compiles an #OrmSqlType to its SQL representation string.
 *
 * Returns: (transfer full): The SQL type string
 */
gchar *
orm_type_compiler_compile_type (OrmTypeCompiler *self,
                                OrmSqlType      *type)
{
    OrmTypeCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_TYPE_COMPILER (self), NULL);
    g_return_val_if_fail (ORM_IS_SQL_TYPE (type), NULL);

    iface = ORM_TYPE_COMPILER_GET_IFACE (self);
    g_return_val_if_fail (iface->compile_type != NULL, NULL);

    return iface->compile_type (self, type);
}

/**
 * orm_type_compiler_compile_integer:
 * @self: An #OrmTypeCompiler
 *
 * Gets the SQL type name for integers.
 *
 * Returns: (transfer none): The SQL integer type name
 */
const gchar *
orm_type_compiler_compile_integer (OrmTypeCompiler *self)
{
    OrmTypeCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_TYPE_COMPILER (self), "INTEGER");

    iface = ORM_TYPE_COMPILER_GET_IFACE (self);
    if (iface->compile_integer == NULL)
    {
        return "INTEGER";
    }

    return iface->compile_integer (self);
}

/**
 * orm_type_compiler_compile_bigint:
 * @self: An #OrmTypeCompiler
 *
 * Gets the SQL type name for big integers.
 *
 * Returns: (transfer none): The SQL big integer type name
 */
const gchar *
orm_type_compiler_compile_bigint (OrmTypeCompiler *self)
{
    OrmTypeCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_TYPE_COMPILER (self), "BIGINT");

    iface = ORM_TYPE_COMPILER_GET_IFACE (self);
    if (iface->compile_bigint == NULL)
    {
        return "BIGINT";
    }

    return iface->compile_bigint (self);
}

/**
 * orm_type_compiler_compile_string:
 * @self: An #OrmTypeCompiler
 * @length: Maximum string length (0 for unlimited)
 *
 * Gets the SQL type name for variable-length strings.
 *
 * Returns: (transfer full): The SQL string type name (e.g., "VARCHAR(255)")
 */
gchar *
orm_type_compiler_compile_string (OrmTypeCompiler *self,
                                  guint            length)
{
    OrmTypeCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_TYPE_COMPILER (self), g_strdup ("VARCHAR"));

    iface = ORM_TYPE_COMPILER_GET_IFACE (self);
    if (iface->compile_string == NULL)
    {
        if (length > 0)
        {
            return g_strdup_printf ("VARCHAR(%u)", length);
        }
        return g_strdup ("VARCHAR");
    }

    return iface->compile_string (self, length);
}

/**
 * orm_type_compiler_compile_text:
 * @self: An #OrmTypeCompiler
 *
 * Gets the SQL type name for unlimited text.
 *
 * Returns: (transfer none): The SQL text type name
 */
const gchar *
orm_type_compiler_compile_text (OrmTypeCompiler *self)
{
    OrmTypeCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_TYPE_COMPILER (self), "TEXT");

    iface = ORM_TYPE_COMPILER_GET_IFACE (self);
    if (iface->compile_text == NULL)
    {
        return "TEXT";
    }

    return iface->compile_text (self);
}

/**
 * orm_type_compiler_compile_boolean:
 * @self: An #OrmTypeCompiler
 *
 * Gets the SQL type name for booleans.
 *
 * Returns: (transfer none): The SQL boolean type name
 */
const gchar *
orm_type_compiler_compile_boolean (OrmTypeCompiler *self)
{
    OrmTypeCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_TYPE_COMPILER (self), "BOOLEAN");

    iface = ORM_TYPE_COMPILER_GET_IFACE (self);
    if (iface->compile_boolean == NULL)
    {
        return "BOOLEAN";
    }

    return iface->compile_boolean (self);
}

/**
 * orm_type_compiler_compile_float:
 * @self: An #OrmTypeCompiler
 *
 * Gets the SQL type name for single-precision floats.
 *
 * Returns: (transfer none): The SQL float type name
 */
const gchar *
orm_type_compiler_compile_float (OrmTypeCompiler *self)
{
    OrmTypeCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_TYPE_COMPILER (self), "REAL");

    iface = ORM_TYPE_COMPILER_GET_IFACE (self);
    if (iface->compile_float == NULL)
    {
        return "REAL";
    }

    return iface->compile_float (self);
}

/**
 * orm_type_compiler_compile_double:
 * @self: An #OrmTypeCompiler
 *
 * Gets the SQL type name for double-precision floats.
 *
 * Returns: (transfer none): The SQL double type name
 */
const gchar *
orm_type_compiler_compile_double (OrmTypeCompiler *self)
{
    OrmTypeCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_TYPE_COMPILER (self), "DOUBLE PRECISION");

    iface = ORM_TYPE_COMPILER_GET_IFACE (self);
    if (iface->compile_double == NULL)
    {
        return "DOUBLE PRECISION";
    }

    return iface->compile_double (self);
}

/**
 * orm_type_compiler_compile_datetime:
 * @self: An #OrmTypeCompiler
 *
 * Gets the SQL type name for datetime values.
 *
 * Returns: (transfer none): The SQL datetime type name
 */
const gchar *
orm_type_compiler_compile_datetime (OrmTypeCompiler *self)
{
    OrmTypeCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_TYPE_COMPILER (self), "TIMESTAMP");

    iface = ORM_TYPE_COMPILER_GET_IFACE (self);
    if (iface->compile_datetime == NULL)
    {
        return "TIMESTAMP";
    }

    return iface->compile_datetime (self);
}

/**
 * orm_type_compiler_compile_blob:
 * @self: An #OrmTypeCompiler
 *
 * Gets the SQL type name for binary data.
 *
 * Returns: (transfer none): The SQL blob type name
 */
const gchar *
orm_type_compiler_compile_blob (OrmTypeCompiler *self)
{
    OrmTypeCompilerInterface *iface;

    g_return_val_if_fail (ORM_IS_TYPE_COMPILER (self), "BLOB");

    iface = ORM_TYPE_COMPILER_GET_IFACE (self);
    if (iface->compile_blob == NULL)
    {
        return "BLOB";
    }

    return iface->compile_blob (self);
}
