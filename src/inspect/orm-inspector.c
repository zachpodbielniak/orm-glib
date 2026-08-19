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
#include "../engine/orm-engine-private.h"

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

/* ------------------------------------------------------------------ */
/* Asynchronous introspection                                         */
/* ------------------------------------------------------------------ */

/*
 * Every asynchronous inspector operation is the synchronous one run on
 * the inspected connection's worker thread.  Nothing about reading a
 * catalog changes off the main thread, and duplicating the per-backend
 * query sequences to make them asynchronous would be two implementations
 * of the same thing, one of which would rot.
 *
 * Going through the connection's queue rather than a thread of the
 * inspector's own is what makes this safe: introspection is several
 * dependent queries on a handle that tolerates one statement at a time.
 */

typedef struct
{
    gchar *table;
    gchar *schema;
} OrmInspectJob;

/*
 * The row count and whether it was estimated travel together, since a
 * GTask carries one value and the two are meaningless apart.
 */
typedef struct
{
    gint64   count;
    gboolean is_estimate;
} OrmRowCountResult;

static OrmInspectJob *
orm_inspect_job_new (const gchar *table,
                     const gchar *schema)
{
    OrmInspectJob *job;

    job = g_new0 (OrmInspectJob, 1);
    job->table = g_strdup (table);
    job->schema = g_strdup (schema);

    return job;
}

static void
orm_inspect_job_free (gpointer data)
{
    OrmInspectJob *job = (OrmInspectJob *) data;

    g_free (job->table);
    g_free (job->schema);
    g_free (job);
}

/*
 * Hands back a string vector, or the error that stopped it.
 */
static void
orm_inspect_return_strv (GTask   *task,
                         gchar  **strv,
                         GError  *error)
{
    if (!orm_connection_task_may_return (task))
    {
        g_strfreev (strv);
        g_clear_error (&error);
        return;
    }

    if (strv == NULL)
        g_task_return_error (task, g_steal_pointer (&error));
    else
        g_task_return_pointer (task, strv, (GDestroyNotify) g_strfreev);
}

/*
 * Hands back an array of schema-info objects, or the error.
 */
static void
orm_inspect_return_array (GTask     *task,
                          GPtrArray *array,
                          GError    *error)
{
    if (!orm_connection_task_may_return (task))
    {
        g_clear_pointer (&array, g_ptr_array_unref);
        g_clear_error (&error);
        return;
    }

    if (array == NULL)
        g_task_return_error (task, g_steal_pointer (&error));
    else
        g_task_return_pointer (task, array, (GDestroyNotify) g_ptr_array_unref);
}

/*
 * Queues @run on the worker of the connection this inspector reads.
 */
static void
orm_inspector_submit (OrmInspector     *self,
                      GTask            *task,
                      OrmWorkerJobFunc  run,
                      OrmInspectJob    *job)
{
    OrmConnection *connection = orm_inspector_get_connection (self);

    if (connection == NULL)
    {
        orm_inspect_job_free (job);
        g_task_return_new_error (task, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                                 "Inspector has no connection");
        return;
    }

    orm_connection_submit_async (connection, task, run, job,
                                 orm_inspect_job_free);
}

static void
orm_inspector_list_schemas_job (gpointer  data,
                                GTask    *task)
{
    OrmInspector  *self = ORM_INSPECTOR (g_task_get_source_object (task));
    GError        *error = NULL;
    gchar        **schemas;

    if (!orm_connection_task_may_run (task))
        return;

    schemas = orm_inspector_list_schemas (self, &error);
    orm_inspect_return_strv (task, schemas, error);
}

/**
 * orm_inspector_list_schemas_async:
 * @self: An #OrmInspector
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the schemas have been read
 * @user_data: (closure): Data for @callback
 *
 * Lists the schemas in the database, on the connection's worker thread.
 */
void
orm_inspector_list_schemas_async (OrmInspector        *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (ORM_IS_INSPECTOR (self));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_inspector_list_schemas_async);

    orm_inspector_submit (self, task, orm_inspector_list_schemas_job,
                          orm_inspect_job_new (NULL, NULL));
}

/**
 * orm_inspector_list_schemas_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_inspector_list_schemas_async().
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): The
 *   schema names, or %NULL on error
 */
gchar **
orm_inspector_list_schemas_finish (OrmInspector  *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return (gchar **) g_task_propagate_pointer (G_TASK (result), error);
}

static void
orm_inspector_list_relations_job (gpointer  data,
                                  GTask    *task)
{
    OrmInspector  *self = ORM_INSPECTOR (g_task_get_source_object (task));
    OrmInspectJob *job = (OrmInspectJob *) data;
    GError        *error = NULL;
    GPtrArray     *relations;

    if (!orm_connection_task_may_run (task))
        return;

    relations = orm_inspector_list_relations (self, job->schema, &error);
    orm_inspect_return_array (task, relations, error);
}

/**
 * orm_inspector_list_relations_async:
 * @self: An #OrmInspector
 * @schema: (nullable): The schema, or %NULL for the default
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the relations have been read
 * @user_data: (closure): Data for @callback
 *
 * Lists the tables and views in @schema, on the connection's worker thread.
 */
void
orm_inspector_list_relations_async (OrmInspector        *self,
                                    const gchar         *schema,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (ORM_IS_INSPECTOR (self));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_inspector_list_relations_async);

    orm_inspector_submit (self, task, orm_inspector_list_relations_job,
                          orm_inspect_job_new (NULL, schema));
}

/**
 * orm_inspector_list_relations_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_inspector_list_relations_async().
 *
 * Returns: (transfer full) (element-type OrmTableInfo) (nullable): The
 *   relations, or %NULL on error
 */
GPtrArray *
orm_inspector_list_relations_finish (OrmInspector  *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return (GPtrArray *) g_task_propagate_pointer (G_TASK (result), error);
}

static void
orm_inspector_get_columns_job (gpointer  data,
                               GTask    *task)
{
    OrmInspector  *self = ORM_INSPECTOR (g_task_get_source_object (task));
    OrmInspectJob *job = (OrmInspectJob *) data;
    GError        *error = NULL;
    GPtrArray     *columns;

    if (!orm_connection_task_may_run (task))
        return;

    columns = orm_inspector_get_columns (self, job->table, job->schema, &error);
    orm_inspect_return_array (task, columns, error);
}

/**
 * orm_inspector_get_columns_async:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the columns have been read
 * @user_data: (closure): Data for @callback
 *
 * Gets the columns of @table, on the connection's worker thread.
 */
void
orm_inspector_get_columns_async (OrmInspector        *self,
                                 const gchar         *table,
                                 const gchar         *schema,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (ORM_IS_INSPECTOR (self));
    g_return_if_fail (table != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_inspector_get_columns_async);

    orm_inspector_submit (self, task, orm_inspector_get_columns_job,
                          orm_inspect_job_new (table, schema));
}

/**
 * orm_inspector_get_columns_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_inspector_get_columns_async().
 *
 * Returns: (transfer full) (element-type OrmColumnInfo) (nullable): The
 *   columns, or %NULL on error
 */
GPtrArray *
orm_inspector_get_columns_finish (OrmInspector  *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return (GPtrArray *) g_task_propagate_pointer (G_TASK (result), error);
}

static void
orm_inspector_get_indexes_job (gpointer  data,
                               GTask    *task)
{
    OrmInspector  *self = ORM_INSPECTOR (g_task_get_source_object (task));
    OrmInspectJob *job = (OrmInspectJob *) data;
    GError        *error = NULL;
    GPtrArray     *indexes;

    if (!orm_connection_task_may_run (task))
        return;

    indexes = orm_inspector_get_indexes (self, job->table, job->schema, &error);
    orm_inspect_return_array (task, indexes, error);
}

/**
 * orm_inspector_get_indexes_async:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the indexes have been read
 * @user_data: (closure): Data for @callback
 *
 * Gets the indexes on @table, on the connection's worker thread.
 */
void
orm_inspector_get_indexes_async (OrmInspector        *self,
                                 const gchar         *table,
                                 const gchar         *schema,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (ORM_IS_INSPECTOR (self));
    g_return_if_fail (table != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_inspector_get_indexes_async);

    orm_inspector_submit (self, task, orm_inspector_get_indexes_job,
                          orm_inspect_job_new (table, schema));
}

/**
 * orm_inspector_get_indexes_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_inspector_get_indexes_async().
 *
 * Returns: (transfer full) (element-type OrmIndexInfo) (nullable): The
 *   indexes, or %NULL on error
 */
GPtrArray *
orm_inspector_get_indexes_finish (OrmInspector  *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return (GPtrArray *) g_task_propagate_pointer (G_TASK (result), error);
}

static void
orm_inspector_get_foreign_keys_job (gpointer  data,
                                    GTask    *task)
{
    OrmInspector  *self = ORM_INSPECTOR (g_task_get_source_object (task));
    OrmInspectJob *job = (OrmInspectJob *) data;
    GError        *error = NULL;
    GPtrArray     *foreign_keys;

    if (!orm_connection_task_may_run (task))
        return;

    foreign_keys = orm_inspector_get_foreign_keys (self, job->table,
                                                   job->schema, &error);
    orm_inspect_return_array (task, foreign_keys, error);
}

/**
 * orm_inspector_get_foreign_keys_async:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the keys have been read
 * @user_data: (closure): Data for @callback
 *
 * Gets the foreign keys declared on @table, on the connection's worker
 * thread.
 */
void
orm_inspector_get_foreign_keys_async (OrmInspector        *self,
                                      const gchar         *table,
                                      const gchar         *schema,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (ORM_IS_INSPECTOR (self));
    g_return_if_fail (table != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_inspector_get_foreign_keys_async);

    orm_inspector_submit (self, task, orm_inspector_get_foreign_keys_job,
                          orm_inspect_job_new (table, schema));
}

/**
 * orm_inspector_get_foreign_keys_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_inspector_get_foreign_keys_async().
 *
 * Returns: (transfer full) (element-type OrmForeignKeyInfo) (nullable):
 *   The foreign keys, or %NULL on error
 */
GPtrArray *
orm_inspector_get_foreign_keys_finish (OrmInspector  *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return (GPtrArray *) g_task_propagate_pointer (G_TASK (result), error);
}

static void
orm_inspector_get_primary_key_job (gpointer  data,
                                   GTask    *task)
{
    OrmInspector  *self = ORM_INSPECTOR (g_task_get_source_object (task));
    OrmInspectJob *job = (OrmInspectJob *) data;
    GError        *error = NULL;
    gchar        **columns;

    if (!orm_connection_task_may_run (task))
        return;

    columns = orm_inspector_get_primary_key (self, job->table, job->schema,
                                             &error);
    orm_inspect_return_strv (task, columns, error);
}

/**
 * orm_inspector_get_primary_key_async:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the key has been read
 * @user_data: (closure): Data for @callback
 *
 * Gets the primary-key columns of @table, on the connection's worker
 * thread.
 */
void
orm_inspector_get_primary_key_async (OrmInspector        *self,
                                     const gchar         *table,
                                     const gchar         *schema,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (ORM_IS_INSPECTOR (self));
    g_return_if_fail (table != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_inspector_get_primary_key_async);

    orm_inspector_submit (self, task, orm_inspector_get_primary_key_job,
                          orm_inspect_job_new (table, schema));
}

/**
 * orm_inspector_get_primary_key_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_inspector_get_primary_key_async().
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): The
 *   column names in key order, or %NULL on error
 */
gchar **
orm_inspector_get_primary_key_finish (OrmInspector  *self,
                                      GAsyncResult  *result,
                                      GError       **error)
{
    g_return_val_if_fail (ORM_IS_INSPECTOR (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return (gchar **) g_task_propagate_pointer (G_TASK (result), error);
}

static void
orm_inspector_estimate_row_count_job (gpointer  data,
                                      GTask    *task)
{
    OrmInspector      *self = ORM_INSPECTOR (g_task_get_source_object (task));
    OrmInspectJob     *job = (OrmInspectJob *) data;
    OrmRowCountResult *count_result;
    GError            *error = NULL;
    gboolean           is_estimate = FALSE;
    gint64             count;

    if (!orm_connection_task_may_run (task))
        return;

    count = orm_inspector_estimate_row_count (self, job->table, job->schema,
                                              &is_estimate, &error);

    if (!orm_connection_task_may_return (task))
    {
        g_clear_error (&error);
        return;
    }

    if (count < 0)
    {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    count_result = g_new0 (OrmRowCountResult, 1);
    count_result->count = count;
    count_result->is_estimate = is_estimate;

    g_task_return_pointer (task, count_result, g_free);
}

/**
 * orm_inspector_estimate_row_count_async:
 * @self: An #OrmInspector
 * @table: The relation name
 * @schema: (nullable): The schema, or %NULL for the default
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the count is known
 * @user_data: (closure): Data for @callback
 *
 * Counts the rows in @table, on the connection's worker thread.
 */
void
orm_inspector_estimate_row_count_async (OrmInspector        *self,
                                        const gchar         *table,
                                        const gchar         *schema,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (ORM_IS_INSPECTOR (self));
    g_return_if_fail (table != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_inspector_estimate_row_count_async);

    orm_inspector_submit (self, task, orm_inspector_estimate_row_count_job,
                          orm_inspect_job_new (table, schema));
}

/**
 * orm_inspector_estimate_row_count_finish:
 * @self: An #OrmInspector
 * @result: The #GAsyncResult
 * @is_estimate: (out) (optional): %TRUE if the number is the planner's
 *   estimate rather than an exact count
 * @error: Return location for error
 *
 * Finishes orm_inspector_estimate_row_count_async().
 *
 * Returns: The row count, or -1 on error
 */
gint64
orm_inspector_estimate_row_count_finish (OrmInspector  *self,
                                         GAsyncResult  *result,
                                         gboolean      *is_estimate,
                                         GError       **error)
{
    OrmRowCountResult *count_result;
    gint64             count;

    g_return_val_if_fail (ORM_IS_INSPECTOR (self), -1);
    g_return_val_if_fail (g_task_is_valid (result, self), -1);

    count_result = (OrmRowCountResult *) g_task_propagate_pointer (G_TASK (result),
                                                                   error);
    if (count_result == NULL)
        return -1;

    if (is_estimate != NULL)
        *is_estimate = count_result->is_estimate;

    count = count_result->count;
    g_free (count_result);

    return count;
}
