/* orm-transaction.c
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

#include "orm-transaction.h"
#include "orm-engine-private.h"
#include "orm-connection.h"
#include "../core/orm-error.h"

/*
 * OrmTransaction - Database transaction management.
 *
 * Handles BEGIN, COMMIT, ROLLBACK, and SAVEPOINT operations.
 */

struct _OrmTransaction
{
    GObject parent_instance;

    OrmConnection *connection;  /* Weak reference */
    gboolean       is_active;
};

G_DEFINE_TYPE (OrmTransaction, orm_transaction, G_TYPE_OBJECT)

/* Internal function to set connection transaction state */

static void
orm_transaction_finalize (GObject *object)
{
    OrmTransaction *self = ORM_TRANSACTION (object);

    /* Auto-rollback if not committed */
    if (self->is_active && self->connection != NULL)
    {
        g_warning ("Transaction destroyed without commit/rollback, rolling back");
        orm_transaction_rollback (self, NULL);
    }

    G_OBJECT_CLASS (orm_transaction_parent_class)->finalize (object);
}

static void
orm_transaction_class_init (OrmTransactionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_transaction_finalize;
}

static void
orm_transaction_init (OrmTransaction *self)
{
    self->connection = NULL;
    self->is_active = FALSE;
}

/**
 * orm_transaction_new:
 * @connection: The connection to create the transaction on
 * @error: Return location for error
 *
 * Creates a new transaction by executing BEGIN.
 *
 * Returns: (transfer full) (nullable): A new #OrmTransaction, or %NULL on error
 */
OrmTransaction *
orm_transaction_new (OrmConnection  *connection,
                     GError        **error)
{
    OrmTransaction *self;

    g_return_val_if_fail (ORM_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (orm_connection_is_open (connection), NULL);
    g_return_val_if_fail (!orm_connection_in_transaction (connection), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    /* Begin the transaction */
    if (!orm_connection_execute (connection, "BEGIN", error))
    {
        return NULL;
    }

    self = g_object_new (ORM_TYPE_TRANSACTION, NULL);
    self->connection = connection;  /* Weak reference */
    self->is_active = TRUE;

    orm_connection_set_in_transaction (connection, TRUE);

    return self;
}

/**
 * orm_transaction_commit:
 * @self: An #OrmTransaction
 * @error: Return location for error
 *
 * Commits the transaction.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_transaction_commit (OrmTransaction  *self,
                        GError         **error)
{
    g_return_val_if_fail (ORM_IS_TRANSACTION (self), FALSE);
    g_return_val_if_fail (self->is_active, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    if (!orm_connection_execute (self->connection, "COMMIT", error))
    {
        return FALSE;
    }

    self->is_active = FALSE;
    orm_connection_set_in_transaction (self->connection, FALSE);

    return TRUE;
}

/**
 * orm_transaction_rollback:
 * @self: An #OrmTransaction
 * @error: Return location for error
 *
 * Rolls back the transaction.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_transaction_rollback (OrmTransaction  *self,
                          GError         **error)
{
    g_return_val_if_fail (ORM_IS_TRANSACTION (self), FALSE);
    g_return_val_if_fail (self->is_active, FALSE);

    if (!orm_connection_execute (self->connection, "ROLLBACK", error))
    {
        /* Even on error, mark as inactive */
        self->is_active = FALSE;
        orm_connection_set_in_transaction (self->connection, FALSE);
        return FALSE;
    }

    self->is_active = FALSE;
    orm_connection_set_in_transaction (self->connection, FALSE);

    return TRUE;
}

/**
 * orm_transaction_savepoint:
 * @self: An #OrmTransaction
 * @name: Savepoint name
 * @error: Return location for error
 *
 * Creates a savepoint within the transaction.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_transaction_savepoint (OrmTransaction  *self,
                           const gchar     *name,
                           GError         **error)
{
    g_autofree gchar *sql = NULL;

    g_return_val_if_fail (ORM_IS_TRANSACTION (self), FALSE);
    g_return_val_if_fail (self->is_active, FALSE);
    g_return_val_if_fail (name != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    sql = g_strdup_printf ("SAVEPOINT \"%s\"", name);

    return orm_connection_execute (self->connection, sql, error);
}

/**
 * orm_transaction_release_savepoint:
 * @self: An #OrmTransaction
 * @name: Savepoint name
 * @error: Return location for error
 *
 * Releases (commits) a savepoint.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_transaction_release_savepoint (OrmTransaction  *self,
                                   const gchar     *name,
                                   GError         **error)
{
    g_autofree gchar *sql = NULL;

    g_return_val_if_fail (ORM_IS_TRANSACTION (self), FALSE);
    g_return_val_if_fail (self->is_active, FALSE);
    g_return_val_if_fail (name != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    sql = g_strdup_printf ("RELEASE SAVEPOINT \"%s\"", name);

    return orm_connection_execute (self->connection, sql, error);
}

/**
 * orm_transaction_rollback_to_savepoint:
 * @self: An #OrmTransaction
 * @name: Savepoint name
 * @error: Return location for error
 *
 * Rolls back to a savepoint.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_transaction_rollback_to_savepoint (OrmTransaction  *self,
                                       const gchar     *name,
                                       GError         **error)
{
    g_autofree gchar *sql = NULL;

    g_return_val_if_fail (ORM_IS_TRANSACTION (self), FALSE);
    g_return_val_if_fail (self->is_active, FALSE);
    g_return_val_if_fail (name != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    sql = g_strdup_printf ("ROLLBACK TO SAVEPOINT \"%s\"", name);

    return orm_connection_execute (self->connection, sql, error);
}

/**
 * orm_transaction_is_active:
 * @self: An #OrmTransaction
 *
 * Checks if the transaction is still active.
 *
 * Returns: %TRUE if active
 */
gboolean
orm_transaction_is_active (OrmTransaction *self)
{
    g_return_val_if_fail (ORM_IS_TRANSACTION (self), FALSE);
    return self->is_active;
}

/**
 * orm_transaction_get_connection:
 * @self: An #OrmTransaction
 *
 * Gets the connection this transaction is on.
 *
 * Returns: (transfer none): The connection
 */
OrmConnection *
orm_transaction_get_connection (OrmTransaction *self)
{
    g_return_val_if_fail (ORM_IS_TRANSACTION (self), NULL);
    return self->connection;
}
