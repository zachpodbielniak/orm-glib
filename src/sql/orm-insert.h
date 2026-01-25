/* orm-insert.h
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

#ifndef ORM_INSERT_H
#define ORM_INSERT_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-expression.h"
#include "../core/orm-enums.h"
#include "../core/orm-value.h"

G_BEGIN_DECLS

#define ORM_TYPE_INSERT (orm_insert_get_type ())

G_DECLARE_FINAL_TYPE (OrmInsert, orm_insert, ORM, INSERT, GObject)

/*
 * OrmInsert:
 *
 * Represents an INSERT SQL statement. Supports:
 * - Single row insert (INSERT INTO table (cols) VALUES (vals))
 * - Multiple row insert
 * - RETURNING clause (where supported)
 */

/*
 * orm_insert_new:
 * @table: Table name
 *
 * Creates a new INSERT statement builder.
 *
 * Returns: (transfer full): A new #OrmInsert
 */
OrmInsert * orm_insert_new (const gchar *table);

/*
 * orm_insert_column:
 * @self: A #OrmInsert
 * @column: Column name
 * @value: (transfer full): Value to insert
 *
 * Adds a column-value pair for the insert.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmInsert * orm_insert_column (OrmInsert *self,
                               const gchar *column,
                               OrmValue    *value);

/*
 * orm_insert_column_expr:
 * @self: A #OrmInsert
 * @column: Column name
 * @expr: (transfer full): Expression for the value
 *
 * Adds a column with an expression value.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmInsert * orm_insert_column_expr (OrmInsert     *self,
                                    const gchar   *column,
                                    OrmExpression *expr);

/*
 * orm_insert_returning:
 * @self: A #OrmInsert
 * @...: Column names to return (NULL-terminated)
 *
 * Adds RETURNING clause (supported by SQLite 3.35+, PostgreSQL).
 *
 * Returns: (transfer none): @self for chaining
 */
OrmInsert * orm_insert_returning (OrmInsert *self,
                                  ...) G_GNUC_NULL_TERMINATED;

/*
 * orm_insert_returning_all:
 * @self: A #OrmInsert
 *
 * Adds RETURNING * clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmInsert * orm_insert_returning_all (OrmInsert *self);

/*
 * orm_insert_compile:
 * @self: A #OrmInsert
 * @dialect: Database dialect
 * @params: (out) (element-type OrmValue) (optional): Parameter values
 *
 * Compiles the INSERT statement to SQL.
 *
 * Returns: (transfer full): The compiled SQL
 */
gchar * orm_insert_compile (OrmInsert      *self,
                            OrmDialectType  dialect,
                            GList         **params);

G_END_DECLS

#endif /* ORM_INSERT_H */
