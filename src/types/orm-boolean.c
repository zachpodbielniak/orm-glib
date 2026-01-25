/* orm-boolean.c
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

#include "orm-boolean.h"
#include "../core/orm-enums.h"
#include "../core/orm-value.h"

/*
 * OrmBooleanType - Boolean type.
 * Maps to: SQLite INTEGER, PostgreSQL BOOLEAN, MySQL TINYINT(1)
 *
 * Note: Named OrmBooleanType to avoid collision with gboolean.
 */
struct _OrmBooleanType
{
    OrmSqlType parent_instance;
};

G_DEFINE_TYPE (OrmBooleanType, orm_boolean_type, ORM_TYPE_SQL_TYPE)

/*
 * Returns the SQL type name for the target dialect.
 */
static const gchar *
orm_boolean_type_get_name (OrmSqlType     *self,
                           OrmDialectType  dialect_type)
{
    (void) self;

    switch (dialect_type)
    {
    case ORM_DIALECT_SQLITE:
        /* SQLite doesn't have a boolean type, uses INTEGER */
        return "INTEGER";
    case ORM_DIALECT_POSTGRES:
        return "BOOLEAN";
    case ORM_DIALECT_MYSQL:
        /* MySQL uses TINYINT(1) for booleans */
        return "TINYINT(1)";
    default:
        return "BOOLEAN";
    }
}

/*
 * Process boolean values for binding to SQLite (convert to 0/1).
 */
static OrmValue *
orm_boolean_type_bind_processor (OrmSqlType     *self,
                                 const OrmValue *value)
{
    (void) self;

    if (value == NULL || orm_value_is_null (value))
    {
        return NULL;
    }

    if (orm_value_get_value_type (value) == ORM_VALUE_BOOLEAN)
    {
        /* Convert boolean to integer for databases that need it */
        gboolean bool_val = orm_value_get_boolean (value);
        return orm_value_new_integer (bool_val ? 1 : 0);
    }

    return NULL;
}

/*
 * Process integer values from SQLite back to boolean.
 */
static OrmValue *
orm_boolean_type_result_processor (OrmSqlType     *self,
                                   const OrmValue *value)
{
    (void) self;

    if (value == NULL || orm_value_is_null (value))
    {
        return NULL;
    }

    if (orm_value_get_value_type (value) == ORM_VALUE_INTEGER)
    {
        /* Convert integer back to boolean */
        gint64 int_val = orm_value_get_integer (value);
        return orm_value_new_boolean (int_val != 0);
    }

    return NULL;
}

static void
orm_boolean_type_class_init (OrmBooleanTypeClass *klass)
{
    OrmSqlTypeClass *type_class = ORM_SQL_TYPE_CLASS (klass);

    type_class->get_name = orm_boolean_type_get_name;
    type_class->bind_processor = orm_boolean_type_bind_processor;
    type_class->result_processor = orm_boolean_type_result_processor;
}

static void
orm_boolean_type_init (OrmBooleanType *self)
{
    (void) self;
}

/**
 * orm_boolean_type_new:
 *
 * Creates a new boolean SQL type.
 *
 * Returns: (transfer full): A new #OrmBooleanType
 */
OrmBooleanType *
orm_boolean_type_new (void)
{
    return g_object_new (ORM_TYPE_BOOLEAN_TYPE, NULL);
}
