/* orm-inspector.c
 *
 * Copyright 2025 Zach Podbielniak
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

#include "orm-inspector.h"
#include "../core/orm-error.h"
#include "../driver/orm-driver.h"
#include "../engine/orm-engine.h"

#ifdef ORM_ENABLE_SQLITE
#include "sqlite/orm-sqlite-inspector.h"
#endif

#ifdef ORM_ENABLE_POSTGRES
#include "postgres/orm-postgres-inspector.h"
#endif

#ifdef ORM_ENABLE_MYSQL
#include "mysql/orm-mysql-inspector.h"
#endif

/*
 * OrmInspector - reads the shape of an existing database.
 *
 * The base class owns the connection and supplies the conveniences that
 * are the same everywhere (list_tables, list_views, has_table) on top of
 * the one vfunc that is not (list_relations). Subclasses answer only
 * what genuinely differs per backend.
 */

typedef struct
{
    OrmConnection *connection;   /* Weak reference: the caller owns it */
} OrmInspectorPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (OrmInspector, orm_inspector, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_CONNECTION,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
orm_inspector_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    OrmInspector        *self = ORM_INSPECTOR (object);
    OrmInspectorPrivate *priv = orm_inspector_get_instance_private (self);

    switch (prop_id)
    {
    case PROP_CONNECTION:
        priv->connection = g_value_get_object (value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
orm_inspector_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
    OrmInspector        *self = ORM_INSPECTOR (object);
    OrmInspectorPrivate *priv = orm_inspector_get_instance_private (self);

    switch (prop_id)
    {
    case PROP_CONNECTION:
        g_value_set_object (value, priv->connection);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
orm_inspector_class_init (OrmInspectorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = orm_inspector_set_property;
    object_class->get_property = orm_inspector_get_property;

    /*
     * Not a reference: an inspector is a short-lived view onto a
     * connection the caller already holds, and taking one would let a
     * forgotten inspector keep a database connection open.
     */
    properties[PROP_CONNECTION] =
        g_param_spec_object ("connection", NULL, NULL,
                             ORM_TYPE_CONNECTION,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
orm_inspector_init (OrmInspector *self)
{
}

/**
 * orm_inspector_new:
 * @connection: An open #OrmConnection
 * @error: Return location for error
 *
 * Creates an inspector for @connection's backend.
 *
 * Returns: (transfer full) (nullable): A new #OrmInspector, or %NULL
 */
OrmInspector *
orm_inspector_new (OrmConnection  *connection,
                   GError        **error)
{
    OrmEngine *engine;

    g_return_val_if_fail (ORM_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    if (!orm_connection_is_open (connection))
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                     "Cannot inspect a closed connection");
        return NULL;
    }

    engine = orm_connection_get_engine (connection);
    if (engine == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                     "Connection has no engine");
        return NULL;
    }

    switch (orm_engine_get_dialect_type (engine))
    {
#ifdef ORM_ENABLE_SQLITE
    case ORM_DIALECT_SQLITE:
        return ORM_INSPECTOR (orm_sqlite_inspector_new (connection));
#endif
#ifdef ORM_ENABLE_POSTGRES
    case ORM_DIALECT_POSTGRES:
        return ORM_INSPECTOR (orm_postgres_inspector_new (connection));
#endif
#ifdef ORM_ENABLE_MYSQL
    case ORM_DIALECT_MYSQL:
        return ORM_INSPECTOR (orm_mysql_inspector_new (connection));
#endif
    default:
        break;
    }

    g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                 "No inspector is available for this backend");
    return NULL;
}

/**
 * orm_inspector_get_connection:
 * @self: An #OrmInspector
 *
 * Returns: (transfer none): The connection being inspected
 */
OrmConnection *
orm_inspector_get_connection (OrmInspector *self)
{
    OrmInspectorPrivate *priv;

    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);

    priv = orm_inspector_get_instance_private (self);
    return priv->connection;
}

/**
 * orm_inspector_list_schemas:
 * @self: An #OrmInspector
 * @error: Return location for error
 *
 * Returns: (transfer full) (array zero-terminated=1): The schema names
 */
gchar **
orm_inspector_list_schemas (OrmInspector  *self,
                            GError       **error)
{
    OrmInspectorClass *klass;

    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    klass = ORM_INSPECTOR_GET_CLASS (self);
    g_return_val_if_fail (klass->list_schemas != NULL, NULL);

    return klass->list_schemas (self, error);
}

/**
 * orm_inspector_list_relations:
 * @self: An #OrmInspector
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Returns: (transfer full) (element-type OrmTableInfo): The relations
 */
GPtrArray *
orm_inspector_list_relations (OrmInspector  *self,
                              const gchar   *schema,
                              GError       **error)
{
    OrmInspectorClass *klass;

    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    klass = ORM_INSPECTOR_GET_CLASS (self);
    g_return_val_if_fail (klass->list_relations != NULL, NULL);

    return klass->list_relations (self, schema, error);
}

/*
 * Shared body of list_tables and list_views: walk the relations once and
 * keep the names of the requested kind.
 */
static gchar **
orm_inspector_list_of_kind (OrmInspector     *self,
                            const gchar      *schema,
                            OrmRelationKind   kind,
                            GError          **error)
{
    g_autoptr(GPtrArray) relations = NULL;
    GPtrArray           *names;
    guint                i;

    relations = orm_inspector_list_relations (self, schema, error);
    if (relations == NULL)
        return NULL;

    names = g_ptr_array_new ();

    for (i = 0; i < relations->len; i++)
    {
        OrmTableInfo *info = g_ptr_array_index (relations, i);

        if (orm_table_info_get_kind (info) == kind)
            g_ptr_array_add (names, g_strdup (orm_table_info_get_name (info)));
    }

    g_ptr_array_add (names, NULL);

    return (gchar **) g_ptr_array_free (names, FALSE);
}

/**
 * orm_inspector_list_tables:
 * @self: An #OrmInspector
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Returns: (transfer full) (array zero-terminated=1): The table names
 */
gchar **
orm_inspector_list_tables (OrmInspector  *self,
                           const gchar   *schema,
                           GError       **error)
{
    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);

    return orm_inspector_list_of_kind (self, schema, ORM_RELATION_TABLE, error);
}

/**
 * orm_inspector_list_views:
 * @self: An #OrmInspector
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Returns: (transfer full) (array zero-terminated=1): The view names
 */
gchar **
orm_inspector_list_views (OrmInspector  *self,
                          const gchar   *schema,
                          GError       **error)
{
    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);

    return orm_inspector_list_of_kind (self, schema, ORM_RELATION_VIEW, error);
}

/**
 * orm_inspector_has_table:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Returns: %TRUE if @table exists
 */
gboolean
orm_inspector_has_table (OrmInspector  *self,
                         const gchar   *table,
                         const gchar   *schema,
                         GError       **error)
{
    g_autoptr(GPtrArray) relations = NULL;
    guint                i;

    g_return_val_if_fail (ORM_IS_INSPECTOR (self), FALSE);
    g_return_val_if_fail (table != NULL, FALSE);

    relations = orm_inspector_list_relations (self, schema, error);
    if (relations == NULL)
        return FALSE;

    for (i = 0; i < relations->len; i++)
    {
        OrmTableInfo *info = g_ptr_array_index (relations, i);

        if (g_strcmp0 (orm_table_info_get_name (info), table) == 0)
            return TRUE;
    }

    return FALSE;
}

/**
 * orm_inspector_get_columns:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Returns: (transfer full) (element-type OrmColumnInfo): The columns
 */
GPtrArray *
orm_inspector_get_columns (OrmInspector  *self,
                           const gchar   *table,
                           const gchar   *schema,
                           GError       **error)
{
    OrmInspectorClass *klass;

    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);
    g_return_val_if_fail (table != NULL, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    klass = ORM_INSPECTOR_GET_CLASS (self);
    g_return_val_if_fail (klass->get_columns != NULL, NULL);

    return klass->get_columns (self, table, schema, error);
}

/**
 * orm_inspector_get_indexes:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Returns: (transfer full) (element-type OrmIndexInfo): The indexes
 */
GPtrArray *
orm_inspector_get_indexes (OrmInspector  *self,
                           const gchar   *table,
                           const gchar   *schema,
                           GError       **error)
{
    OrmInspectorClass *klass;

    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);
    g_return_val_if_fail (table != NULL, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    klass = ORM_INSPECTOR_GET_CLASS (self);
    g_return_val_if_fail (klass->get_indexes != NULL, NULL);

    return klass->get_indexes (self, table, schema, error);
}

/**
 * orm_inspector_get_foreign_keys:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Returns: (transfer full) (element-type OrmForeignKeyInfo): The foreign keys
 */
GPtrArray *
orm_inspector_get_foreign_keys (OrmInspector  *self,
                                const gchar   *table,
                                const gchar   *schema,
                                GError       **error)
{
    OrmInspectorClass *klass;

    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);
    g_return_val_if_fail (table != NULL, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    klass = ORM_INSPECTOR_GET_CLASS (self);
    g_return_val_if_fail (klass->get_foreign_keys != NULL, NULL);

    return klass->get_foreign_keys (self, table, schema, error);
}

/**
 * orm_inspector_get_primary_key:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @error: Return location for error
 *
 * Gets the primary-key columns in key order.  An empty array means the
 * table has no primary key.
 *
 * Returns: (transfer full) (array zero-terminated=1): The column names
 */
gchar **
orm_inspector_get_primary_key (OrmInspector  *self,
                               const gchar   *table,
                               const gchar   *schema,
                               GError       **error)
{
    OrmInspectorClass *klass;

    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);
    g_return_val_if_fail (table != NULL, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    klass = ORM_INSPECTOR_GET_CLASS (self);
    g_return_val_if_fail (klass->get_primary_key != NULL, NULL);

    return klass->get_primary_key (self, table, schema, error);
}

/**
 * orm_inspector_estimate_row_count:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @is_estimate: (out) (optional): %TRUE if the number is an estimate
 * @error: Return location for error
 *
 * Returns: The row count, or -1 on error
 */
gint64
orm_inspector_estimate_row_count (OrmInspector  *self,
                                  const gchar   *table,
                                  const gchar   *schema,
                                  gboolean      *is_estimate,
                                  GError       **error)
{
    OrmInspectorClass *klass;
    gboolean           local_is_estimate = FALSE;

    g_return_val_if_fail (ORM_IS_INSPECTOR (self), -1);
    g_return_val_if_fail (table != NULL, -1);
    g_return_val_if_fail (error == NULL || *error == NULL, -1);

    klass = ORM_INSPECTOR_GET_CLASS (self);
    g_return_val_if_fail (klass->estimate_row_count != NULL, -1);

    if (is_estimate == NULL)
        is_estimate = &local_is_estimate;

    return klass->estimate_row_count (self, table, schema, is_estimate, error);
}
