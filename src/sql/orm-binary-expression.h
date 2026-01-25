/* orm-binary-expression.h
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

#ifndef ORM_BINARY_EXPRESSION_H
#define ORM_BINARY_EXPRESSION_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-expression.h"
#include "../core/orm-enums.h"

G_BEGIN_DECLS

#define ORM_TYPE_BINARY_EXPRESSION (orm_binary_expression_get_type ())

G_DECLARE_FINAL_TYPE (OrmBinaryExpression, orm_binary_expression,
                      ORM, BINARY_EXPRESSION, OrmExpression)

/*
 * OrmBinaryExpression:
 *
 * Represents a binary expression with left operand, operator, and right operand.
 * Used for comparison operations (=, !=, <, >, etc.) and logical operations
 * (AND, OR).
 *
 * Examples:
 *   column = 'value'
 *   (expr1) AND (expr2)
 *   price > 100
 */

/*
 * orm_binary_expression_new_compare:
 * @left: Left expression
 * @op: Comparison operator
 * @right: Right expression
 *
 * Creates a comparison expression (=, !=, <, >, etc.).
 *
 * Returns: (transfer full): A new #OrmBinaryExpression
 */
OrmBinaryExpression * orm_binary_expression_new_compare (OrmExpression *left,
                                                         OrmCompareOp   op,
                                                         OrmExpression *right);

/*
 * orm_binary_expression_new_logical:
 * @left: Left expression
 * @op: Logical operator (AND, OR)
 * @right: Right expression
 *
 * Creates a logical expression combining two expressions.
 *
 * Returns: (transfer full): A new #OrmBinaryExpression
 */
OrmBinaryExpression * orm_binary_expression_new_logical (OrmExpression *left,
                                                         OrmLogicalOp   op,
                                                         OrmExpression *right);

/*
 * orm_binary_expression_new_not:
 * @expr: Expression to negate
 *
 * Creates a NOT expression.
 *
 * Returns: (transfer full): A new #OrmBinaryExpression
 */
OrmBinaryExpression * orm_binary_expression_new_not (OrmExpression *expr);

/*
 * orm_binary_expression_get_left:
 * @self: A #OrmBinaryExpression
 *
 * Gets the left operand.
 *
 * Returns: (transfer none): The left expression
 */
OrmExpression * orm_binary_expression_get_left (OrmBinaryExpression *self);

/*
 * orm_binary_expression_get_right:
 * @self: A #OrmBinaryExpression
 *
 * Gets the right operand.
 *
 * Returns: (transfer none) (nullable): The right expression (NULL for NOT)
 */
OrmExpression * orm_binary_expression_get_right (OrmBinaryExpression *self);

/*
 * orm_binary_expression_is_comparison:
 * @self: A #OrmBinaryExpression
 *
 * Checks if this is a comparison expression.
 *
 * Returns: %TRUE if this is a comparison
 */
gboolean orm_binary_expression_is_comparison (OrmBinaryExpression *self);

/*
 * orm_binary_expression_is_logical:
 * @self: A #OrmBinaryExpression
 *
 * Checks if this is a logical expression.
 *
 * Returns: %TRUE if this is a logical operation
 */
gboolean orm_binary_expression_is_logical (OrmBinaryExpression *self);

/*
 * orm_binary_expression_get_compare_op:
 * @self: A #OrmBinaryExpression
 *
 * Gets the comparison operator (only valid if is_comparison is TRUE).
 *
 * Returns: The comparison operator
 */
OrmCompareOp orm_binary_expression_get_compare_op (OrmBinaryExpression *self);

/*
 * orm_binary_expression_get_logical_op:
 * @self: A #OrmBinaryExpression
 *
 * Gets the logical operator (only valid if is_logical is TRUE).
 *
 * Returns: The logical operator
 */
OrmLogicalOp orm_binary_expression_get_logical_op (OrmBinaryExpression *self);

G_END_DECLS

#endif /* ORM_BINARY_EXPRESSION_H */
