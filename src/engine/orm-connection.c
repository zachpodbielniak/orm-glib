/* orm-connection.c
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

#include "orm-connection.h"
#include "orm-engine.h"
#include "orm-result.h"
#include "orm-transaction.h"
#include "../core/orm-error.h"
#include "../driver/orm-driver.h"
#include "../driver/orm-driver-registry.h"
#include "orm-engine-private.h"

/*
 * OrmConnection - a database connection, whatever the database.
 *
 * This is a facade. Every operation forwards to an OrmDriverConnection
 * supplied by the backend's driver, so nothing here knows what a
 * sqlite3, PGconn or MYSQL is. What the facade owns is the part that is
 * genuinely the same everywhere: the open flag, the transaction flag,
 * and the tracked isolation level.
 */

struct _OrmConnection
{
    GObject parent_instance;

    OrmEngine           *engine;       /* Weak reference */
    OrmDriverConnection *driver_conn;
    OrmDialectType       dialect_type;
    gboolean             is_open;
    gboolean             in_transaction;
    OrmIsolationLevel    isolation_level;
};

G_DEFINE_TYPE (OrmConnection, orm_connection, G_TYPE_OBJECT)

static void
orm_connection_finalize (GObject *object)
{
    OrmConnection *self = ORM_CONNECTION (object);

    orm_connection_close (self);

    G_OBJECT_CLASS (orm_connection_parent_class)->finalize (object);
}

static void
orm_connection_class_init (OrmConnectionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_connection_finalize;
}

static void
orm_connection_init (OrmConnection *self)
{
    self->engine = NULL;
    self->driver_conn = NULL;
    self->is_open = FALSE;
    self->in_transaction = FALSE;
    self->isolation_level = ORM_ISOLATION_SERIALIZABLE;
}

/**
 * orm_connection_new:
 * @engine: The engine that owns this connection
 * @error: Return location for error
 *
 * Opens a new database connection.
 *
 * Returns: (transfer full) (nullable): A new #OrmConnection, or %NULL on error
 */
OrmConnection *
orm_connection_new (OrmEngine  *engine,
                    GError    **error)
{
    OrmConnection       *self;
    OrmDriver           *driver;
    OrmDriverConnection *driver_conn;

    g_return_val_if_fail (ORM_IS_ENGINE (engine), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    driver = orm_engine_get_driver (engine);
    if (driver == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                     "No driver is registered for this database");
        return NULL;
    }

    driver_conn = orm_driver_open (driver, engine, error);
    if (driver_conn == NULL)
        return NULL;

    self = g_object_new (ORM_TYPE_CONNECTION, NULL);
    self->engine = engine;  /* Weak reference */
    self->driver_conn = driver_conn;
    self->dialect_type = orm_engine_get_dialect_type (engine);
    self->is_open = TRUE;

    /* Each backend opens at its own documented default, not a common one. */
    switch (self->dialect_type)
    {
    case ORM_DIALECT_POSTGRES:
        self->isolation_level = ORM_ISOLATION_READ_COMMITTED;
        break;
    case ORM_DIALECT_MYSQL:
        self->isolation_level = ORM_ISOLATION_REPEATABLE_READ;
        break;
    default:
        self->isolation_level = ORM_ISOLATION_SERIALIZABLE;
        break;
    }

    return self;
}

/**
 * orm_connection_close:
 * @self: An #OrmConnection
 *
 * Closes the connection.  Safe to call more than once.
 */
void
orm_connection_close (OrmConnection *self)
{
    g_return_if_fail (ORM_IS_CONNECTION (self));

    if (self->driver_conn != NULL)
    {
        orm_driver_connection_close (self->driver_conn);
        g_clear_object (&self->driver_conn);
    }

    self->is_open = FALSE;
    self->in_transaction = FALSE;
}

/**
 * orm_connection_is_open:
 * @self: An #OrmConnection
 *
 * Returns: %TRUE if the connection is open
 */
gboolean
orm_connection_is_open (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    return self->is_open;
}

/**
 * orm_connection_execute:
 * @self: An #OrmConnection
 * @sql: SQL statement
 * @error: Return location for error
 *
 * Executes a statement that returns no rows.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_connection_execute (OrmConnection  *self,
                        const gchar    *sql,
                        GError        **error)
{
    return orm_connection_execute_with_params (self, sql, NULL, error);
}

/**
 * orm_connection_execute_with_params:
 * @self: An #OrmConnection
 * @sql: SQL statement with placeholders
 * @params: (element-type OrmValue) (nullable): Parameter values
 * @error: Return location for error
 *
 * Executes a parameterized statement that returns no rows.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_connection_execute_with_params (OrmConnection  *self,
                                    const gchar    *sql,
                                    GList          *params,
                                    GError        **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    g_return_val_if_fail (sql != NULL, FALSE);
    g_return_val_if_fail (self->is_open, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    return orm_driver_connection_execute (self->driver_conn, sql, params, error);
}

/**
 * orm_connection_query:
 * @self: An #OrmConnection
 * @sql: SQL query
 * @error: Return location for error
 *
 * Executes a query and returns its result set.
 *
 * Returns: (transfer full) (nullable): A new #OrmResult, or %NULL on error
 */
OrmResult *
orm_connection_query (OrmConnection  *self,
                      const gchar    *sql,
                      GError        **error)
{
    return orm_connection_query_with_params (self, sql, NULL, error);
}

/**
 * orm_connection_query_with_params:
 * @self: An #OrmConnection
 * @sql: SQL query with placeholders
 * @params: (element-type OrmValue) (nullable): Parameter values
 * @error: Return location for error
 *
 * Executes a parameterized query and returns its result set.
 *
 * Returns: (transfer full) (nullable): A new #OrmResult, or %NULL on error
 */
OrmResult *
orm_connection_query_with_params (OrmConnection  *self,
                                  const gchar    *sql,
                                  GList          *params,
                                  GError        **error)
{
    OrmDriverResult *driver_result;

    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    g_return_val_if_fail (sql != NULL, NULL);
    g_return_val_if_fail (self->is_open, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    driver_result = orm_driver_connection_query (self->driver_conn, sql, params,
                                                 ORM_QUERY_FLAGS_NONE, error);
    if (driver_result == NULL)
        return NULL;

    return orm_result_new_for_driver (self, driver_result);
}

/**
 * orm_connection_begin_transaction:
 * @self: An #OrmConnection
 * @error: Return location for error
 *
 * Begins a new transaction.
 *
 * Returns: (transfer full) (nullable): A new #OrmTransaction, or %NULL on error
 */
OrmTransaction *
orm_connection_begin_transaction (OrmConnection  *self,
                                  GError        **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    g_return_val_if_fail (self->is_open, NULL);
    g_return_val_if_fail (!self->in_transaction, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    return orm_transaction_new (self, error);
}

/**
 * orm_connection_set_isolation_level:
 * @self: An #OrmConnection
 * @level: The isolation level to apply
 * @error: Return location for error
 *
 * Sets the transaction isolation level for the whole session, so it
 * governs every transaction started afterwards on this connection.
 *
 * Support varies by backend: SQLite accepts only
 * %ORM_ISOLATION_SERIALIZABLE (its native behaviour, applied as a no-op)
 * and %ORM_ISOLATION_READ_UNCOMMITTED, and fails the rest with
 * %ORM_ERROR_NOT_SUPPORTED rather than quietly giving you weaker
 * guarantees than you asked for.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_connection_set_isolation_level (OrmConnection      *self,
                                    OrmIsolationLevel   level,
                                    GError            **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    g_return_val_if_fail (self->is_open, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    if (!orm_driver_connection_set_isolation_level (self->driver_conn, level,
                                                    FALSE, error))
        return FALSE;

    self->isolation_level = level;
    return TRUE;
}

/**
 * orm_connection_get_isolation_level:
 * @self: An #OrmConnection
 *
 * Gets the isolation level this connection is known to be using.
 *
 * The value is tracked rather than queried: it starts at the backend's
 * documented default and follows every successful
 * orm_connection_set_isolation_level(). A level changed behind the
 * library's back -- by raw SQL, say -- is not reflected here.
 *
 * Returns: The current #OrmIsolationLevel
 */
OrmIsolationLevel
orm_connection_get_isolation_level (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), ORM_ISOLATION_SERIALIZABLE);
    return self->isolation_level;
}

/**
 * orm_connection_begin_transaction_with_isolation:
 * @self: An #OrmConnection
 * @level: The isolation level for this transaction only
 * @error: Return location for error
 *
 * Begins a transaction that runs at @level, leaving the session default
 * untouched.
 *
 * Returns: (transfer full) (nullable): A new #OrmTransaction, or %NULL on error
 */
OrmTransaction *
orm_connection_begin_transaction_with_isolation (OrmConnection      *self,
                                                 OrmIsolationLevel   level,
                                                 GError            **error)
{
    OrmTransaction *transaction;

    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    g_return_val_if_fail (self->is_open, NULL);
    g_return_val_if_fail (!self->in_transaction, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    /*
     * MySQL wants the level set before the transaction opens; PostgreSQL
     * wants it as the transaction's first statement.  SQLite's pragma is
     * connection-scoped and so belongs before BEGIN as well.
     */
    if (self->dialect_type != ORM_DIALECT_POSTGRES)
    {
        if (!orm_driver_connection_set_isolation_level (self->driver_conn, level,
                                                        TRUE, error))
            return NULL;

        return orm_transaction_new (self, error);
    }

    transaction = orm_transaction_new (self, error);
    if (transaction == NULL)
        return NULL;

    if (!orm_driver_connection_set_isolation_level (self->driver_conn, level,
                                                    TRUE, error))
    {
        /*
         * Roll back rather than hand back a transaction running at the
         * wrong isolation level -- a caller that asked for SERIALIZABLE
         * and silently got READ COMMITTED is the worst outcome here.
         */
        orm_transaction_rollback (transaction, NULL);
        g_object_unref (transaction);
        return NULL;
    }

    return transaction;
}

/**
 * orm_connection_in_transaction:
 * @self: An #OrmConnection
 *
 * Checks if a transaction is active.
 *
 * Returns: %TRUE if in a transaction
 */
gboolean
orm_connection_in_transaction (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    return self->in_transaction;
}

/**
 * orm_connection_get_last_insert_rowid:
 * @self: An #OrmConnection
 *
 * Gets the row id generated by the last INSERT.
 *
 * Not every backend has one: PostgreSQL keeps the value in a sequence
 * rather than on the connection, and returns 0 here.  Use
 * INSERT ... RETURNING there.
 *
 * Returns: The last insert rowid, or 0
 */
gint64
orm_connection_get_last_insert_rowid (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), 0);

    if (self->driver_conn == NULL)
        return 0;

    return orm_driver_connection_get_last_insert_id (self->driver_conn);
}

/**
 * orm_connection_get_changes:
 * @self: An #OrmConnection
 *
 * Gets the number of rows affected by the last statement.
 *
 * Returns: The number of affected rows
 */
gint
orm_connection_get_changes (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), 0);

    if (self->driver_conn == NULL)
        return 0;

    return orm_driver_connection_get_changes (self->driver_conn);
}

/**
 * orm_connection_get_engine:
 * @self: An #OrmConnection
 *
 * Returns: (transfer none) (nullable): The engine that owns this connection
 */
OrmEngine *
orm_connection_get_engine (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    return self->engine;
}

/*
 * Internal: OrmTransaction tracks whether a transaction is open, since
 * BEGIN and COMMIT go through orm_connection_execute like any other
 * statement and the connection cannot otherwise tell.
 */
void
orm_connection_set_in_transaction (OrmConnection *self,
                                   gboolean       in_transaction)
{
    g_return_if_fail (ORM_IS_CONNECTION (self));
    self->in_transaction = in_transaction;
}

/*
 * Internal: the driver connection, for the layers that need to reach the
 * backend directly (OrmResult's streaming path, the inspector).
 */
OrmDriverConnection *
orm_connection_get_driver_connection (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    return self->driver_conn;
}

/*
 * Internal: the dialect this connection speaks, cached from the engine so
 * callers do not have to reach through a weak reference for it.
 */
OrmDialectType
orm_connection_get_dialect_type (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), ORM_DIALECT_SQLITE);
    return self->dialect_type;
}
