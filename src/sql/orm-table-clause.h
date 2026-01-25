/* orm-table-clause.h
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

#ifndef ORM_TABLE_CLAUSE_H
#define ORM_TABLE_CLAUSE_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-expression.h"
#include "../core/orm-enums.h"

G_BEGIN_DECLS

#define ORM_TYPE_TABLE_CLAUSE (orm_table_clause_get_type ())

G_DECLARE_FINAL_TYPE (OrmTableClause, orm_table_clause,
                      ORM, TABLE_CLAUSE, OrmExpression)

/*
 * OrmTableClause:
 *
 * Represents a table reference in SQL, potentially with an alias.
 * Used in FROM and JOIN clauses.
 *
 * Examples:
 *   users
 *   users AS u
 *   users u
 */

/*
 * orm_table_clause_new:
 * @table_name: Name of the table
 *
 * Creates a new table clause without an alias.
 *
 * Returns: (transfer full): A new #OrmTableClause
 */
OrmTableClause * orm_table_clause_new (const gchar *table_name);

/*
 * orm_table_clause_new_with_alias:
 * @table_name: Name of the table
 * @alias: Table alias
 *
 * Creates a new table clause with an alias.
 *
 * Returns: (transfer full): A new #OrmTableClause
 */
OrmTableClause * orm_table_clause_new_with_alias (const gchar *table_name,
                                                  const gchar *alias);

/*
 * orm_table_clause_get_name:
 * @self: A #OrmTableClause
 *
 * Gets the table name.
 *
 * Returns: (transfer none): The table name
 */
const gchar * orm_table_clause_get_name (OrmTableClause *self);

/*
 * orm_table_clause_get_alias:
 * @self: A #OrmTableClause
 *
 * Gets the table alias, if set.
 *
 * Returns: (transfer none) (nullable): The alias, or NULL
 */
const gchar * orm_table_clause_get_alias (OrmTableClause *self);

/*
 * orm_table_clause_get_effective_name:
 * @self: A #OrmTableClause
 *
 * Gets the alias if set, otherwise the table name.
 *
 * Returns: (transfer none): The effective name for column references
 */
const gchar * orm_table_clause_get_effective_name (OrmTableClause *self);

G_END_DECLS

#endif /* ORM_TABLE_CLAUSE_H */
