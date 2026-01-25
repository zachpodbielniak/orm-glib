/* orm-update.h
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

#ifndef ORM_UPDATE_H
#define ORM_UPDATE_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-expression.h"
#include "../core/orm-enums.h"
#include "../core/orm-value.h"

G_BEGIN_DECLS

#define ORM_TYPE_UPDATE (orm_update_get_type ())

G_DECLARE_FINAL_TYPE (OrmUpdate, orm_update, ORM, UPDATE, GObject)

/*
 * OrmUpdate:
 *
 * Represents an UPDATE SQL statement. Supports:
 * - SET column = value
 * - WHERE condition
 * - RETURNING clause (where supported)
 */

/*
 * orm_update_new:
 * @table: Table name
 *
 * Creates a new UPDATE statement builder.
 *
 * Returns: (transfer full): A new #OrmUpdate
 */
OrmUpdate * orm_update_new (const gchar *table);

/*
 * orm_update_set:
 * @self: A #OrmUpdate
 * @column: Column name
 * @value: (transfer full): New value
 *
 * Sets a column to a value.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmUpdate * orm_update_set (OrmUpdate   *self,
                            const gchar *column,
                            OrmValue    *value);

/*
 * orm_update_set_expr:
 * @self: A #OrmUpdate
 * @column: Column name
 * @expr: (transfer full): Expression for the value
 *
 * Sets a column to an expression value.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmUpdate * orm_update_set_expr (OrmUpdate     *self,
                                 const gchar   *column,
                                 OrmExpression *expr);

/*
 * orm_update_where:
 * @self: A #OrmUpdate
 * @condition: (transfer full): WHERE condition
 *
 * Sets the WHERE condition.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmUpdate * orm_update_where (OrmUpdate     *self,
                              OrmExpression *condition);

/*
 * orm_update_and_where:
 * @self: A #OrmUpdate
 * @condition: (transfer full): Additional condition
 *
 * Adds an AND condition to the WHERE clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmUpdate * orm_update_and_where (OrmUpdate     *self,
                                  OrmExpression *condition);

/*
 * orm_update_returning:
 * @self: A #OrmUpdate
 * @...: Column names to return (NULL-terminated)
 *
 * Adds RETURNING clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmUpdate * orm_update_returning (OrmUpdate *self,
                                  ...) G_GNUC_NULL_TERMINATED;

/*
 * orm_update_returning_all:
 * @self: A #OrmUpdate
 *
 * Adds RETURNING * clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmUpdate * orm_update_returning_all (OrmUpdate *self);

/*
 * orm_update_compile:
 * @self: A #OrmUpdate
 * @dialect: Database dialect
 * @params: (out) (element-type OrmValue) (optional): Parameter values
 *
 * Compiles the UPDATE statement to SQL.
 *
 * Returns: (transfer full): The compiled SQL
 */
gchar * orm_update_compile (OrmUpdate      *self,
                            OrmDialectType  dialect,
                            GList         **params);

G_END_DECLS

#endif /* ORM_UPDATE_H */
