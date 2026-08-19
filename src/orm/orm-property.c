/* orm-property.c
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

#include "orm-property.h"
#include "../schema/orm-column.h"
#include "../types/orm-integer.h"
#include "../types/orm-string.h"
#include "../types/orm-boolean.h"
#include "../types/orm-float.h"
#include "../types/orm-datetime.h"
#include "../types/orm-blob.h"
#include "../types/orm-text.h"

/*
 * GType registration for OrmPropertyFlags.  A bitmask, so it registers
 * with g_flags_register_static and not g_enum_register_static -- the
 * difference decides whether a binding can OR two values together.
 */
GType
orm_property_flags_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GFlagsValue values[] = {
            { ORM_PROPERTY_NONE, "ORM_PROPERTY_NONE", "none" },
            { ORM_PROPERTY_PRIMARY_KEY, "ORM_PROPERTY_PRIMARY_KEY", "primary-key" },
            { ORM_PROPERTY_NULLABLE, "ORM_PROPERTY_NULLABLE", "nullable" },
            { ORM_PROPERTY_UNIQUE, "ORM_PROPERTY_UNIQUE", "unique" },
            { ORM_PROPERTY_AUTO_INCREMENT, "ORM_PROPERTY_AUTO_INCREMENT", "auto-increment" },
            { ORM_PROPERTY_READ_ONLY, "ORM_PROPERTY_READ_ONLY", "read-only" },
            { ORM_PROPERTY_DEFERRED, "ORM_PROPERTY_DEFERRED", "deferred" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_flags_register_static ("OrmPropertyFlags", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * OrmProperty - Property to column mapping.
 *
 * Maps a GObject property to a database column, storing the property name,
 * column name, SQL type, and various configuration flags.
 */

struct _OrmProperty
{
    GObject parent_instance;

    gchar            *property_name;
    gchar            *column_name;
    OrmSqlType       *sql_type;
    OrmPropertyFlags  flags;
    OrmValue         *default_value;
};

G_DEFINE_TYPE (OrmProperty, orm_property, G_TYPE_OBJECT)

static void
orm_property_finalize (GObject *object)
{
    OrmProperty *self = ORM_PROPERTY (object);

    g_clear_pointer (&self->property_name, g_free);
    g_clear_pointer (&self->column_name, g_free);
    g_clear_object (&self->sql_type);
    g_clear_pointer (&self->default_value, orm_value_free);

    G_OBJECT_CLASS (orm_property_parent_class)->finalize (object);
}

static void
orm_property_class_init (OrmPropertyClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_property_finalize;
}

static void
orm_property_init (OrmProperty *self)
{
    self->property_name = NULL;
    self->column_name = NULL;
    self->sql_type = NULL;
    self->flags = ORM_PROPERTY_NONE;
    self->default_value = NULL;
}

/**
 * orm_property_new:
 * @property_name: The GObject property name
 * @column_name: (nullable): The database column name (defaults to property_name)
 * @sql_type: (transfer none): The SQL type for this column
 * @flags: Property flags
 *
 * Creates a new property mapping.
 *
 * Returns: (transfer full): A new #OrmProperty
 */
OrmProperty *
orm_property_new (const gchar      *property_name,
                  const gchar      *column_name,
                  OrmSqlType       *sql_type,
                  OrmPropertyFlags  flags)
{
    OrmProperty *self;

    g_return_val_if_fail (property_name != NULL, NULL);
    g_return_val_if_fail (ORM_IS_SQL_TYPE (sql_type), NULL);

    self = g_object_new (ORM_TYPE_PROPERTY, NULL);
    self->property_name = g_strdup (property_name);
    self->column_name = g_strdup (column_name != NULL ? column_name : property_name);
    self->sql_type = g_object_ref (sql_type);
    self->flags = flags;

    return self;
}

/*
 * Infer SQL type from GParamSpec.
 * Creates an appropriate OrmSqlType based on the GValue type.
 */
static OrmSqlType *
infer_sql_type_from_pspec (GParamSpec *pspec)
{
    GType value_type;

    value_type = G_PARAM_SPEC_VALUE_TYPE (pspec);

    if (value_type == G_TYPE_INT ||
        value_type == G_TYPE_UINT ||
        value_type == G_TYPE_LONG ||
        value_type == G_TYPE_ULONG ||
        value_type == G_TYPE_INT64 ||
        value_type == G_TYPE_UINT64)
    {
        return ORM_SQL_TYPE (orm_integer_new ());
    }
    else if (value_type == G_TYPE_BOOLEAN)
    {
        return ORM_SQL_TYPE (orm_boolean_type_new ());
    }
    else if (value_type == G_TYPE_FLOAT ||
             value_type == G_TYPE_DOUBLE)
    {
        return ORM_SQL_TYPE (orm_float_type_new ());
    }
    else if (value_type == G_TYPE_STRING)
    {
        /* Use TEXT for strings without length limit */
        return ORM_SQL_TYPE (orm_text_new ());
    }
    else if (g_type_is_a (value_type, G_TYPE_DATE_TIME))
    {
        return ORM_SQL_TYPE (orm_datetime_type_new ());
    }
    else if (g_type_is_a (value_type, G_TYPE_BYTES))
    {
        return ORM_SQL_TYPE (orm_blob_type_new ());
    }
    else
    {
        /* Default to TEXT for unknown types */
        return ORM_SQL_TYPE (orm_text_new ());
    }
}

/*
 * Convert GObject property name (with hyphens) to database column name (with underscores).
 */
static gchar *
property_name_to_column_name (const gchar *prop_name)
{
    gchar *result;
    gchar *p;

    result = g_strdup (prop_name);
    for (p = result; *p != '\0'; p++)
    {
        if (*p == '-')
        {
            *p = '_';
        }
    }
    return result;
}

/**
 * orm_property_new_from_pspec:
 * @pspec: The property specification
 * @column_name: (nullable): The database column name (defaults to property name with underscores)
 * @flags: Property flags
 *
 * Creates a new property mapping from a GParamSpec.
 * The SQL type is inferred from the GParamSpec type.
 * If no column name is provided, it defaults to the property name
 * with hyphens converted to underscores.
 *
 * Returns: (transfer full): A new #OrmProperty
 */
OrmProperty *
orm_property_new_from_pspec (GParamSpec       *pspec,
                             const gchar      *column_name,
                             OrmPropertyFlags  flags)
{
    OrmProperty *self;
    g_autoptr(OrmSqlType) sql_type = NULL;
    const gchar *prop_name;

    g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);

    prop_name = g_param_spec_get_name (pspec);
    sql_type = infer_sql_type_from_pspec (pspec);

    self = g_object_new (ORM_TYPE_PROPERTY, NULL);
    self->property_name = g_strdup (prop_name);
    /* Convert hyphens to underscores for column name if not explicitly provided */
    if (column_name != NULL)
    {
        self->column_name = g_strdup (column_name);
    }
    else
    {
        self->column_name = property_name_to_column_name (prop_name);
    }
    self->sql_type = g_steal_pointer (&sql_type);
    self->flags = flags;

    /* Infer nullable from param flags */
    if (!(pspec->flags & G_PARAM_CONSTRUCT_ONLY) &&
        (pspec->flags & G_PARAM_WRITABLE))
    {
        /* Properties that can be set after construction might allow NULL */
    }

    return self;
}

/**
 * orm_property_get_property_name:
 * @self: An #OrmProperty
 *
 * Gets the GObject property name.
 *
 * Returns: (transfer none): The property name
 */
const gchar *
orm_property_get_property_name (OrmProperty *self)
{
    g_return_val_if_fail (ORM_IS_PROPERTY (self), NULL);
    return self->property_name;
}

/**
 * orm_property_get_column_name:
 * @self: An #OrmProperty
 *
 * Gets the database column name.
 *
 * Returns: (transfer none): The column name
 */
const gchar *
orm_property_get_column_name (OrmProperty *self)
{
    g_return_val_if_fail (ORM_IS_PROPERTY (self), NULL);
    return self->column_name;
}

/**
 * orm_property_get_sql_type:
 * @self: An #OrmProperty
 *
 * Gets the SQL type for this column.
 *
 * Returns: (transfer none): The SQL type
 */
OrmSqlType *
orm_property_get_sql_type (OrmProperty *self)
{
    g_return_val_if_fail (ORM_IS_PROPERTY (self), NULL);
    return self->sql_type;
}

/**
 * orm_property_get_flags:
 * @self: An #OrmProperty
 *
 * Gets the property flags.
 *
 * Returns: The property flags
 */
OrmPropertyFlags
orm_property_get_flags (OrmProperty *self)
{
    g_return_val_if_fail (ORM_IS_PROPERTY (self), ORM_PROPERTY_NONE);
    return self->flags;
}

/**
 * orm_property_is_primary_key:
 * @self: An #OrmProperty
 *
 * Checks if this property is a primary key.
 *
 * Returns: %TRUE if this is a primary key
 */
gboolean
orm_property_is_primary_key (OrmProperty *self)
{
    g_return_val_if_fail (ORM_IS_PROPERTY (self), FALSE);
    return (self->flags & ORM_PROPERTY_PRIMARY_KEY) != 0;
}

/**
 * orm_property_is_nullable:
 * @self: An #OrmProperty
 *
 * Checks if this property allows NULL values.
 *
 * Returns: %TRUE if nullable
 */
gboolean
orm_property_is_nullable (OrmProperty *self)
{
    g_return_val_if_fail (ORM_IS_PROPERTY (self), FALSE);
    return (self->flags & ORM_PROPERTY_NULLABLE) != 0;
}

/**
 * orm_property_is_unique:
 * @self: An #OrmProperty
 *
 * Checks if this property must have unique values.
 *
 * Returns: %TRUE if unique
 */
gboolean
orm_property_is_unique (OrmProperty *self)
{
    g_return_val_if_fail (ORM_IS_PROPERTY (self), FALSE);
    return (self->flags & ORM_PROPERTY_UNIQUE) != 0;
}

/**
 * orm_property_is_auto_increment:
 * @self: An #OrmProperty
 *
 * Checks if this property auto-increments.
 *
 * Returns: %TRUE if auto-increment
 */
gboolean
orm_property_is_auto_increment (OrmProperty *self)
{
    g_return_val_if_fail (ORM_IS_PROPERTY (self), FALSE);
    return (self->flags & ORM_PROPERTY_AUTO_INCREMENT) != 0;
}

/**
 * orm_property_set_default_value:
 * @self: An #OrmProperty
 * @default_value: (nullable) (transfer none): The default value
 *
 * Sets the default value for this property.
 */
void
orm_property_set_default_value (OrmProperty *self,
                                OrmValue    *default_value)
{
    g_return_if_fail (ORM_IS_PROPERTY (self));

    g_clear_pointer (&self->default_value, orm_value_free);

    if (default_value != NULL)
    {
        self->default_value = orm_value_copy (default_value);
    }
}

/**
 * orm_property_get_default_value:
 * @self: An #OrmProperty
 *
 * Gets the default value for this property.
 *
 * Returns: (transfer none) (nullable): The default value
 */
OrmValue *
orm_property_get_default_value (OrmProperty *self)
{
    g_return_val_if_fail (ORM_IS_PROPERTY (self), NULL);
    return self->default_value;
}

/**
 * orm_property_to_column:
 * @self: An #OrmProperty
 *
 * Creates an OrmColumn from this property mapping.
 *
 * Returns: (transfer full): A new #OrmColumn
 */
OrmColumn *
orm_property_to_column (OrmProperty *self)
{
    OrmColumn *column;

    g_return_val_if_fail (ORM_IS_PROPERTY (self), NULL);

    column = orm_column_new (self->column_name, self->sql_type);

    orm_column_set_nullable (column, orm_property_is_nullable (self));
    orm_column_set_primary_key (column, orm_property_is_primary_key (self));
    orm_column_set_unique (column, orm_property_is_unique (self));
    orm_column_set_autoincrement (column, orm_property_is_auto_increment (self));

    if (self->default_value != NULL)
    {
        g_autofree gchar *default_sql = orm_value_to_string (self->default_value);
        orm_column_set_default (column, default_sql);
    }

    return column;
}
