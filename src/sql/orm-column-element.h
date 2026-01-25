/* orm-column-element.h
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

#ifndef ORM_COLUMN_ELEMENT_H
#define ORM_COLUMN_ELEMENT_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-expression.h"
#include "orm-literal.h"

G_BEGIN_DECLS

#define ORM_TYPE_COLUMN_ELEMENT (orm_column_element_get_type ())

G_DECLARE_FINAL_TYPE (OrmColumnElement, orm_column_element,
                      ORM, COLUMN_ELEMENT, OrmExpression)

/*
 * OrmColumnElement:
 *
 * Represents a column reference in SQL expressions. Can include
 * an optional table alias for disambiguating column references
 * in joins.
 *
 * Examples:
 *   "id"           -> id
 *   "users"."id"   -> users.id
 */

/*
 * orm_column_element_new:
 * @column_name: Name of the column
 *
 * Creates a new column reference without a table qualifier.
 *
 * Returns: (transfer full): A new #OrmColumnElement
 */
OrmColumnElement * orm_column_element_new (const gchar *column_name);

/*
 * orm_column_element_new_with_table:
 * @table_name: Table name or alias
 * @column_name: Name of the column
 *
 * Creates a new column reference with a table qualifier.
 *
 * Returns: (transfer full): A new #OrmColumnElement
 */
OrmColumnElement * orm_column_element_new_with_table (const gchar *table_name,
                                                      const gchar *column_name);

/*
 * orm_column_element_get_name:
 * @self: A #OrmColumnElement
 *
 * Gets the column name.
 *
 * Returns: (transfer none): The column name
 */
const gchar * orm_column_element_get_name (OrmColumnElement *self);

/*
 * orm_column_element_get_table:
 * @self: A #OrmColumnElement
 *
 * Gets the table name or alias, if set.
 *
 * Returns: (transfer none) (nullable): The table name, or NULL
 */
const gchar * orm_column_element_get_table (OrmColumnElement *self);

/*
 * Convenience methods for creating comparison expressions
 * These allow chaining like: col_eq(column, literal)
 */

/*
 * orm_column_element_eq:
 * @self: A #OrmColumnElement
 * @value: Value to compare
 *
 * Creates column = value expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression * orm_column_element_eq (OrmColumnElement *self,
                                       OrmExpression    *value);

/*
 * orm_column_element_ne:
 * @self: A #OrmColumnElement
 * @value: Value to compare
 *
 * Creates column != value expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression * orm_column_element_ne (OrmColumnElement *self,
                                       OrmExpression    *value);

/*
 * orm_column_element_lt:
 * @self: A #OrmColumnElement
 * @value: Value to compare
 *
 * Creates column < value expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression * orm_column_element_lt (OrmColumnElement *self,
                                       OrmExpression    *value);

/*
 * orm_column_element_le:
 * @self: A #OrmColumnElement
 * @value: Value to compare
 *
 * Creates column <= value expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression * orm_column_element_le (OrmColumnElement *self,
                                       OrmExpression    *value);

/*
 * orm_column_element_gt:
 * @self: A #OrmColumnElement
 * @value: Value to compare
 *
 * Creates column > value expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression * orm_column_element_gt (OrmColumnElement *self,
                                       OrmExpression    *value);

/*
 * orm_column_element_ge:
 * @self: A #OrmColumnElement
 * @value: Value to compare
 *
 * Creates column >= value expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression * orm_column_element_ge (OrmColumnElement *self,
                                       OrmExpression    *value);

/*
 * orm_column_element_like:
 * @self: A #OrmColumnElement
 * @pattern: LIKE pattern string
 *
 * Creates column LIKE pattern expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression * orm_column_element_like (OrmColumnElement *self,
                                         const gchar      *pattern);

/*
 * orm_column_element_is_null:
 * @self: A #OrmColumnElement
 *
 * Creates column IS NULL expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression * orm_column_element_is_null (OrmColumnElement *self);

/*
 * orm_column_element_is_not_null:
 * @self: A #OrmColumnElement
 *
 * Creates column IS NOT NULL expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression * orm_column_element_is_not_null (OrmColumnElement *self);

/*
 * orm_column_element_in:
 * @self: A #OrmColumnElement
 * @values: (element-type OrmExpression): List of values
 *
 * Creates column IN (values) expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression * orm_column_element_in (OrmColumnElement *self,
                                       GList            *values);

G_END_DECLS

#endif /* ORM_COLUMN_ELEMENT_H */
