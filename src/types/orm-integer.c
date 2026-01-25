/* orm-integer.c
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

#include "orm-integer.h"
#include "../core/orm-enums.h"

/*
 * OrmInteger - Standard integer type (32-bit).
 * Maps to: SQLite INTEGER, PostgreSQL INTEGER, MySQL INT
 */
struct _OrmInteger
{
    OrmSqlType parent_instance;
};

G_DEFINE_TYPE (OrmInteger, orm_integer, ORM_TYPE_SQL_TYPE)

/*
 * Returns the SQL type name for the target dialect.
 */
static const gchar *
orm_integer_get_name (OrmSqlType     *self,
                      OrmDialectType  dialect_type)
{
    (void) self;

    switch (dialect_type)
    {
    case ORM_DIALECT_SQLITE:
        return "INTEGER";
    case ORM_DIALECT_POSTGRES:
        return "INTEGER";
    case ORM_DIALECT_MYSQL:
        return "INT";
    default:
        return "INTEGER";
    }
}

static void
orm_integer_class_init (OrmIntegerClass *klass)
{
    OrmSqlTypeClass *type_class = ORM_SQL_TYPE_CLASS (klass);

    type_class->get_name = orm_integer_get_name;
}

static void
orm_integer_init (OrmInteger *self)
{
    (void) self;
}

/**
 * orm_integer_new:
 *
 * Creates a new integer SQL type.
 *
 * Returns: (transfer full): A new #OrmInteger
 */
OrmInteger *
orm_integer_new (void)
{
    return g_object_new (ORM_TYPE_INTEGER, NULL);
}

/*
 * OrmBigInt - 64-bit integer type.
 * Maps to: SQLite INTEGER, PostgreSQL BIGINT, MySQL BIGINT
 */
struct _OrmBigInt
{
    OrmSqlType parent_instance;
};

G_DEFINE_TYPE (OrmBigInt, orm_bigint, ORM_TYPE_SQL_TYPE)

/*
 * Returns the SQL type name for the target dialect.
 */
static const gchar *
orm_bigint_get_name (OrmSqlType     *self,
                     OrmDialectType  dialect_type)
{
    (void) self;

    switch (dialect_type)
    {
    case ORM_DIALECT_SQLITE:
        /* SQLite uses INTEGER for all integer types */
        return "INTEGER";
    case ORM_DIALECT_POSTGRES:
        return "BIGINT";
    case ORM_DIALECT_MYSQL:
        return "BIGINT";
    default:
        return "BIGINT";
    }
}

static void
orm_bigint_class_init (OrmBigIntClass *klass)
{
    OrmSqlTypeClass *type_class = ORM_SQL_TYPE_CLASS (klass);

    type_class->get_name = orm_bigint_get_name;
}

static void
orm_bigint_init (OrmBigInt *self)
{
    (void) self;
}

/**
 * orm_bigint_new:
 *
 * Creates a new bigint SQL type for 64-bit integers.
 *
 * Returns: (transfer full): A new #OrmBigInt
 */
OrmBigInt *
orm_bigint_new (void)
{
    return g_object_new (ORM_TYPE_BIGINT, NULL);
}
