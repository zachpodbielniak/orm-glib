/* orm-driver-connection.c
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

#include "orm-driver-connection.h"
#include "../core/orm-error.h"

/*
 * OrmDriverConnection - abstract live connection to one database.
 *
 * The base class forwards to the subclass and supplies defaults only
 * where a backend may legitimately have nothing to offer: isolation
 * levels and interruption.  Both defaults fail with
 * ORM_ERROR_NOT_SUPPORTED rather than silently succeeding, because a
 * caller that asked for SERIALIZABLE and was told "fine" without
 * anything happening has been given a guarantee that does not exist.
 */

G_DEFINE_ABSTRACT_TYPE (OrmDriverConnection, orm_driver_connection, G_TYPE_OBJECT)

static gint64
orm_driver_connection_real_get_last_insert_id (OrmDriverConnection *self)
{
    return 0;
}

static gint
orm_driver_connection_real_get_changes (OrmDriverConnection *self)
{
    return 0;
}

static gboolean
orm_driver_connection_real_set_isolation_level (OrmDriverConnection  *self,
                                                OrmIsolationLevel     level,
                                                gboolean              for_next_transaction,
                                                GError              **error)
{
    g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                 "This backend does not support isolation levels");
    return FALSE;
}

static gboolean
orm_driver_connection_real_interrupt (OrmDriverConnection  *self,
                                      GError              **error)
{
    g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                 "This backend cannot interrupt a statement in flight");
    return FALSE;
}

static void
orm_driver_connection_real_close (OrmDriverConnection *self)
{
}

static void
orm_driver_connection_class_init (OrmDriverConnectionClass *klass)
{
    klass->close = orm_driver_connection_real_close;
    klass->get_last_insert_id = orm_driver_connection_real_get_last_insert_id;
    klass->get_changes = orm_driver_connection_real_get_changes;
    klass->set_isolation_level = orm_driver_connection_real_set_isolation_level;
    klass->interrupt = orm_driver_connection_real_interrupt;
}

static void
orm_driver_connection_init (OrmDriverConnection *self)
{
}

/**
 * orm_driver_connection_close:
 * @self: An #OrmDriverConnection
 *
 * Disconnects from the database.  Safe to call more than once.
 */
void
orm_driver_connection_close (OrmDriverConnection *self)
{
    g_return_if_fail (ORM_IS_DRIVER_CONNECTION (self));

    ORM_DRIVER_CONNECTION_GET_CLASS (self)->close (self);
}

/**
 * orm_driver_connection_execute:
 * @self: An #OrmDriverConnection
 * @sql: The statement
 * @params: (element-type OrmValue) (nullable): Bound parameters
 * @error: Return location for error
 *
 * Runs a statement that returns no rows.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_driver_connection_execute (OrmDriverConnection  *self,
                               const gchar          *sql,
                               GList                *params,
                               GError              **error)
{
    OrmDriverConnectionClass *klass;

    g_return_val_if_fail (ORM_IS_DRIVER_CONNECTION (self), FALSE);
    g_return_val_if_fail (sql != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    klass = ORM_DRIVER_CONNECTION_GET_CLASS (self);
    g_return_val_if_fail (klass->execute != NULL, FALSE);

    return klass->execute (self, sql, params, error);
}

/**
 * orm_driver_connection_query:
 * @self: An #OrmDriverConnection
 * @sql: The statement
 * @params: (element-type OrmValue) (nullable): Bound parameters
 * @flags: Query flags
 * @error: Return location for error
 *
 * Runs a statement that returns rows.
 *
 * Returns: (transfer full) (nullable): A new #OrmDriverResult, or %NULL
 */
OrmDriverResult *
orm_driver_connection_query (OrmDriverConnection  *self,
                             const gchar          *sql,
                             GList                *params,
                             OrmQueryFlags         flags,
                             GError              **error)
{
    OrmDriverConnectionClass *klass;

    g_return_val_if_fail (ORM_IS_DRIVER_CONNECTION (self), NULL);
    g_return_val_if_fail (sql != NULL, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    klass = ORM_DRIVER_CONNECTION_GET_CLASS (self);
    g_return_val_if_fail (klass->query != NULL, NULL);

    return klass->query (self, sql, params, flags, error);
}

/**
 * orm_driver_connection_get_last_insert_id:
 * @self: An #OrmDriverConnection
 *
 * Returns: The row id generated by the last INSERT, or 0
 */
gint64
orm_driver_connection_get_last_insert_id (OrmDriverConnection *self)
{
    g_return_val_if_fail (ORM_IS_DRIVER_CONNECTION (self), 0);

    return ORM_DRIVER_CONNECTION_GET_CLASS (self)->get_last_insert_id (self);
}

/**
 * orm_driver_connection_get_changes:
 * @self: An #OrmDriverConnection
 *
 * Returns: The number of rows affected by the last statement
 */
gint
orm_driver_connection_get_changes (OrmDriverConnection *self)
{
    g_return_val_if_fail (ORM_IS_DRIVER_CONNECTION (self), 0);

    return ORM_DRIVER_CONNECTION_GET_CLASS (self)->get_changes (self);
}

/**
 * orm_driver_connection_set_isolation_level:
 * @self: An #OrmDriverConnection
 * @level: The isolation level
 * @for_next_transaction: %TRUE to affect only the next transaction
 * @error: Return location for error
 *
 * Returns: %TRUE on success
 */
gboolean
orm_driver_connection_set_isolation_level (OrmDriverConnection  *self,
                                           OrmIsolationLevel     level,
                                           gboolean              for_next_transaction,
                                           GError              **error)
{
    g_return_val_if_fail (ORM_IS_DRIVER_CONNECTION (self), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    return ORM_DRIVER_CONNECTION_GET_CLASS (self)->set_isolation_level (
        self, level, for_next_transaction, error);
}

/**
 * orm_driver_connection_interrupt:
 * @self: An #OrmDriverConnection
 * @error: Return location for error
 *
 * Asks the backend to abandon the statement in flight.  May be called
 * from a thread other than the one running the query.
 *
 * Returns: %TRUE if the backend was asked to stop
 */
gboolean
orm_driver_connection_interrupt (OrmDriverConnection  *self,
                                 GError              **error)
{
    g_return_val_if_fail (ORM_IS_DRIVER_CONNECTION (self), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    return ORM_DRIVER_CONNECTION_GET_CLASS (self)->interrupt (self, error);
}
