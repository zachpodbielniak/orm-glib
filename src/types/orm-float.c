/* orm-float.c
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

#include "orm-float.h"
#include "../core/orm-enums.h"

/*
 * OrmFloatType - Single precision floating point type.
 * Maps to: SQLite REAL, PostgreSQL REAL, MySQL FLOAT
 */
struct _OrmFloatType
{
    OrmSqlType parent_instance;
};

G_DEFINE_TYPE (OrmFloatType, orm_float_type, ORM_TYPE_SQL_TYPE)

/*
 * Returns the SQL type name for the target dialect.
 */
static const gchar *
orm_float_type_get_name (OrmSqlType     *self,
                         OrmDialectType  dialect_type)
{
    (void) self;

    switch (dialect_type)
    {
    case ORM_DIALECT_SQLITE:
        return "REAL";
    case ORM_DIALECT_POSTGRES:
        return "REAL";
    case ORM_DIALECT_MYSQL:
        return "FLOAT";
    default:
        return "FLOAT";
    }
}

static void
orm_float_type_class_init (OrmFloatTypeClass *klass)
{
    OrmSqlTypeClass *type_class = ORM_SQL_TYPE_CLASS (klass);

    type_class->get_name = orm_float_type_get_name;
}

static void
orm_float_type_init (OrmFloatType *self)
{
    (void) self;
}

/**
 * orm_float_type_new:
 *
 * Creates a new single-precision float SQL type.
 *
 * Returns: (transfer full): A new #OrmFloatType
 */
OrmFloatType *
orm_float_type_new (void)
{
    return g_object_new (ORM_TYPE_FLOAT_TYPE, NULL);
}

/*
 * OrmDoubleType - Double precision floating point type.
 * Maps to: SQLite REAL, PostgreSQL DOUBLE PRECISION, MySQL DOUBLE
 */
struct _OrmDoubleType
{
    OrmSqlType parent_instance;
};

G_DEFINE_TYPE (OrmDoubleType, orm_double_type, ORM_TYPE_SQL_TYPE)

/*
 * Returns the SQL type name for the target dialect.
 */
static const gchar *
orm_double_type_get_name (OrmSqlType     *self,
                          OrmDialectType  dialect_type)
{
    (void) self;

    switch (dialect_type)
    {
    case ORM_DIALECT_SQLITE:
        return "REAL";
    case ORM_DIALECT_POSTGRES:
        return "DOUBLE PRECISION";
    case ORM_DIALECT_MYSQL:
        return "DOUBLE";
    default:
        return "DOUBLE";
    }
}

static void
orm_double_type_class_init (OrmDoubleTypeClass *klass)
{
    OrmSqlTypeClass *type_class = ORM_SQL_TYPE_CLASS (klass);

    type_class->get_name = orm_double_type_get_name;
}

static void
orm_double_type_init (OrmDoubleType *self)
{
    (void) self;
}

/**
 * orm_double_type_new:
 *
 * Creates a new double-precision float SQL type.
 *
 * Returns: (transfer full): A new #OrmDoubleType
 */
OrmDoubleType *
orm_double_type_new (void)
{
    return g_object_new (ORM_TYPE_DOUBLE_TYPE, NULL);
}
