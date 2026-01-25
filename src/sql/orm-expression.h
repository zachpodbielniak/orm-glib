/* orm-expression.h
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

#ifndef ORM_EXPRESSION_H
#define ORM_EXPRESSION_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-enums.h"

G_BEGIN_DECLS

#define ORM_TYPE_EXPRESSION (orm_expression_get_type ())

G_DECLARE_DERIVABLE_TYPE (OrmExpression, orm_expression, ORM, EXPRESSION, GObject)

/*
 * OrmExpression:
 *
 * Base class for all SQL expressions. Expressions represent elements
 * that can appear in SQL statements such as column references, literals,
 * function calls, and compound expressions (AND, OR, etc.).
 *
 * Subclasses must implement the compile() virtual method to generate
 * dialect-specific SQL.
 */

struct _OrmExpressionClass
{
    GObjectClass parent_class;

    /*
     * compile:
     * @self: The expression
     * @dialect: The dialect to compile for (OrmDialectType)
     * @params: (out) (element-type OrmValue) (optional): Parameter values
     *
     * Compiles the expression to a SQL string for the given dialect.
     * If @params is not NULL, literal values should be replaced with
     * parameter placeholders and added to the list.
     *
     * Returns: (transfer full): The compiled SQL string
     */
    gchar * (*compile) (OrmExpression  *self,
                        OrmDialectType  dialect,
                        GList         **params);

    /*
     * clone:
     * @self: The expression
     *
     * Creates a deep copy of the expression.
     *
     * Returns: (transfer full): A new expression
     */
    OrmExpression * (*clone) (OrmExpression *self);

    /*
     * negate:
     * @self: The expression
     *
     * Returns the logical negation of the expression.
     * Default implementation wraps in NOT(...).
     *
     * Returns: (transfer full): A new negated expression
     */
    OrmExpression * (*negate) (OrmExpression *self);

    gpointer _reserved[8];
};

/*
 * orm_expression_compile:
 * @self: An #OrmExpression
 * @dialect: The database dialect
 * @params: (out) (element-type OrmValue) (optional): Parameter list
 *
 * Compiles the expression to SQL for the specified dialect.
 *
 * Returns: (transfer full): The compiled SQL string
 */
gchar *         orm_expression_compile      (OrmExpression  *self,
                                             OrmDialectType  dialect,
                                             GList         **params);

/*
 * orm_expression_clone:
 * @self: An #OrmExpression
 *
 * Creates a deep copy of the expression.
 *
 * Returns: (transfer full): A new #OrmExpression
 */
OrmExpression * orm_expression_clone        (OrmExpression *self);

/*
 * orm_expression_negate:
 * @self: An #OrmExpression
 *
 * Returns the logical negation of the expression.
 *
 * Returns: (transfer full): A negated #OrmExpression
 */
OrmExpression * orm_expression_negate       (OrmExpression *self);

/*
 * orm_expression_and:
 * @self: An #OrmExpression
 * @other: Another #OrmExpression
 *
 * Creates an AND expression combining self and other.
 *
 * Returns: (transfer full): A new AND expression
 */
OrmExpression * orm_expression_and          (OrmExpression *self,
                                             OrmExpression *other);

/*
 * orm_expression_or:
 * @self: An #OrmExpression
 * @other: Another #OrmExpression
 *
 * Creates an OR expression combining self and other.
 *
 * Returns: (transfer full): A new OR expression
 */
OrmExpression * orm_expression_or           (OrmExpression *self,
                                             OrmExpression *other);

G_END_DECLS

#endif /* ORM_EXPRESSION_H */
