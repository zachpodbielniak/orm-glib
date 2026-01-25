/* orm-sql-type.c
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

#include "orm-sql-type.h"
#include "../core/orm-enums.h"
#include "../core/orm-value.h"

/*
 * Private data for OrmSqlType.
 */
typedef struct
{
    gboolean nullable;
} OrmSqlTypePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (OrmSqlType, orm_sql_type, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_NULLABLE,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

/*
 * Default implementation of get_name.
 * Subclasses must override this.
 */
static const gchar *
orm_sql_type_real_get_name (OrmSqlType     *self,
                            OrmDialectType  dialect_type)
{
    (void) self;
    (void) dialect_type;

    g_warning ("OrmSqlType::get_name not implemented for type %s",
               G_OBJECT_TYPE_NAME (self));
    return "UNKNOWN";
}

/*
 * Default implementation of bind_processor.
 * Returns NULL to indicate no processing needed.
 */
static OrmValue *
orm_sql_type_real_bind_processor (OrmSqlType     *self,
                                  const OrmValue *value)
{
    (void) self;
    (void) value;

    return NULL;
}

/*
 * Default implementation of result_processor.
 * Returns NULL to indicate no processing needed.
 */
static OrmValue *
orm_sql_type_real_result_processor (OrmSqlType     *self,
                                    const OrmValue *value)
{
    (void) self;
    (void) value;

    return NULL;
}

/*
 * Default implementation of compare_values.
 * Uses orm_value_compare for comparison.
 */
static gint
orm_sql_type_real_compare_values (OrmSqlType     *self,
                                  const OrmValue *a,
                                  const OrmValue *b)
{
    (void) self;

    return orm_value_compare (a, b);
}

static void
orm_sql_type_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    OrmSqlType *self = ORM_SQL_TYPE (object);
    OrmSqlTypePrivate *priv = orm_sql_type_get_instance_private (self);

    switch (prop_id)
    {
    case PROP_NULLABLE:
        g_value_set_boolean (value, priv->nullable);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_sql_type_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    OrmSqlType *self = ORM_SQL_TYPE (object);
    OrmSqlTypePrivate *priv = orm_sql_type_get_instance_private (self);

    switch (prop_id)
    {
    case PROP_NULLABLE:
        priv->nullable = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_sql_type_class_init (OrmSqlTypeClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->get_property = orm_sql_type_get_property;
    object_class->set_property = orm_sql_type_set_property;

    /* Set default virtual methods */
    klass->get_name = orm_sql_type_real_get_name;
    klass->bind_processor = orm_sql_type_real_bind_processor;
    klass->result_processor = orm_sql_type_real_result_processor;
    klass->compare_values = orm_sql_type_real_compare_values;

    /**
     * OrmSqlType:nullable:
     *
     * Whether NULL values are allowed for this type.
     */
    properties[PROP_NULLABLE] =
        g_param_spec_boolean ("nullable",
                              "Nullable",
                              "Whether NULL values are allowed",
                              TRUE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
orm_sql_type_init (OrmSqlType *self)
{
    OrmSqlTypePrivate *priv = orm_sql_type_get_instance_private (self);

    priv->nullable = TRUE;
}

/**
 * orm_sql_type_get_name:
 * @self: An #OrmSqlType
 * @dialect_type: The target database dialect
 *
 * Gets the SQL type name for a specific database dialect.
 * For example, an integer type might return "INTEGER" for SQLite
 * or "INT" for MySQL.
 *
 * Returns: (transfer none): The SQL type name string
 */
const gchar *
orm_sql_type_get_name (OrmSqlType     *self,
                       OrmDialectType  dialect_type)
{
    OrmSqlTypeClass *klass;

    g_return_val_if_fail (ORM_IS_SQL_TYPE (self), NULL);

    klass = ORM_SQL_TYPE_GET_CLASS (self);
    return klass->get_name (self, dialect_type);
}

/**
 * orm_sql_type_bind_processor:
 * @self: An #OrmSqlType
 * @value: The value to process
 *
 * Processes a value before binding to a prepared statement.
 * This can be used to convert application values to database values.
 *
 * Returns: (transfer full) (nullable): The processed value, or %NULL
 *          if no processing is needed (use the original value)
 */
OrmValue *
orm_sql_type_bind_processor (OrmSqlType     *self,
                             const OrmValue *value)
{
    OrmSqlTypeClass *klass;

    g_return_val_if_fail (ORM_IS_SQL_TYPE (self), NULL);

    klass = ORM_SQL_TYPE_GET_CLASS (self);
    return klass->bind_processor (self, value);
}

/**
 * orm_sql_type_result_processor:
 * @self: An #OrmSqlType
 * @value: The value from the database
 *
 * Processes a value retrieved from query results.
 * This can be used to convert database values to application values.
 *
 * Returns: (transfer full) (nullable): The processed value, or %NULL
 *          if no processing is needed (use the original value)
 */
OrmValue *
orm_sql_type_result_processor (OrmSqlType     *self,
                               const OrmValue *value)
{
    OrmSqlTypeClass *klass;

    g_return_val_if_fail (ORM_IS_SQL_TYPE (self), NULL);

    klass = ORM_SQL_TYPE_GET_CLASS (self);
    return klass->result_processor (self, value);
}

/**
 * orm_sql_type_compare_values:
 * @self: An #OrmSqlType
 * @a: First value
 * @b: Second value
 *
 * Compares two values of this SQL type.
 *
 * Returns: negative if @a < @b, 0 if equal, positive if @a > @b
 */
gint
orm_sql_type_compare_values (OrmSqlType     *self,
                             const OrmValue *a,
                             const OrmValue *b)
{
    OrmSqlTypeClass *klass;

    g_return_val_if_fail (ORM_IS_SQL_TYPE (self), 0);

    klass = ORM_SQL_TYPE_GET_CLASS (self);
    return klass->compare_values (self, a, b);
}

/**
 * orm_sql_type_get_nullable:
 * @self: An #OrmSqlType
 *
 * Gets whether NULL values are allowed for this type.
 *
 * Returns: %TRUE if nullable, %FALSE otherwise
 */
gboolean
orm_sql_type_get_nullable (OrmSqlType *self)
{
    OrmSqlTypePrivate *priv;

    g_return_val_if_fail (ORM_IS_SQL_TYPE (self), TRUE);

    priv = orm_sql_type_get_instance_private (self);
    return priv->nullable;
}

/**
 * orm_sql_type_set_nullable:
 * @self: An #OrmSqlType
 * @nullable: Whether NULL values are allowed
 *
 * Sets whether NULL values are allowed for this type.
 */
void
orm_sql_type_set_nullable (OrmSqlType *self,
                           gboolean    nullable)
{
    OrmSqlTypePrivate *priv;

    g_return_if_fail (ORM_IS_SQL_TYPE (self));

    priv = orm_sql_type_get_instance_private (self);

    if (priv->nullable != nullable)
    {
        priv->nullable = nullable;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NULLABLE]);
    }
}
