/* orm-expression.c
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

#include "orm-expression.h"
#include "orm-binary-expression.h"

/*
 * OrmExpression - Base class for SQL expressions.
 *
 * This is an abstract base class that provides the foundation for
 * all SQL expression types. Subclasses include:
 * - OrmColumnElement: Column references
 * - OrmLiteral: Literal values
 * - OrmBinaryExpression: Binary operations (AND, OR, =, etc.)
 * - OrmUnaryExpression: Unary operations (NOT, -)
 */

G_DEFINE_TYPE (OrmExpression, orm_expression, G_TYPE_OBJECT)

static gchar *
orm_expression_default_compile (OrmExpression  *self,
                                OrmDialectType  dialect,
                                GList         **params)
{
    (void) self;
    (void) dialect;
    (void) params;

    /* Subclasses must override this */
    g_warning ("OrmExpression::compile not implemented for %s",
               G_OBJECT_TYPE_NAME (self));
    return g_strdup ("/* unimplemented */");
}

static OrmExpression *
orm_expression_default_clone (OrmExpression *self)
{
    (void) self;

    /* Subclasses must override this */
    g_warning ("OrmExpression::clone not implemented for %s",
               G_OBJECT_TYPE_NAME (self));
    return NULL;
}

static OrmExpression *
orm_expression_default_negate (OrmExpression *self)
{
    /*
     * Default implementation: wrap in a NOT binary expression.
     * Subclasses may provide more efficient implementations.
     */
    return ORM_EXPRESSION (orm_binary_expression_new_not (self));
}

static void
orm_expression_class_init (OrmExpressionClass *klass)
{
    klass->compile = orm_expression_default_compile;
    klass->clone = orm_expression_default_clone;
    klass->negate = orm_expression_default_negate;
}

static void
orm_expression_init (OrmExpression *self)
{
    (void) self;
}

/**
 * orm_expression_compile:
 * @self: An #OrmExpression
 * @dialect: The database dialect to compile for
 * @params: (out) (element-type OrmValue) (optional): List to receive
 *          parameter values when using parameterized queries
 *
 * Compiles the expression to a SQL string appropriate for the given
 * database dialect. If @params is provided, literal values will be
 * replaced with parameter placeholders and the actual values appended
 * to the list.
 *
 * Returns: (transfer full): The compiled SQL string
 */
gchar *
orm_expression_compile (OrmExpression  *self,
                        OrmDialectType  dialect,
                        GList         **params)
{
    OrmExpressionClass *klass;

    g_return_val_if_fail (ORM_IS_EXPRESSION (self), NULL);

    klass = ORM_EXPRESSION_GET_CLASS (self);
    g_return_val_if_fail (klass->compile != NULL, NULL);

    return klass->compile (self, dialect, params);
}

/**
 * orm_expression_clone:
 * @self: An #OrmExpression
 *
 * Creates a deep copy of the expression tree. This is useful when
 * building queries that reuse expression components.
 *
 * Returns: (transfer full): A new #OrmExpression that is a copy of @self
 */
OrmExpression *
orm_expression_clone (OrmExpression *self)
{
    OrmExpressionClass *klass;

    g_return_val_if_fail (ORM_IS_EXPRESSION (self), NULL);

    klass = ORM_EXPRESSION_GET_CLASS (self);
    g_return_val_if_fail (klass->clone != NULL, NULL);

    return klass->clone (self);
}

/**
 * orm_expression_negate:
 * @self: An #OrmExpression
 *
 * Returns the logical negation of the expression. For example,
 * negating (a = 1) produces NOT(a = 1).
 *
 * Returns: (transfer full): A new expression representing NOT(@self)
 */
OrmExpression *
orm_expression_negate (OrmExpression *self)
{
    OrmExpressionClass *klass;

    g_return_val_if_fail (ORM_IS_EXPRESSION (self), NULL);

    klass = ORM_EXPRESSION_GET_CLASS (self);
    g_return_val_if_fail (klass->negate != NULL, NULL);

    return klass->negate (self);
}

/**
 * orm_expression_and:
 * @self: An #OrmExpression
 * @other: Another #OrmExpression
 *
 * Creates a new expression that represents the logical AND of
 * @self and @other.
 *
 * Returns: (transfer full): A new AND expression
 */
OrmExpression *
orm_expression_and (OrmExpression *self,
                    OrmExpression *other)
{
    g_return_val_if_fail (ORM_IS_EXPRESSION (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (other), NULL);

    return ORM_EXPRESSION (orm_binary_expression_new_logical (self,
                                                              ORM_LOGICAL_AND,
                                                              other));
}

/**
 * orm_expression_or:
 * @self: An #OrmExpression
 * @other: Another #OrmExpression
 *
 * Creates a new expression that represents the logical OR of
 * @self and @other.
 *
 * Returns: (transfer full): A new OR expression
 */
OrmExpression *
orm_expression_or (OrmExpression *self,
                   OrmExpression *other)
{
    g_return_val_if_fail (ORM_IS_EXPRESSION (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (other), NULL);

    return ORM_EXPRESSION (orm_binary_expression_new_logical (self,
                                                              ORM_LOGICAL_OR,
                                                              other));
}
