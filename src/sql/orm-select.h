/* orm-select.h
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

#ifndef ORM_SELECT_H
#define ORM_SELECT_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-expression.h"
#include "orm-table-clause.h"
#include "orm-column-element.h"
#include "../core/orm-enums.h"

G_BEGIN_DECLS

#define ORM_TYPE_SELECT (orm_select_get_type ())

G_DECLARE_FINAL_TYPE (OrmSelect, orm_select, ORM, SELECT, GObject)

/*
 * OrmSelect:
 *
 * Represents a SELECT SQL statement. Supports:
 * - Column selection (SELECT columns)
 * - Table source (FROM table)
 * - Filtering (WHERE condition)
 * - Ordering (ORDER BY columns)
 * - Limiting (LIMIT n OFFSET m)
 * - Grouping (GROUP BY columns)
 * - Having (HAVING condition)
 * - Joins (JOIN table ON condition)
 * - Distinct (SELECT DISTINCT)
 */

/*
 * orm_select_new:
 *
 * Creates a new SELECT statement builder.
 *
 * Returns: (transfer full): A new #OrmSelect
 */
OrmSelect * orm_select_new (void);

/*
 * orm_select_from:
 * @self: A #OrmSelect
 * @table: Table name or clause
 *
 * Sets the FROM table.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_from (OrmSelect   *self,
                             const gchar *table);

/*
 * orm_select_from_clause:
 * @self: A #OrmSelect
 * @table: (transfer full): Table clause with alias
 *
 * Sets the FROM table using a table clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_from_clause (OrmSelect      *self,
                                    OrmTableClause *table);

/*
 * orm_select_columns:
 * @self: A #OrmSelect
 * @...: Column names (NULL-terminated)
 *
 * Sets the columns to select.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_columns (OrmSelect *self,
                                ...) G_GNUC_NULL_TERMINATED;

/*
 * orm_select_column:
 * @self: A #OrmSelect
 * @column: Column name
 *
 * Adds a single column to select.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_column (OrmSelect   *self,
                               const gchar *column);

/*
 * orm_select_column_expr:
 * @self: A #OrmSelect
 * @expr: (transfer full): Column expression
 *
 * Adds an expression to the select list.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_column_expr (OrmSelect     *self,
                                    OrmExpression *expr);

/*
 * orm_select_where:
 * @self: A #OrmSelect
 * @condition: (transfer full): WHERE condition
 *
 * Sets the WHERE condition.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_where (OrmSelect     *self,
                              OrmExpression *condition);

/*
 * orm_select_and_where:
 * @self: A #OrmSelect
 * @condition: (transfer full): Additional condition
 *
 * Adds an AND condition to the WHERE clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_and_where (OrmSelect     *self,
                                  OrmExpression *condition);

/*
 * orm_select_or_where:
 * @self: A #OrmSelect
 * @condition: (transfer full): Additional condition
 *
 * Adds an OR condition to the WHERE clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_or_where (OrmSelect     *self,
                                 OrmExpression *condition);

/*
 * orm_select_order_by:
 * @self: A #OrmSelect
 * @column: Column name
 * @direction: Sort direction
 *
 * Adds an ORDER BY clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_order_by (OrmSelect         *self,
                                 const gchar       *column,
                                 OrmOrderDirection  direction);

/*
 * orm_select_limit:
 * @self: A #OrmSelect
 * @limit: Maximum rows to return
 *
 * Sets the LIMIT.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_limit (OrmSelect *self,
                              gint64     limit);

/*
 * orm_select_offset:
 * @self: A #OrmSelect
 * @offset: Rows to skip
 *
 * Sets the OFFSET.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_offset (OrmSelect *self,
                               gint64     offset);

/*
 * orm_select_distinct:
 * @self: A #OrmSelect
 * @distinct: Whether to use DISTINCT
 *
 * Enables or disables DISTINCT.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_distinct (OrmSelect *self,
                                 gboolean   distinct);

/*
 * orm_select_group_by:
 * @self: A #OrmSelect
 * @column: Column to group by
 *
 * Adds a GROUP BY column.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_group_by (OrmSelect   *self,
                                 const gchar *column);

/*
 * orm_select_having:
 * @self: A #OrmSelect
 * @condition: (transfer full): HAVING condition
 *
 * Sets the HAVING condition.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_having (OrmSelect     *self,
                               OrmExpression *condition);

/*
 * orm_select_join:
 * @self: A #OrmSelect
 * @table: Table to join
 * @condition: (transfer full): Join condition
 * @join_type: Type of join
 *
 * Adds a JOIN clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect * orm_select_join (OrmSelect     *self,
                             const gchar   *table,
                             OrmExpression *condition,
                             OrmJoinType    join_type);

/*
 * orm_select_compile:
 * @self: A #OrmSelect
 * @dialect: Database dialect
 * @params: (out) (element-type OrmValue) (optional): Parameter values
 *
 * Compiles the SELECT statement to SQL.
 *
 * Returns: (transfer full): The compiled SQL
 */
gchar * orm_select_compile (OrmSelect      *self,
                            OrmDialectType  dialect,
                            GList         **params);

G_END_DECLS

#endif /* ORM_SELECT_H */
