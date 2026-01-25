/* orm-datetime.c
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

#include "orm-datetime.h"
#include "../core/orm-enums.h"
#include "../core/orm-value.h"

/*
 * OrmDateTimeType - Date and time type.
 * Maps to: SQLite TEXT (ISO8601), PostgreSQL TIMESTAMP, MySQL DATETIME
 *
 * For SQLite, we store datetime as ISO8601 strings since SQLite
 * doesn't have a native datetime type.
 */
struct _OrmDateTimeType
{
    OrmSqlType parent_instance;
};

G_DEFINE_TYPE (OrmDateTimeType, orm_datetime_type, ORM_TYPE_SQL_TYPE)

/*
 * Returns the SQL type name for the target dialect.
 */
static const gchar *
orm_datetime_type_get_name (OrmSqlType     *self,
                            OrmDialectType  dialect_type)
{
    (void) self;

    switch (dialect_type)
    {
    case ORM_DIALECT_SQLITE:
        /* SQLite stores datetime as TEXT in ISO8601 format */
        return "TEXT";
    case ORM_DIALECT_POSTGRES:
        return "TIMESTAMP";
    case ORM_DIALECT_MYSQL:
        return "DATETIME";
    default:
        return "TIMESTAMP";
    }
}

/*
 * Process datetime values for binding to SQLite.
 * Converts GDateTime to ISO8601 string for storage.
 */
static OrmValue *
orm_datetime_type_bind_processor (OrmSqlType     *self,
                                  const OrmValue *value)
{
    g_autoptr(GDateTime) dt = NULL;
    g_autofree gchar *iso_string = NULL;

    (void) self;

    if (value == NULL || orm_value_is_null (value))
    {
        return NULL;
    }

    if (orm_value_get_value_type (value) == ORM_VALUE_DATETIME)
    {
        dt = g_date_time_ref (orm_value_get_datetime (value));
        iso_string = g_date_time_format_iso8601 (dt);
        return orm_value_new_string (iso_string);
    }

    return NULL;
}

/*
 * Process string values from SQLite back to datetime.
 * Parses ISO8601 strings into GDateTime.
 */
static OrmValue *
orm_datetime_type_result_processor (OrmSqlType     *self,
                                    const OrmValue *value)
{
    const gchar *str;
    g_autoptr(GDateTime) dt = NULL;
    g_autoptr(GTimeZone) tz = NULL;

    (void) self;

    if (value == NULL || orm_value_is_null (value))
    {
        return NULL;
    }

    if (orm_value_get_value_type (value) == ORM_VALUE_STRING)
    {
        str = orm_value_get_string (value);
        if (str != NULL)
        {
            /* Try parsing as ISO8601 first */
            dt = g_date_time_new_from_iso8601 (str, NULL);
            if (dt != NULL)
            {
                return orm_value_new_datetime (dt);
            }

            /* Fall back to common formats */
            tz = g_time_zone_new_local ();
            dt = g_date_time_new_from_iso8601 (str, tz);
            if (dt != NULL)
            {
                return orm_value_new_datetime (dt);
            }
        }
    }

    return NULL;
}

static void
orm_datetime_type_class_init (OrmDateTimeTypeClass *klass)
{
    OrmSqlTypeClass *type_class = ORM_SQL_TYPE_CLASS (klass);

    type_class->get_name = orm_datetime_type_get_name;
    type_class->bind_processor = orm_datetime_type_bind_processor;
    type_class->result_processor = orm_datetime_type_result_processor;
}

static void
orm_datetime_type_init (OrmDateTimeType *self)
{
    (void) self;
}

/**
 * orm_datetime_type_new:
 *
 * Creates a new datetime SQL type.
 *
 * Returns: (transfer full): A new #OrmDateTimeType
 */
OrmDateTimeType *
orm_datetime_type_new (void)
{
    return g_object_new (ORM_TYPE_DATETIME_TYPE, NULL);
}
