/* orm-transaction.h
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

#ifndef ORM_TRANSACTION_H
#define ORM_TRANSACTION_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define ORM_TYPE_TRANSACTION (orm_transaction_get_type ())

G_DECLARE_FINAL_TYPE (OrmTransaction, orm_transaction, ORM, TRANSACTION, GObject)

/* Forward declarations */
typedef struct _OrmConnection OrmConnection;

/*
 * OrmTransaction:
 *
 * Represents a database transaction. Provides commit() and rollback()
 * operations, and supports savepoints.
 *
 * Transactions are obtained from OrmConnection::begin_transaction().
 * If not explicitly committed, the transaction will be rolled back
 * when the object is destroyed.
 */

/*
 * orm_transaction_new:
 * @connection: The connection to create the transaction on
 * @error: Return location for error
 *
 * Creates a new transaction. This is typically called internally.
 *
 * Returns: (transfer full) (nullable): A new #OrmTransaction, or %NULL on error
 */
OrmTransaction * orm_transaction_new (OrmConnection  *connection,
                                      GError        **error);

/*
 * orm_transaction_commit:
 * @self: An #OrmTransaction
 * @error: Return location for error
 *
 * Commits the transaction.
 *
 * Returns: %TRUE on success
 */
gboolean orm_transaction_commit (OrmTransaction  *self,
                                 GError         **error);

/*
 * orm_transaction_rollback:
 * @self: An #OrmTransaction
 * @error: Return location for error
 *
 * Rolls back the transaction.
 *
 * Returns: %TRUE on success
 */
gboolean orm_transaction_rollback (OrmTransaction  *self,
                                   GError         **error);

/*
 * orm_transaction_savepoint:
 * @self: An #OrmTransaction
 * @name: Savepoint name
 * @error: Return location for error
 *
 * Creates a savepoint.
 *
 * Returns: %TRUE on success
 */
gboolean orm_transaction_savepoint (OrmTransaction  *self,
                                    const gchar     *name,
                                    GError         **error);

/*
 * orm_transaction_release_savepoint:
 * @self: An #OrmTransaction
 * @name: Savepoint name
 * @error: Return location for error
 *
 * Releases (commits) a savepoint.
 *
 * Returns: %TRUE on success
 */
gboolean orm_transaction_release_savepoint (OrmTransaction  *self,
                                            const gchar     *name,
                                            GError         **error);

/*
 * orm_transaction_rollback_to_savepoint:
 * @self: An #OrmTransaction
 * @name: Savepoint name
 * @error: Return location for error
 *
 * Rolls back to a savepoint.
 *
 * Returns: %TRUE on success
 */
gboolean orm_transaction_rollback_to_savepoint (OrmTransaction  *self,
                                                const gchar     *name,
                                                GError         **error);

/*
 * orm_transaction_is_active:
 * @self: An #OrmTransaction
 *
 * Checks if the transaction is still active.
 *
 * Returns: %TRUE if active
 */
gboolean orm_transaction_is_active (OrmTransaction *self);

/*
 * orm_transaction_get_connection:
 * @self: An #OrmTransaction
 *
 * Gets the connection this transaction is on.
 *
 * Returns: (transfer none): The connection
 */
OrmConnection * orm_transaction_get_connection (OrmTransaction *self);

G_END_DECLS

#endif /* ORM_TRANSACTION_H */
