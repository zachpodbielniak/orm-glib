/* orm-binary-expression.c
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

#include "orm-binary-expression.h"

/*
 * OrmBinaryExpression - Binary SQL expression.
 *
 * This class represents binary operations in SQL including:
 * - Comparison: =, !=, <, <=, >, >=, LIKE, IN, etc.
 * - Logical: AND, OR, NOT
 *
 * For unary NOT, the right operand is NULL.
 */

typedef enum {
    EXPR_TYPE_COMPARE,
    EXPR_TYPE_LOGICAL,
    EXPR_TYPE_NOT
} BinaryExprType;

struct _OrmBinaryExpression
{
    OrmExpression parent_instance;

    OrmExpression *left;
    OrmExpression *right;

    BinaryExprType expr_type;
    union {
        OrmCompareOp compare_op;
        OrmLogicalOp logical_op;
    } op;
};

G_DEFINE_TYPE (OrmBinaryExpression, orm_binary_expression, ORM_TYPE_EXPRESSION)

/*
 * Get SQL string for comparison operator.
 */
static const gchar *
compare_op_to_sql (OrmCompareOp op)
{
    switch (op)
    {
    case ORM_OP_EQ:
        return "=";
    case ORM_OP_NE:
        return "!=";
    case ORM_OP_LT:
        return "<";
    case ORM_OP_LE:
        return "<=";
    case ORM_OP_GT:
        return ">";
    case ORM_OP_GE:
        return ">=";
    case ORM_OP_LIKE:
        return "LIKE";
    case ORM_OP_ILIKE:
        return "ILIKE";
    case ORM_OP_IN:
        return "IN";
    case ORM_OP_NOT_IN:
        return "NOT IN";
    case ORM_OP_IS_NULL:
        return "IS NULL";
    case ORM_OP_IS_NOT_NULL:
        return "IS NOT NULL";
    case ORM_OP_BETWEEN:
        return "BETWEEN";
    default:
        return "=";
    }
}

/*
 * Get SQL string for logical operator.
 */
static const gchar *
logical_op_to_sql (OrmLogicalOp op)
{
    switch (op)
    {
    case ORM_LOGICAL_AND:
        return "AND";
    case ORM_LOGICAL_OR:
        return "OR";
    case ORM_LOGICAL_NOT:
        return "NOT";
    default:
        return "AND";
    }
}

/*
 * Compile the binary expression to SQL.
 */
static gchar *
orm_binary_expression_compile_impl (OrmExpression  *expr,
                                    OrmDialectType  dialect,
                                    GList         **params)
{
    OrmBinaryExpression *self = ORM_BINARY_EXPRESSION (expr);
    g_autofree gchar *left_sql = NULL;
    g_autofree gchar *right_sql = NULL;
    const gchar *op_sql;

    /* Compile left operand */
    left_sql = orm_expression_compile (self->left, dialect, params);

    /* Handle NOT (unary) */
    if (self->expr_type == EXPR_TYPE_NOT)
    {
        return g_strdup_printf ("NOT (%s)", left_sql);
    }

    /* Compile right operand */
    if (self->right != NULL)
    {
        right_sql = orm_expression_compile (self->right, dialect, params);
    }

    /* Get operator string */
    if (self->expr_type == EXPR_TYPE_COMPARE)
    {
        op_sql = compare_op_to_sql (self->op.compare_op);

        /* Handle special operators with no right operand */
        if (self->op.compare_op == ORM_OP_IS_NULL ||
            self->op.compare_op == ORM_OP_IS_NOT_NULL)
        {
            return g_strdup_printf ("%s %s", left_sql, op_sql);
        }
    }
    else
    {
        op_sql = logical_op_to_sql (self->op.logical_op);
    }

    /* Format based on expression type */
    if (self->expr_type == EXPR_TYPE_LOGICAL)
    {
        /* Wrap logical operands in parentheses for clarity */
        return g_strdup_printf ("(%s) %s (%s)", left_sql, op_sql, right_sql);
    }
    else
    {
        return g_strdup_printf ("%s %s %s", left_sql, op_sql, right_sql);
    }
}

/*
 * Clone the binary expression.
 */
static OrmExpression *
orm_binary_expression_clone_impl (OrmExpression *expr)
{
    OrmBinaryExpression *self = ORM_BINARY_EXPRESSION (expr);
    OrmBinaryExpression *clone;

    clone = g_object_new (ORM_TYPE_BINARY_EXPRESSION, NULL);
    clone->left = orm_expression_clone (self->left);
    clone->right = self->right ? orm_expression_clone (self->right) : NULL;
    clone->expr_type = self->expr_type;
    clone->op = self->op;

    return ORM_EXPRESSION (clone);
}

/*
 * Negate the binary expression.
 */
static OrmExpression *
orm_binary_expression_negate_impl (OrmExpression *expr)
{
    OrmBinaryExpression *self = ORM_BINARY_EXPRESSION (expr);

    /* Double negation optimization */
    if (self->expr_type == EXPR_TYPE_NOT)
    {
        return orm_expression_clone (self->left);
    }

    /* For comparisons, we can invert some operators */
    if (self->expr_type == EXPR_TYPE_COMPARE)
    {
        OrmCompareOp inverted_op;

        switch (self->op.compare_op)
        {
        case ORM_OP_EQ:
            inverted_op = ORM_OP_NE;
            break;
        case ORM_OP_NE:
            inverted_op = ORM_OP_EQ;
            break;
        case ORM_OP_LT:
            inverted_op = ORM_OP_GE;
            break;
        case ORM_OP_LE:
            inverted_op = ORM_OP_GT;
            break;
        case ORM_OP_GT:
            inverted_op = ORM_OP_LE;
            break;
        case ORM_OP_GE:
            inverted_op = ORM_OP_LT;
            break;
        case ORM_OP_IS_NULL:
            inverted_op = ORM_OP_IS_NOT_NULL;
            break;
        case ORM_OP_IS_NOT_NULL:
            inverted_op = ORM_OP_IS_NULL;
            break;
        case ORM_OP_IN:
            inverted_op = ORM_OP_NOT_IN;
            break;
        case ORM_OP_NOT_IN:
            inverted_op = ORM_OP_IN;
            break;
        default:
            /* Fall through to default NOT wrapping */
            goto wrap_not;
        }

        return ORM_EXPRESSION (orm_binary_expression_new_compare (
            orm_expression_clone (self->left),
            inverted_op,
            self->right ? orm_expression_clone (self->right) : NULL));
    }

wrap_not:
    /* Default: wrap in NOT */
    return ORM_EXPRESSION (orm_binary_expression_new_not (
        orm_expression_clone (expr)));
}

static void
orm_binary_expression_finalize (GObject *object)
{
    OrmBinaryExpression *self = ORM_BINARY_EXPRESSION (object);

    g_clear_object (&self->left);
    g_clear_object (&self->right);

    G_OBJECT_CLASS (orm_binary_expression_parent_class)->finalize (object);
}

static void
orm_binary_expression_class_init (OrmBinaryExpressionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    OrmExpressionClass *expr_class = ORM_EXPRESSION_CLASS (klass);

    object_class->finalize = orm_binary_expression_finalize;

    expr_class->compile = orm_binary_expression_compile_impl;
    expr_class->clone = orm_binary_expression_clone_impl;
    expr_class->negate = orm_binary_expression_negate_impl;
}

static void
orm_binary_expression_init (OrmBinaryExpression *self)
{
    self->left = NULL;
    self->right = NULL;
    self->expr_type = EXPR_TYPE_COMPARE;
    self->op.compare_op = ORM_OP_EQ;
}

/**
 * orm_binary_expression_new_compare:
 * @left: (transfer full): Left expression
 * @op: Comparison operator
 * @right: (transfer full) (nullable): Right expression
 *
 * Creates a new comparison expression.
 *
 * Returns: (transfer full): A new #OrmBinaryExpression
 */
OrmBinaryExpression *
orm_binary_expression_new_compare (OrmExpression *left,
                                   OrmCompareOp   op,
                                   OrmExpression *right)
{
    OrmBinaryExpression *self;

    g_return_val_if_fail (ORM_IS_EXPRESSION (left), NULL);

    self = g_object_new (ORM_TYPE_BINARY_EXPRESSION, NULL);
    self->left = left;
    self->right = right;
    self->expr_type = EXPR_TYPE_COMPARE;
    self->op.compare_op = op;

    return self;
}

/**
 * orm_binary_expression_new_logical:
 * @left: (transfer full): Left expression
 * @op: Logical operator
 * @right: (transfer full): Right expression
 *
 * Creates a new logical expression (AND, OR).
 *
 * Returns: (transfer full): A new #OrmBinaryExpression
 */
OrmBinaryExpression *
orm_binary_expression_new_logical (OrmExpression *left,
                                   OrmLogicalOp   op,
                                   OrmExpression *right)
{
    OrmBinaryExpression *self;

    g_return_val_if_fail (ORM_IS_EXPRESSION (left), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (right), NULL);
    g_return_val_if_fail (op != ORM_LOGICAL_NOT, NULL);

    self = g_object_new (ORM_TYPE_BINARY_EXPRESSION, NULL);
    self->left = left;
    self->right = right;
    self->expr_type = EXPR_TYPE_LOGICAL;
    self->op.logical_op = op;

    return self;
}

/**
 * orm_binary_expression_new_not:
 * @expr: (transfer full): Expression to negate
 *
 * Creates a NOT expression.
 *
 * Returns: (transfer full): A new #OrmBinaryExpression
 */
OrmBinaryExpression *
orm_binary_expression_new_not (OrmExpression *expr)
{
    OrmBinaryExpression *self;

    g_return_val_if_fail (ORM_IS_EXPRESSION (expr), NULL);

    self = g_object_new (ORM_TYPE_BINARY_EXPRESSION, NULL);
    self->left = expr;
    self->right = NULL;
    self->expr_type = EXPR_TYPE_NOT;
    self->op.logical_op = ORM_LOGICAL_NOT;

    return self;
}

/**
 * orm_binary_expression_get_left:
 * @self: A #OrmBinaryExpression
 *
 * Gets the left operand of the expression.
 *
 * Returns: (transfer none): The left expression
 */
OrmExpression *
orm_binary_expression_get_left (OrmBinaryExpression *self)
{
    g_return_val_if_fail (ORM_IS_BINARY_EXPRESSION (self), NULL);
    return self->left;
}

/**
 * orm_binary_expression_get_right:
 * @self: A #OrmBinaryExpression
 *
 * Gets the right operand of the expression.
 *
 * Returns: (transfer none) (nullable): The right expression, or NULL for NOT
 */
OrmExpression *
orm_binary_expression_get_right (OrmBinaryExpression *self)
{
    g_return_val_if_fail (ORM_IS_BINARY_EXPRESSION (self), NULL);
    return self->right;
}

/**
 * orm_binary_expression_is_comparison:
 * @self: A #OrmBinaryExpression
 *
 * Checks if this is a comparison expression (=, !=, <, etc.).
 *
 * Returns: %TRUE if this is a comparison expression
 */
gboolean
orm_binary_expression_is_comparison (OrmBinaryExpression *self)
{
    g_return_val_if_fail (ORM_IS_BINARY_EXPRESSION (self), FALSE);
    return self->expr_type == EXPR_TYPE_COMPARE;
}

/**
 * orm_binary_expression_is_logical:
 * @self: A #OrmBinaryExpression
 *
 * Checks if this is a logical expression (AND, OR, NOT).
 *
 * Returns: %TRUE if this is a logical expression
 */
gboolean
orm_binary_expression_is_logical (OrmBinaryExpression *self)
{
    g_return_val_if_fail (ORM_IS_BINARY_EXPRESSION (self), FALSE);
    return self->expr_type == EXPR_TYPE_LOGICAL || self->expr_type == EXPR_TYPE_NOT;
}

/**
 * orm_binary_expression_get_compare_op:
 * @self: A #OrmBinaryExpression
 *
 * Gets the comparison operator. Only valid if is_comparison() returns TRUE.
 *
 * Returns: The comparison operator
 */
OrmCompareOp
orm_binary_expression_get_compare_op (OrmBinaryExpression *self)
{
    g_return_val_if_fail (ORM_IS_BINARY_EXPRESSION (self), ORM_OP_EQ);
    g_return_val_if_fail (self->expr_type == EXPR_TYPE_COMPARE, ORM_OP_EQ);
    return self->op.compare_op;
}

/**
 * orm_binary_expression_get_logical_op:
 * @self: A #OrmBinaryExpression
 *
 * Gets the logical operator. Only valid if is_logical() returns TRUE.
 *
 * Returns: The logical operator
 */
OrmLogicalOp
orm_binary_expression_get_logical_op (OrmBinaryExpression *self)
{
    g_return_val_if_fail (ORM_IS_BINARY_EXPRESSION (self), ORM_LOGICAL_AND);
    return self->op.logical_op;
}
