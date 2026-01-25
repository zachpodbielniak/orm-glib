/* orm-column.c
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

#include "orm-column.h"
#include "orm-table.h"
#include "../types/orm-sql-type.h"

/*
 * OrmColumn - Represents a column in a database table.
 *
 * A column has a name, SQL type, and various constraints like
 * primary key, nullable, unique, and autoincrement.
 */
struct _OrmColumn
{
    GObject parent_instance;

    gchar      *name;
    OrmSqlType *sql_type;
    OrmTable   *table;          /* Weak reference to parent table */

    gboolean    primary_key;
    gboolean    nullable;
    gboolean    unique;
    gboolean    autoincrement;
    gchar      *default_value;
};

G_DEFINE_TYPE (OrmColumn, orm_column, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_NAME,
    PROP_SQL_TYPE,
    PROP_TABLE,
    PROP_PRIMARY_KEY,
    PROP_NULLABLE,
    PROP_UNIQUE,
    PROP_AUTOINCREMENT,
    PROP_DEFAULT,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
orm_column_finalize (GObject *object)
{
    OrmColumn *self = ORM_COLUMN (object);

    g_free (self->name);
    g_free (self->default_value);
    g_clear_object (&self->sql_type);

    G_OBJECT_CLASS (orm_column_parent_class)->finalize (object);
}

static void
orm_column_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    OrmColumn *self = ORM_COLUMN (object);

    switch (prop_id)
    {
    case PROP_NAME:
        g_value_set_string (value, self->name);
        break;
    case PROP_SQL_TYPE:
        g_value_set_object (value, self->sql_type);
        break;
    case PROP_TABLE:
        g_value_set_object (value, self->table);
        break;
    case PROP_PRIMARY_KEY:
        g_value_set_boolean (value, self->primary_key);
        break;
    case PROP_NULLABLE:
        g_value_set_boolean (value, self->nullable);
        break;
    case PROP_UNIQUE:
        g_value_set_boolean (value, self->unique);
        break;
    case PROP_AUTOINCREMENT:
        g_value_set_boolean (value, self->autoincrement);
        break;
    case PROP_DEFAULT:
        g_value_set_string (value, self->default_value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_column_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
    OrmColumn *self = ORM_COLUMN (object);

    switch (prop_id)
    {
    case PROP_NAME:
        g_free (self->name);
        self->name = g_value_dup_string (value);
        break;
    case PROP_SQL_TYPE:
        g_clear_object (&self->sql_type);
        self->sql_type = g_value_dup_object (value);
        break;
    case PROP_TABLE:
        /* Weak reference - don't ref */
        self->table = g_value_get_object (value);
        break;
    case PROP_PRIMARY_KEY:
        self->primary_key = g_value_get_boolean (value);
        break;
    case PROP_NULLABLE:
        self->nullable = g_value_get_boolean (value);
        break;
    case PROP_UNIQUE:
        self->unique = g_value_get_boolean (value);
        break;
    case PROP_AUTOINCREMENT:
        self->autoincrement = g_value_get_boolean (value);
        break;
    case PROP_DEFAULT:
        g_free (self->default_value);
        self->default_value = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_column_class_init (OrmColumnClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_column_finalize;
    object_class->get_property = orm_column_get_property;
    object_class->set_property = orm_column_set_property;

    /**
     * OrmColumn:name:
     *
     * The name of the column.
     */
    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "The column name",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    /**
     * OrmColumn:sql-type:
     *
     * The SQL type of the column.
     */
    properties[PROP_SQL_TYPE] =
        g_param_spec_object ("sql-type",
                             "SQL Type",
                             "The SQL type of the column",
                             ORM_TYPE_SQL_TYPE,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    /**
     * OrmColumn:table:
     *
     * The table this column belongs to.
     */
    properties[PROP_TABLE] =
        g_param_spec_object ("table",
                             "Table",
                             "The table this column belongs to",
                             G_TYPE_OBJECT, /* Forward reference */
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    /**
     * OrmColumn:primary-key:
     *
     * Whether this column is a primary key.
     */
    properties[PROP_PRIMARY_KEY] =
        g_param_spec_boolean ("primary-key",
                              "Primary Key",
                              "Whether this column is a primary key",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS);

    /**
     * OrmColumn:nullable:
     *
     * Whether this column allows NULL values.
     */
    properties[PROP_NULLABLE] =
        g_param_spec_boolean ("nullable",
                              "Nullable",
                              "Whether this column allows NULL values",
                              TRUE,
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS);

    /**
     * OrmColumn:unique:
     *
     * Whether this column has a unique constraint.
     */
    properties[PROP_UNIQUE] =
        g_param_spec_boolean ("unique",
                              "Unique",
                              "Whether this column has a unique constraint",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS);

    /**
     * OrmColumn:autoincrement:
     *
     * Whether this column auto-increments.
     */
    properties[PROP_AUTOINCREMENT] =
        g_param_spec_boolean ("autoincrement",
                              "Autoincrement",
                              "Whether this column auto-increments",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS);

    /**
     * OrmColumn:default:
     *
     * The default value expression for this column.
     */
    properties[PROP_DEFAULT] =
        g_param_spec_string ("default",
                             "Default",
                             "The default value expression",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
orm_column_init (OrmColumn *self)
{
    self->name = NULL;
    self->sql_type = NULL;
    self->table = NULL;
    self->primary_key = FALSE;
    self->nullable = TRUE;
    self->unique = FALSE;
    self->autoincrement = FALSE;
    self->default_value = NULL;
}

/**
 * orm_column_new:
 * @name: The column name
 * @type: The SQL type for this column
 *
 * Creates a new column definition.
 *
 * Returns: (transfer full): A new #OrmColumn
 */
OrmColumn *
orm_column_new (const gchar *name,
                OrmSqlType  *type)
{
    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (ORM_IS_SQL_TYPE (type), NULL);

    return g_object_new (ORM_TYPE_COLUMN,
                         "name", name,
                         "sql-type", type,
                         NULL);
}

/**
 * orm_column_get_name:
 * @self: An #OrmColumn
 *
 * Gets the column name.
 *
 * Returns: (transfer none): The column name
 */
const gchar *
orm_column_get_name (OrmColumn *self)
{
    g_return_val_if_fail (ORM_IS_COLUMN (self), NULL);

    return self->name;
}

/**
 * orm_column_get_sql_type:
 * @self: An #OrmColumn
 *
 * Gets the SQL type of the column.
 *
 * Returns: (transfer none): The SQL type
 */
OrmSqlType *
orm_column_get_sql_type (OrmColumn *self)
{
    g_return_val_if_fail (ORM_IS_COLUMN (self), NULL);

    return self->sql_type;
}

/**
 * orm_column_get_table:
 * @self: An #OrmColumn
 *
 * Gets the table this column belongs to.
 *
 * Returns: (transfer none) (nullable): The parent table, or %NULL
 */
OrmTable *
orm_column_get_table (OrmColumn *self)
{
    g_return_val_if_fail (ORM_IS_COLUMN (self), NULL);

    return self->table;
}

/**
 * orm_column_set_table:
 * @self: An #OrmColumn
 * @table: (nullable): The parent table
 *
 * Sets the table this column belongs to. This is typically called
 * by OrmTable when adding a column.
 */
void
orm_column_set_table (OrmColumn *self,
                      OrmTable  *table)
{
    g_return_if_fail (ORM_IS_COLUMN (self));

    if (self->table != table)
    {
        self->table = table;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TABLE]);
    }
}

/**
 * orm_column_get_primary_key:
 * @self: An #OrmColumn
 *
 * Gets whether this column is a primary key.
 *
 * Returns: %TRUE if primary key, %FALSE otherwise
 */
gboolean
orm_column_get_primary_key (OrmColumn *self)
{
    g_return_val_if_fail (ORM_IS_COLUMN (self), FALSE);

    return self->primary_key;
}

/**
 * orm_column_set_primary_key:
 * @self: An #OrmColumn
 * @primary_key: Whether this is a primary key
 *
 * Sets whether this column is a primary key.
 */
void
orm_column_set_primary_key (OrmColumn *self,
                            gboolean   primary_key)
{
    g_return_if_fail (ORM_IS_COLUMN (self));

    if (self->primary_key != primary_key)
    {
        self->primary_key = primary_key;
        /* Primary keys are typically not nullable */
        if (primary_key)
        {
            self->nullable = FALSE;
        }
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRIMARY_KEY]);
    }
}

/**
 * orm_column_get_nullable:
 * @self: An #OrmColumn
 *
 * Gets whether this column allows NULL values.
 *
 * Returns: %TRUE if nullable, %FALSE otherwise
 */
gboolean
orm_column_get_nullable (OrmColumn *self)
{
    g_return_val_if_fail (ORM_IS_COLUMN (self), TRUE);

    return self->nullable;
}

/**
 * orm_column_set_nullable:
 * @self: An #OrmColumn
 * @nullable: Whether NULL values are allowed
 *
 * Sets whether this column allows NULL values.
 */
void
orm_column_set_nullable (OrmColumn *self,
                         gboolean   nullable)
{
    g_return_if_fail (ORM_IS_COLUMN (self));

    if (self->nullable != nullable)
    {
        self->nullable = nullable;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NULLABLE]);
    }
}

/**
 * orm_column_get_unique:
 * @self: An #OrmColumn
 *
 * Gets whether this column has a unique constraint.
 *
 * Returns: %TRUE if unique, %FALSE otherwise
 */
gboolean
orm_column_get_unique (OrmColumn *self)
{
    g_return_val_if_fail (ORM_IS_COLUMN (self), FALSE);

    return self->unique;
}

/**
 * orm_column_set_unique:
 * @self: An #OrmColumn
 * @unique: Whether this column is unique
 *
 * Sets whether this column has a unique constraint.
 */
void
orm_column_set_unique (OrmColumn *self,
                       gboolean   unique)
{
    g_return_if_fail (ORM_IS_COLUMN (self));

    if (self->unique != unique)
    {
        self->unique = unique;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UNIQUE]);
    }
}

/**
 * orm_column_get_autoincrement:
 * @self: An #OrmColumn
 *
 * Gets whether this column auto-increments.
 *
 * Returns: %TRUE if autoincrement, %FALSE otherwise
 */
gboolean
orm_column_get_autoincrement (OrmColumn *self)
{
    g_return_val_if_fail (ORM_IS_COLUMN (self), FALSE);

    return self->autoincrement;
}

/**
 * orm_column_set_autoincrement:
 * @self: An #OrmColumn
 * @autoincrement: Whether this column auto-increments
 *
 * Sets whether this column auto-increments.
 */
void
orm_column_set_autoincrement (OrmColumn *self,
                              gboolean   autoincrement)
{
    g_return_if_fail (ORM_IS_COLUMN (self));

    if (self->autoincrement != autoincrement)
    {
        self->autoincrement = autoincrement;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTOINCREMENT]);
    }
}

/**
 * orm_column_get_default:
 * @self: An #OrmColumn
 *
 * Gets the default value expression for this column.
 *
 * Returns: (transfer none) (nullable): The default value expression
 */
const gchar *
orm_column_get_default (OrmColumn *self)
{
    g_return_val_if_fail (ORM_IS_COLUMN (self), NULL);

    return self->default_value;
}

/**
 * orm_column_set_default:
 * @self: An #OrmColumn
 * @default_value: (nullable): The default value expression
 *
 * Sets the default value expression for this column.
 */
void
orm_column_set_default (OrmColumn   *self,
                        const gchar *default_value)
{
    g_return_if_fail (ORM_IS_COLUMN (self));

    if (g_strcmp0 (self->default_value, default_value) != 0)
    {
        g_free (self->default_value);
        self->default_value = g_strdup (default_value);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEFAULT]);
    }
}
