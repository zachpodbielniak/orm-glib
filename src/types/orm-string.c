/* orm-string.c
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

#include "orm-string.h"
#include "../core/orm-enums.h"

/*
 * OrmString - Variable-length string type.
 * Maps to: SQLite TEXT, PostgreSQL VARCHAR, MySQL VARCHAR
 */
struct _OrmString
{
    OrmSqlType parent_instance;

    guint length;
    gchar *cached_name;
};

G_DEFINE_TYPE (OrmString, orm_string, ORM_TYPE_SQL_TYPE)

enum {
    PROP_0,
    PROP_LENGTH,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

/*
 * Returns the SQL type name for the target dialect.
 * For SQLite, always returns TEXT.
 * For PostgreSQL and MySQL, returns VARCHAR(length) if length > 0.
 */
static const gchar *
orm_string_get_name (OrmSqlType     *sql_type,
                     OrmDialectType  dialect_type)
{
    OrmString *self = ORM_STRING (sql_type);

    switch (dialect_type)
    {
    case ORM_DIALECT_SQLITE:
        /* SQLite uses TEXT for all string types */
        return "TEXT";

    case ORM_DIALECT_POSTGRES:
    case ORM_DIALECT_MYSQL:
        if (self->length > 0)
        {
            /* Cache the formatted name to avoid repeated allocations */
            if (self->cached_name == NULL)
            {
                self->cached_name = g_strdup_printf ("VARCHAR(%u)", self->length);
            }
            return self->cached_name;
        }
        else
        {
            /* For unlimited length, use TEXT on PostgreSQL, VARCHAR(65535) on MySQL */
            if (dialect_type == ORM_DIALECT_POSTGRES)
            {
                return "TEXT";
            }
            else
            {
                return "TEXT";
            }
        }

    default:
        return "VARCHAR";
    }
}

static void
orm_string_finalize (GObject *object)
{
    OrmString *self = ORM_STRING (object);

    g_free (self->cached_name);

    G_OBJECT_CLASS (orm_string_parent_class)->finalize (object);
}

static void
orm_string_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    OrmString *self = ORM_STRING (object);

    switch (prop_id)
    {
    case PROP_LENGTH:
        g_value_set_uint (value, self->length);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_string_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
    OrmString *self = ORM_STRING (object);

    switch (prop_id)
    {
    case PROP_LENGTH:
        self->length = g_value_get_uint (value);
        /* Invalidate cached name */
        g_clear_pointer (&self->cached_name, g_free);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_string_class_init (OrmStringClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    OrmSqlTypeClass *type_class = ORM_SQL_TYPE_CLASS (klass);

    object_class->finalize = orm_string_finalize;
    object_class->get_property = orm_string_get_property;
    object_class->set_property = orm_string_set_property;

    type_class->get_name = orm_string_get_name;

    /**
     * OrmString:length:
     *
     * Maximum length of the string. 0 means unlimited.
     */
    properties[PROP_LENGTH] =
        g_param_spec_uint ("length",
                           "Length",
                           "Maximum length of the string",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
orm_string_init (OrmString *self)
{
    self->length = 0;
    self->cached_name = NULL;
}

/**
 * orm_string_new:
 * @length: Maximum length of the string (0 for unlimited)
 *
 * Creates a new variable-length string SQL type.
 *
 * Returns: (transfer full): A new #OrmString
 */
OrmString *
orm_string_new (guint length)
{
    return g_object_new (ORM_TYPE_STRING,
                         "length", length,
                         NULL);
}

/**
 * orm_string_get_length:
 * @self: An #OrmString
 *
 * Gets the maximum length of the string.
 *
 * Returns: The maximum length, or 0 for unlimited
 */
guint
orm_string_get_length (OrmString *self)
{
    g_return_val_if_fail (ORM_IS_STRING (self), 0);

    return self->length;
}
