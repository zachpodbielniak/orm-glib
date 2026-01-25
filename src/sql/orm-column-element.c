/* orm-column-element.c
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

#include "orm-column-element.h"
#include "orm-binary-expression.h"
#include "orm-literal.h"

/*
 * OrmColumnElement - Column reference expression.
 *
 * Represents a reference to a database column. Can be qualified
 * with a table name or alias for use in JOIN queries.
 */

struct _OrmColumnElement
{
    OrmExpression parent_instance;

    gchar *column_name;
    gchar *table_name;
};

G_DEFINE_TYPE (OrmColumnElement, orm_column_element, ORM_TYPE_EXPRESSION)

/*
 * Quote an identifier based on dialect.
 */
static gchar *
quote_identifier (const gchar *identifier, OrmDialectType dialect)
{
    switch (dialect)
    {
    case ORM_DIALECT_MYSQL:
        /* MySQL uses backticks */
        return g_strdup_printf ("`%s`", identifier);
    case ORM_DIALECT_SQLITE:
    case ORM_DIALECT_POSTGRES:
    default:
        /* Standard SQL uses double quotes */
        return g_strdup_printf ("\"%s\"", identifier);
    }
}

/*
 * Compile the column reference to SQL.
 */
static gchar *
orm_column_element_compile_impl (OrmExpression  *expr,
                                 OrmDialectType  dialect,
                                 GList         **params)
{
    OrmColumnElement *self = ORM_COLUMN_ELEMENT (expr);
    g_autofree gchar *quoted_col = NULL;

    (void) params;

    quoted_col = quote_identifier (self->column_name, dialect);

    if (self->table_name != NULL)
    {
        g_autofree gchar *quoted_table = NULL;
        quoted_table = quote_identifier (self->table_name, dialect);
        return g_strdup_printf ("%s.%s", quoted_table, quoted_col);
    }

    return g_steal_pointer (&quoted_col);
}

/*
 * Clone the column element.
 */
static OrmExpression *
orm_column_element_clone_impl (OrmExpression *expr)
{
    OrmColumnElement *self = ORM_COLUMN_ELEMENT (expr);

    if (self->table_name != NULL)
    {
        return ORM_EXPRESSION (orm_column_element_new_with_table (self->table_name,
                                                                  self->column_name));
    }

    return ORM_EXPRESSION (orm_column_element_new (self->column_name));
}

static void
orm_column_element_finalize (GObject *object)
{
    OrmColumnElement *self = ORM_COLUMN_ELEMENT (object);

    g_clear_pointer (&self->column_name, g_free);
    g_clear_pointer (&self->table_name, g_free);

    G_OBJECT_CLASS (orm_column_element_parent_class)->finalize (object);
}

static void
orm_column_element_class_init (OrmColumnElementClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    OrmExpressionClass *expr_class = ORM_EXPRESSION_CLASS (klass);

    object_class->finalize = orm_column_element_finalize;

    expr_class->compile = orm_column_element_compile_impl;
    expr_class->clone = orm_column_element_clone_impl;
}

static void
orm_column_element_init (OrmColumnElement *self)
{
    self->column_name = NULL;
    self->table_name = NULL;
}

/**
 * orm_column_element_new:
 * @column_name: Name of the column
 *
 * Creates a new column reference without a table qualifier.
 *
 * Returns: (transfer full): A new #OrmColumnElement
 */
OrmColumnElement *
orm_column_element_new (const gchar *column_name)
{
    OrmColumnElement *self;

    g_return_val_if_fail (column_name != NULL, NULL);

    self = g_object_new (ORM_TYPE_COLUMN_ELEMENT, NULL);
    self->column_name = g_strdup (column_name);

    return self;
}

/**
 * orm_column_element_new_with_table:
 * @table_name: Table name or alias
 * @column_name: Name of the column
 *
 * Creates a new column reference with a table qualifier.
 *
 * Returns: (transfer full): A new #OrmColumnElement
 */
OrmColumnElement *
orm_column_element_new_with_table (const gchar *table_name,
                                   const gchar *column_name)
{
    OrmColumnElement *self;

    g_return_val_if_fail (table_name != NULL, NULL);
    g_return_val_if_fail (column_name != NULL, NULL);

    self = g_object_new (ORM_TYPE_COLUMN_ELEMENT, NULL);
    self->table_name = g_strdup (table_name);
    self->column_name = g_strdup (column_name);

    return self;
}

/**
 * orm_column_element_get_name:
 * @self: A #OrmColumnElement
 *
 * Gets the column name.
 *
 * Returns: (transfer none): The column name
 */
const gchar *
orm_column_element_get_name (OrmColumnElement *self)
{
    g_return_val_if_fail (ORM_IS_COLUMN_ELEMENT (self), NULL);
    return self->column_name;
}

/**
 * orm_column_element_get_table:
 * @self: A #OrmColumnElement
 *
 * Gets the table name or alias, if set.
 *
 * Returns: (transfer none) (nullable): The table name, or NULL
 */
const gchar *
orm_column_element_get_table (OrmColumnElement *self)
{
    g_return_val_if_fail (ORM_IS_COLUMN_ELEMENT (self), NULL);
    return self->table_name;
}

/**
 * orm_column_element_eq:
 * @self: A #OrmColumnElement
 * @value: (transfer full): Value to compare
 *
 * Creates column = value expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression *
orm_column_element_eq (OrmColumnElement *self,
                       OrmExpression    *value)
{
    g_return_val_if_fail (ORM_IS_COLUMN_ELEMENT (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (value), NULL);

    return ORM_EXPRESSION (orm_binary_expression_new_compare (
        orm_expression_clone (ORM_EXPRESSION (self)),
        ORM_OP_EQ,
        value));
}

/**
 * orm_column_element_ne:
 * @self: A #OrmColumnElement
 * @value: (transfer full): Value to compare
 *
 * Creates column != value expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression *
orm_column_element_ne (OrmColumnElement *self,
                       OrmExpression    *value)
{
    g_return_val_if_fail (ORM_IS_COLUMN_ELEMENT (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (value), NULL);

    return ORM_EXPRESSION (orm_binary_expression_new_compare (
        orm_expression_clone (ORM_EXPRESSION (self)),
        ORM_OP_NE,
        value));
}

/**
 * orm_column_element_lt:
 * @self: A #OrmColumnElement
 * @value: (transfer full): Value to compare
 *
 * Creates column < value expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression *
orm_column_element_lt (OrmColumnElement *self,
                       OrmExpression    *value)
{
    g_return_val_if_fail (ORM_IS_COLUMN_ELEMENT (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (value), NULL);

    return ORM_EXPRESSION (orm_binary_expression_new_compare (
        orm_expression_clone (ORM_EXPRESSION (self)),
        ORM_OP_LT,
        value));
}

/**
 * orm_column_element_le:
 * @self: A #OrmColumnElement
 * @value: (transfer full): Value to compare
 *
 * Creates column <= value expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression *
orm_column_element_le (OrmColumnElement *self,
                       OrmExpression    *value)
{
    g_return_val_if_fail (ORM_IS_COLUMN_ELEMENT (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (value), NULL);

    return ORM_EXPRESSION (orm_binary_expression_new_compare (
        orm_expression_clone (ORM_EXPRESSION (self)),
        ORM_OP_LE,
        value));
}

/**
 * orm_column_element_gt:
 * @self: A #OrmColumnElement
 * @value: (transfer full): Value to compare
 *
 * Creates column > value expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression *
orm_column_element_gt (OrmColumnElement *self,
                       OrmExpression    *value)
{
    g_return_val_if_fail (ORM_IS_COLUMN_ELEMENT (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (value), NULL);

    return ORM_EXPRESSION (orm_binary_expression_new_compare (
        orm_expression_clone (ORM_EXPRESSION (self)),
        ORM_OP_GT,
        value));
}

/**
 * orm_column_element_ge:
 * @self: A #OrmColumnElement
 * @value: (transfer full): Value to compare
 *
 * Creates column >= value expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression *
orm_column_element_ge (OrmColumnElement *self,
                       OrmExpression    *value)
{
    g_return_val_if_fail (ORM_IS_COLUMN_ELEMENT (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (value), NULL);

    return ORM_EXPRESSION (orm_binary_expression_new_compare (
        orm_expression_clone (ORM_EXPRESSION (self)),
        ORM_OP_GE,
        value));
}

/**
 * orm_column_element_like:
 * @self: A #OrmColumnElement
 * @pattern: LIKE pattern string
 *
 * Creates column LIKE pattern expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression *
orm_column_element_like (OrmColumnElement *self,
                         const gchar      *pattern)
{
    g_return_val_if_fail (ORM_IS_COLUMN_ELEMENT (self), NULL);
    g_return_val_if_fail (pattern != NULL, NULL);

    return ORM_EXPRESSION (orm_binary_expression_new_compare (
        orm_expression_clone (ORM_EXPRESSION (self)),
        ORM_OP_LIKE,
        ORM_EXPRESSION (orm_literal_new_string (pattern))));
}

/**
 * orm_column_element_is_null:
 * @self: A #OrmColumnElement
 *
 * Creates column IS NULL expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression *
orm_column_element_is_null (OrmColumnElement *self)
{
    g_return_val_if_fail (ORM_IS_COLUMN_ELEMENT (self), NULL);

    return ORM_EXPRESSION (orm_binary_expression_new_compare (
        orm_expression_clone (ORM_EXPRESSION (self)),
        ORM_OP_IS_NULL,
        NULL));
}

/**
 * orm_column_element_is_not_null:
 * @self: A #OrmColumnElement
 *
 * Creates column IS NOT NULL expression.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression *
orm_column_element_is_not_null (OrmColumnElement *self)
{
    g_return_val_if_fail (ORM_IS_COLUMN_ELEMENT (self), NULL);

    return ORM_EXPRESSION (orm_binary_expression_new_compare (
        orm_expression_clone (ORM_EXPRESSION (self)),
        ORM_OP_IS_NOT_NULL,
        NULL));
}

/**
 * orm_column_element_in:
 * @self: A #OrmColumnElement
 * @values: (element-type OrmExpression) (transfer none): List of values
 *
 * Creates column IN (values) expression.
 * Note: This creates a special IN expression by wrapping the values in
 * an OrmInList (to be implemented), or for now uses a placeholder approach.
 *
 * Returns: (transfer full): A comparison expression
 */
OrmExpression *
orm_column_element_in (OrmColumnElement *self,
                       GList            *values)
{
    (void) values;

    g_return_val_if_fail (ORM_IS_COLUMN_ELEMENT (self), NULL);

    /* TODO: Implement OrmInList expression type for proper IN support */
    g_warning ("orm_column_element_in: IN expressions not yet fully implemented");

    return ORM_EXPRESSION (orm_binary_expression_new_compare (
        orm_expression_clone (ORM_EXPRESSION (self)),
        ORM_OP_IN,
        NULL));
}
