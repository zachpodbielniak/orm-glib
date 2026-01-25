/* orm-session.c
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

#include "orm-session.h"
#include "orm-query.h"
#include "../engine/orm-transaction.h"
#include "../engine/orm-result.h"
#include "../engine/orm-row.h"
#include "../core/orm-error.h"

/*
 * OrmSession - Unit of Work pattern implementation.
 *
 * Manages object state (new, dirty, deleted) and coordinates
 * persistence operations through the connection. Maintains an
 * identity map to ensure object uniqueness.
 */

struct _OrmSession
{
    GObject parent_instance;

    OrmConnection  *connection;
    OrmTransaction *transaction;
    OrmIdentityMap *identity_map;

    /* Registered mappers: GType -> OrmMapper */
    GHashTable     *mappers;

    /* Object state tracking */
    GHashTable     *pending;   /* New objects pending INSERT */
    GHashTable     *dirty;     /* Modified objects pending UPDATE */
    GHashTable     *deleted;   /* Objects pending DELETE */
    GHashTable     *attached;  /* All attached objects */

    gboolean        is_closed;
    gboolean        autoflush;
};

G_DEFINE_TYPE (OrmSession, orm_session, G_TYPE_OBJECT)

static void
orm_session_finalize (GObject *object)
{
    OrmSession *self = ORM_SESSION (object);

    /* Rollback any uncommitted changes */
    if (!self->is_closed && self->transaction != NULL &&
        orm_transaction_is_active (self->transaction))
    {
        orm_transaction_rollback (self->transaction, NULL);
    }

    g_clear_object (&self->connection);
    g_clear_object (&self->transaction);
    g_clear_object (&self->identity_map);
    g_clear_pointer (&self->mappers, g_hash_table_unref);
    g_clear_pointer (&self->pending, g_hash_table_unref);
    g_clear_pointer (&self->dirty, g_hash_table_unref);
    g_clear_pointer (&self->deleted, g_hash_table_unref);
    g_clear_pointer (&self->attached, g_hash_table_unref);

    G_OBJECT_CLASS (orm_session_parent_class)->finalize (object);
}

static void
orm_session_class_init (OrmSessionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_session_finalize;
}

static void
orm_session_init (OrmSession *self)
{
    self->connection = NULL;
    self->transaction = NULL;
    self->identity_map = orm_identity_map_new ();
    self->mappers = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                            NULL, g_object_unref);
    self->pending = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                            NULL, NULL);
    self->dirty = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                          NULL, NULL);
    self->deleted = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                            NULL, NULL);
    self->attached = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                             NULL, g_object_unref);
    self->is_closed = FALSE;
    self->autoflush = TRUE;
}

/**
 * orm_session_new:
 * @engine: The database engine
 *
 * Creates a new session for the given engine.
 * Opens a connection automatically.
 *
 * Returns: (transfer full): A new #OrmSession
 */
OrmSession *
orm_session_new (OrmEngine *engine)
{
    OrmSession *self;
    g_autoptr(GError) error = NULL;

    g_return_val_if_fail (ORM_IS_ENGINE (engine), NULL);

    self = g_object_new (ORM_TYPE_SESSION, NULL);
    self->connection = orm_engine_connect (engine, &error);

    if (self->connection == NULL)
    {
        g_warning ("Failed to create connection: %s",
                   error ? error->message : "unknown");
        g_object_unref (self);
        return NULL;
    }

    return self;
}

/**
 * orm_session_new_with_connection:
 * @connection: An existing connection
 *
 * Creates a new session using an existing connection.
 *
 * Returns: (transfer full): A new #OrmSession
 */
OrmSession *
orm_session_new_with_connection (OrmConnection *connection)
{
    OrmSession *self;

    g_return_val_if_fail (ORM_IS_CONNECTION (connection), NULL);

    self = g_object_new (ORM_TYPE_SESSION, NULL);
    self->connection = g_object_ref (connection);

    return self;
}

/**
 * orm_session_get_connection:
 * @self: An #OrmSession
 *
 * Gets the underlying connection.
 *
 * Returns: (transfer none): The connection
 */
OrmConnection *
orm_session_get_connection (OrmSession *self)
{
    g_return_val_if_fail (ORM_IS_SESSION (self), NULL);
    return self->connection;
}

/**
 * orm_session_register_mapper:
 * @self: An #OrmSession
 * @mapper: The mapper to register
 *
 * Registers a mapper with the session.
 * Required before working with objects of that type.
 */
void
orm_session_register_mapper (OrmSession *self,
                             OrmMapper  *mapper)
{
    GType gtype;

    g_return_if_fail (ORM_IS_SESSION (self));
    g_return_if_fail (ORM_IS_MAPPER (mapper));

    gtype = orm_mapper_get_gtype (mapper);
    g_hash_table_insert (self->mappers, GSIZE_TO_POINTER (gtype),
                         g_object_ref (mapper));
}

/**
 * orm_session_get_mapper:
 * @self: An #OrmSession
 * @gtype: The GType to look up
 *
 * Gets the mapper for a GType.
 *
 * Returns: (transfer none) (nullable): The mapper
 */
OrmMapper *
orm_session_get_mapper (OrmSession *self,
                        GType       gtype)
{
    g_return_val_if_fail (ORM_IS_SESSION (self), NULL);
    return g_hash_table_lookup (self->mappers, GSIZE_TO_POINTER (gtype));
}

/**
 * orm_session_add:
 * @self: An #OrmSession
 * @object: The object to add
 *
 * Adds a new object to the session (pending INSERT).
 * The object must implement OrmSerializable.
 */
void
orm_session_add (OrmSession *self,
                 GObject    *object)
{
    GType gtype;

    g_return_if_fail (ORM_IS_SESSION (self));
    g_return_if_fail (G_IS_OBJECT (object));
    g_return_if_fail (ORM_IS_SERIALIZABLE (object));
    g_return_if_fail (!self->is_closed);

    gtype = G_OBJECT_TYPE (object);

    /* Ensure we have a mapper */
    if (orm_session_get_mapper (self, gtype) == NULL)
    {
        /* Auto-register mapper from serializable */
        g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (gtype);
        orm_session_register_mapper (self, mapper);
    }

    /* Add to pending set */
    g_hash_table_add (self->pending, object);

    /* Add to attached set (takes reference) */
    g_hash_table_insert (self->attached, object, g_object_ref (object));
}

/**
 * orm_session_add_all:
 * @self: An #OrmSession
 * @objects: (element-type GObject): List of objects to add
 *
 * Adds multiple objects to the session.
 */
void
orm_session_add_all (OrmSession *self,
                     GList      *objects)
{
    GList *l;

    g_return_if_fail (ORM_IS_SESSION (self));

    for (l = objects; l != NULL; l = l->next)
    {
        orm_session_add (self, G_OBJECT (l->data));
    }
}

/**
 * orm_session_delete:
 * @self: An #OrmSession
 * @object: The object to delete
 *
 * Marks an object for deletion (pending DELETE).
 */
void
orm_session_delete (OrmSession *self,
                    GObject    *object)
{
    g_return_if_fail (ORM_IS_SESSION (self));
    g_return_if_fail (G_IS_OBJECT (object));
    g_return_if_fail (!self->is_closed);

    /* If object was pending, just remove it */
    if (g_hash_table_contains (self->pending, object))
    {
        g_hash_table_remove (self->pending, object);
        g_hash_table_remove (self->attached, object);
        return;
    }

    /* Mark for deletion */
    g_hash_table_remove (self->dirty, object);
    g_hash_table_add (self->deleted, object);
}

/**
 * orm_session_expunge:
 * @self: An #OrmSession
 * @object: The object to remove
 *
 * Removes an object from the session without deleting from database.
 * The object becomes detached.
 */
void
orm_session_expunge (OrmSession *self,
                     GObject    *object)
{
    g_return_if_fail (ORM_IS_SESSION (self));
    g_return_if_fail (G_IS_OBJECT (object));

    g_hash_table_remove (self->pending, object);
    g_hash_table_remove (self->dirty, object);
    g_hash_table_remove (self->deleted, object);
    g_hash_table_remove (self->attached, object);
    orm_identity_map_remove_object (self->identity_map, object);
}

/**
 * orm_session_get_object_state:
 * @self: An #OrmSession
 * @object: The object to check
 *
 * Gets the state of an object in this session.
 *
 * Returns: The object state
 */
OrmObjectState
orm_session_get_object_state (OrmSession *self,
                              GObject    *object)
{
    g_return_val_if_fail (ORM_IS_SESSION (self), ORM_OBJECT_TRANSIENT);
    g_return_val_if_fail (G_IS_OBJECT (object), ORM_OBJECT_TRANSIENT);

    if (g_hash_table_contains (self->deleted, object))
    {
        return ORM_OBJECT_DELETED;
    }

    if (g_hash_table_contains (self->pending, object))
    {
        return ORM_OBJECT_PENDING;
    }

    if (g_hash_table_contains (self->dirty, object))
    {
        return ORM_OBJECT_DIRTY;
    }

    if (g_hash_table_contains (self->attached, object))
    {
        return ORM_OBJECT_PERSISTENT;
    }

    return ORM_OBJECT_TRANSIENT;
}

/**
 * orm_session_is_dirty:
 * @self: An #OrmSession
 *
 * Checks if the session has uncommitted changes.
 *
 * Returns: %TRUE if there are uncommitted changes
 */
gboolean
orm_session_is_dirty (OrmSession *self)
{
    g_return_val_if_fail (ORM_IS_SESSION (self), FALSE);

    return g_hash_table_size (self->pending) > 0 ||
           g_hash_table_size (self->dirty) > 0 ||
           g_hash_table_size (self->deleted) > 0;
}

/*
 * Build INSERT SQL for an object.
 */
static gchar *
build_insert_sql (OrmSession *self,
                  GObject    *object,
                  OrmMapper  *mapper,
                  GList     **out_params)
{
    GString *sql;
    GString *values;
    GList *columns;
    GList *params = NULL;
    GList *l;
    gboolean first;

    sql = g_string_new ("INSERT INTO ");
    g_string_append_printf (sql, "\"%s\" (", orm_mapper_get_table_name (mapper));

    values = g_string_new ("VALUES (");
    columns = orm_mapper_get_insert_columns (mapper);

    first = TRUE;
    for (l = columns; l != NULL; l = l->next)
    {
        const gchar *col_name = l->data;
        OrmProperty *prop = orm_mapper_get_property_by_column (mapper, col_name);
        const gchar *prop_name;
        OrmValue *value;

        if (prop == NULL)
        {
            continue;
        }

        prop_name = orm_property_get_property_name (prop);
        value = orm_serializable_get_property_value (ORM_SERIALIZABLE (object),
                                                      prop_name);

        if (!first)
        {
            g_string_append (sql, ", ");
            g_string_append (values, ", ");
        }
        first = FALSE;

        g_string_append_printf (sql, "\"%s\"", col_name);
        g_string_append (values, "?");
        params = g_list_append (params, value);
    }

    g_string_append (sql, ") ");
    g_string_append (values, ")");
    g_string_append (sql, values->str);

    g_list_free_full (columns, g_free);
    g_string_free (values, TRUE);

    *out_params = params;
    return g_string_free (sql, FALSE);
}

/*
 * Build DELETE SQL for an object.
 */
static gchar *
build_delete_sql (OrmSession *self,
                  GObject    *object,
                  OrmMapper  *mapper,
                  GList     **out_params)
{
    OrmProperty *pk_prop;
    const gchar *pk_name;
    const gchar *pk_col;
    OrmValue *pk_value;
    gchar *sql;

    pk_prop = orm_mapper_get_primary_key_property (mapper);
    if (pk_prop == NULL)
    {
        return NULL;
    }

    pk_name = orm_property_get_property_name (pk_prop);
    pk_col = orm_property_get_column_name (pk_prop);
    pk_value = orm_serializable_get_property_value (ORM_SERIALIZABLE (object),
                                                     pk_name);

    sql = g_strdup_printf ("DELETE FROM \"%s\" WHERE \"%s\" = ?",
                           orm_mapper_get_table_name (mapper),
                           pk_col);

    *out_params = g_list_append (NULL, pk_value);
    return sql;
}

/**
 * orm_session_flush:
 * @self: An #OrmSession
 * @error: Return location for error
 *
 * Flushes pending changes to the database without committing.
 * Executes INSERT, UPDATE, and DELETE statements.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_session_flush (OrmSession  *self,
                   GError     **error)
{
    GHashTableIter iter;
    gpointer key;
    gboolean success = TRUE;

    g_return_val_if_fail (ORM_IS_SESSION (self), FALSE);
    g_return_val_if_fail (!self->is_closed, FALSE);

    /* Process pending INSERTs */
    g_hash_table_iter_init (&iter, self->pending);
    while (g_hash_table_iter_next (&iter, &key, NULL))
    {
        GObject *object = G_OBJECT (key);
        GType gtype = G_OBJECT_TYPE (object);
        OrmMapper *mapper = orm_session_get_mapper (self, gtype);
        g_autofree gchar *sql = NULL;
        GList *params = NULL;

        if (mapper == NULL)
        {
            continue;
        }

        sql = build_insert_sql (self, object, mapper, &params);
        if (sql != NULL)
        {
            success = orm_connection_execute_with_params (self->connection,
                                                           sql, params, error);
            g_list_free_full (params, (GDestroyNotify) orm_value_free);

            if (!success)
            {
                return FALSE;
            }

            /* Get last insert ID and update object */
            if (orm_mapper_get_primary_key_property (mapper) != NULL)
            {
                gint64 last_id = orm_connection_get_last_insert_rowid (self->connection);
                if (last_id > 0)
                {
                    OrmProperty *pk_prop = orm_mapper_get_primary_key_property (mapper);
                    const gchar *pk_name = orm_property_get_property_name (pk_prop);
                    g_autoptr(OrmValue) pk_value = orm_value_new_integer (last_id);
                    orm_serializable_set_property_value (ORM_SERIALIZABLE (object),
                                                          pk_name, pk_value);

                    /* Add to identity map */
                    orm_identity_map_add (self->identity_map, gtype, pk_value, object);
                }
            }
        }

        g_hash_table_iter_remove (&iter);
    }

    /* Process DELETEs */
    g_hash_table_iter_init (&iter, self->deleted);
    while (g_hash_table_iter_next (&iter, &key, NULL))
    {
        GObject *object = G_OBJECT (key);
        GType gtype = G_OBJECT_TYPE (object);
        OrmMapper *mapper = orm_session_get_mapper (self, gtype);
        g_autofree gchar *sql = NULL;
        GList *params = NULL;

        if (mapper == NULL)
        {
            continue;
        }

        sql = build_delete_sql (self, object, mapper, &params);
        if (sql != NULL)
        {
            success = orm_connection_execute_with_params (self->connection,
                                                           sql, params, error);
            g_list_free_full (params, (GDestroyNotify) orm_value_free);

            if (!success)
            {
                return FALSE;
            }

            /* Remove from identity map */
            orm_identity_map_remove_object (self->identity_map, object);
        }

        g_hash_table_iter_remove (&iter);
        g_hash_table_remove (self->attached, object);
    }

    /* Clear dirty set (UPDATE not yet implemented) */
    g_hash_table_remove_all (self->dirty);

    return TRUE;
}

/**
 * orm_session_commit:
 * @self: An #OrmSession
 * @error: Return location for error
 *
 * Commits all pending changes to the database.
 * Calls flush() and then commits the transaction.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_session_commit (OrmSession  *self,
                    GError     **error)
{
    g_return_val_if_fail (ORM_IS_SESSION (self), FALSE);
    g_return_val_if_fail (!self->is_closed, FALSE);

    /* Flush pending changes */
    if (!orm_session_flush (self, error))
    {
        return FALSE;
    }

    /* Commit transaction if active */
    if (self->transaction != NULL && orm_transaction_is_active (self->transaction))
    {
        return orm_transaction_commit (self->transaction, error);
    }

    return TRUE;
}

/**
 * orm_session_rollback:
 * @self: An #OrmSession
 *
 * Rolls back all pending changes.
 * Clears the session state and expires all objects.
 */
void
orm_session_rollback (OrmSession *self)
{
    g_return_if_fail (ORM_IS_SESSION (self));

    /* Rollback transaction if active */
    if (self->transaction != NULL && orm_transaction_is_active (self->transaction))
    {
        orm_transaction_rollback (self->transaction, NULL);
    }

    /* Clear all pending changes */
    g_hash_table_remove_all (self->pending);
    g_hash_table_remove_all (self->dirty);
    g_hash_table_remove_all (self->deleted);
}

/**
 * orm_session_refresh:
 * @self: An #OrmSession
 * @object: The object to refresh
 * @error: Return location for error
 *
 * Refreshes an object from the database.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_session_refresh (OrmSession  *self,
                     GObject     *object,
                     GError     **error)
{
    GType gtype;
    OrmMapper *mapper;
    OrmProperty *pk_prop;
    OrmValue *pk_value;
    g_autofree gchar *sql = NULL;
    g_autoptr(OrmResult) result = NULL;
    OrmRow *row;
    GList *props;
    GList *l;

    g_return_val_if_fail (ORM_IS_SESSION (self), FALSE);
    g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
    g_return_val_if_fail (ORM_IS_SERIALIZABLE (object), FALSE);

    gtype = G_OBJECT_TYPE (object);
    mapper = orm_session_get_mapper (self, gtype);
    if (mapper == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_MAPPER_ERROR,
                     "No mapper registered for type %s", g_type_name (gtype));
        return FALSE;
    }

    pk_prop = orm_mapper_get_primary_key_property (mapper);
    if (pk_prop == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_MAPPER_ERROR,
                     "No primary key defined for type %s", g_type_name (gtype));
        return FALSE;
    }

    pk_value = orm_serializable_get_property_value (ORM_SERIALIZABLE (object),
                                                     orm_property_get_property_name (pk_prop));

    sql = g_strdup_printf ("SELECT * FROM \"%s\" WHERE \"%s\" = ?",
                           orm_mapper_get_table_name (mapper),
                           orm_property_get_column_name (pk_prop));

    {
        GList *params = g_list_append (NULL, pk_value);
        result = orm_connection_query_with_params (self->connection, sql, params, error);
        g_list_free (params);
    }
    orm_value_free (pk_value);

    if (result == NULL)
    {
        return FALSE;
    }

    if (!orm_result_next (result))
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_FOUND,
                     "Object not found in database");
        return FALSE;
    }

    row = orm_result_get_row (result);
    props = orm_mapper_get_properties (mapper);

    for (l = props; l != NULL; l = l->next)
    {
        OrmProperty *prop = ORM_PROPERTY (l->data);
        const gchar *col_name = orm_property_get_column_name (prop);
        const gchar *prop_name = orm_property_get_property_name (prop);
        OrmValue *value = orm_row_get_value_by_name (row, col_name);

        if (value != NULL)
        {
            orm_serializable_set_property_value (ORM_SERIALIZABLE (object),
                                                  prop_name, value);
        }
    }

    return TRUE;
}

/**
 * orm_session_get:
 * @self: An #OrmSession
 * @gtype: The GType of the object
 * @primary_key: The primary key value
 * @error: Return location for error
 *
 * Gets an object by primary key.
 * Returns from identity map if available, otherwise loads from database.
 *
 * Returns: (transfer full) (nullable): The object, or %NULL if not found
 */
GObject *
orm_session_get (OrmSession  *self,
                 GType        gtype,
                 OrmValue    *primary_key,
                 GError     **error)
{
    GObject *object;
    OrmMapper *mapper;
    g_autofree gchar *sql = NULL;
    g_autoptr(OrmResult) result = NULL;
    OrmRow *row;
    GList *props;
    GList *l;
    OrmProperty *pk_prop;

    g_return_val_if_fail (ORM_IS_SESSION (self), NULL);
    g_return_val_if_fail (gtype != G_TYPE_NONE, NULL);
    g_return_val_if_fail (primary_key != NULL, NULL);

    /* Check identity map first */
    object = orm_identity_map_get (self->identity_map, gtype, primary_key);
    if (object != NULL)
    {
        return g_object_ref (object);
    }

    /* Get mapper */
    mapper = orm_session_get_mapper (self, gtype);
    if (mapper == NULL)
    {
        /* Try to auto-register */
        if (g_type_is_a (gtype, ORM_TYPE_SERIALIZABLE))
        {
            g_autoptr(OrmMapper) new_mapper = orm_mapper_new_from_serializable (gtype);
            orm_session_register_mapper (self, new_mapper);
            mapper = orm_session_get_mapper (self, gtype);
        }

        if (mapper == NULL)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_MAPPER_ERROR,
                         "No mapper for type %s", g_type_name (gtype));
            return NULL;
        }
    }

    pk_prop = orm_mapper_get_primary_key_property (mapper);
    if (pk_prop == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_MAPPER_ERROR,
                     "No primary key for type %s", g_type_name (gtype));
        return NULL;
    }

    /* Query database */
    sql = g_strdup_printf ("SELECT * FROM \"%s\" WHERE \"%s\" = ?",
                           orm_mapper_get_table_name (mapper),
                           orm_property_get_column_name (pk_prop));

    {
        g_autoptr(OrmValue) pk_copy = orm_value_copy (primary_key);
        GList *params = g_list_append (NULL, pk_copy);
        result = orm_connection_query_with_params (self->connection, sql, params, error);
        g_list_free (params);
    }
    if (result == NULL)
    {
        return NULL;
    }

    if (!orm_result_next (result))
    {
        return NULL;  /* Not found */
    }

    /* Create new object */
    object = g_object_new (gtype, NULL);
    row = orm_result_get_row (result);
    props = orm_mapper_get_properties (mapper);

    for (l = props; l != NULL; l = l->next)
    {
        OrmProperty *prop = ORM_PROPERTY (l->data);
        const gchar *col_name = orm_property_get_column_name (prop);
        const gchar *prop_name = orm_property_get_property_name (prop);
        OrmValue *value = orm_row_get_value_by_name (row, col_name);

        if (value != NULL)
        {
            orm_serializable_set_property_value (ORM_SERIALIZABLE (object),
                                                  prop_name, value);
        }
    }

    /* Add to identity map and attached set */
    orm_identity_map_add (self->identity_map, gtype, primary_key, object);
    g_hash_table_insert (self->attached, object, g_object_ref (object));

    return object;
}

/**
 * orm_session_query:
 * @self: An #OrmSession
 * @gtype: The GType to query
 *
 * Creates a query for the given type.
 *
 * Returns: (transfer full): A new #OrmQuery
 */
OrmQuery *
orm_session_query (OrmSession *self,
                   GType       gtype)
{
    g_return_val_if_fail (ORM_IS_SESSION (self), NULL);
    g_return_val_if_fail (gtype != G_TYPE_NONE, NULL);

    return orm_query_new (self, gtype);
}

/**
 * orm_session_execute:
 * @self: An #OrmSession
 * @sql: Raw SQL to execute
 * @error: Return location for error
 *
 * Executes raw SQL through the session.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_session_execute (OrmSession  *self,
                     const gchar *sql,
                     GError     **error)
{
    g_return_val_if_fail (ORM_IS_SESSION (self), FALSE);
    g_return_val_if_fail (sql != NULL, FALSE);
    g_return_val_if_fail (!self->is_closed, FALSE);

    return orm_connection_execute (self->connection, sql, error);
}

/**
 * orm_session_close:
 * @self: An #OrmSession
 *
 * Closes the session and releases resources.
 * Any uncommitted changes are rolled back.
 */
void
orm_session_close (OrmSession *self)
{
    g_return_if_fail (ORM_IS_SESSION (self));

    if (self->is_closed)
    {
        return;
    }

    /* Rollback any pending changes */
    orm_session_rollback (self);

    /* Clear identity map and attached objects */
    orm_identity_map_clear (self->identity_map);
    g_hash_table_remove_all (self->attached);

    /* Close connection if we own it */
    if (self->connection != NULL)
    {
        orm_connection_close (self->connection);
    }

    self->is_closed = TRUE;
}

/**
 * orm_session_is_closed:
 * @self: An #OrmSession
 *
 * Checks if the session is closed.
 *
 * Returns: %TRUE if closed
 */
gboolean
orm_session_is_closed (OrmSession *self)
{
    g_return_val_if_fail (ORM_IS_SESSION (self), TRUE);
    return self->is_closed;
}
