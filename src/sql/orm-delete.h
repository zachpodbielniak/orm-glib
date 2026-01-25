/* orm-delete.h
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

#ifndef ORM_DELETE_H
#define ORM_DELETE_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-expression.h"
#include "../core/orm-enums.h"

G_BEGIN_DECLS

#define ORM_TYPE_DELETE (orm_delete_get_type ())

G_DECLARE_FINAL_TYPE (OrmDelete, orm_delete, ORM, DELETE, GObject)

/*
 * OrmDelete:
 *
 * Represents a DELETE SQL statement. Supports:
 * - DELETE FROM table
 * - WHERE condition
 * - RETURNING clause (where supported)
 */

/*
 * orm_delete_new:
 * @table: Table name
 *
 * Creates a new DELETE statement builder.
 *
 * Returns: (transfer full): A new #OrmDelete
 */
OrmDelete * orm_delete_new (const gchar *table);

/*
 * orm_delete_where:
 * @self: A #OrmDelete
 * @condition: (transfer full): WHERE condition
 *
 * Sets the WHERE condition.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmDelete * orm_delete_where (OrmDelete     *self,
                              OrmExpression *condition);

/*
 * orm_delete_and_where:
 * @self: A #OrmDelete
 * @condition: (transfer full): Additional condition
 *
 * Adds an AND condition to the WHERE clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmDelete * orm_delete_and_where (OrmDelete     *self,
                                  OrmExpression *condition);

/*
 * orm_delete_returning:
 * @self: A #OrmDelete
 * @...: Column names to return (NULL-terminated)
 *
 * Adds RETURNING clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmDelete * orm_delete_returning (OrmDelete *self,
                                  ...) G_GNUC_NULL_TERMINATED;

/*
 * orm_delete_returning_all:
 * @self: A #OrmDelete
 *
 * Adds RETURNING * clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmDelete * orm_delete_returning_all (OrmDelete *self);

/*
 * orm_delete_compile:
 * @self: A #OrmDelete
 * @dialect: Database dialect
 * @params: (out) (element-type OrmValue) (optional): Parameter values
 *
 * Compiles the DELETE statement to SQL.
 *
 * Returns: (transfer full): The compiled SQL
 */
gchar * orm_delete_compile (OrmDelete      *self,
                            OrmDialectType  dialect,
                            GList         **params);

G_END_DECLS

#endif /* ORM_DELETE_H */
