/* orm-update.c
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

#include "orm-update.h"
#include "orm-literal.h"

#include <stdarg.h>

/*
 * OrmUpdate - UPDATE statement builder.
 */

typedef struct {
    gchar         *column;
    OrmExpression *value_expr;
} SetColumn;

struct _OrmUpdate
{
    GObject parent_instance;

    gchar         *table_name;
    GList         *set_columns;     /* List of SetColumn */
    OrmExpression *where_clause;
    GList         *returning;       /* List of gchar* */
    gboolean       returning_all;
};

G_DEFINE_TYPE (OrmUpdate, orm_update, G_TYPE_OBJECT)

static void
set_column_free (SetColumn *col)
{
    if (col)
    {
        g_free (col->column);
        g_clear_object (&col->value_expr);
        g_free (col);
    }
}

static void
orm_update_finalize (GObject *object)
{
    OrmUpdate *self = ORM_UPDATE (object);

    g_free (self->table_name);
    g_list_free_full (self->set_columns, (GDestroyNotify) set_column_free);
    g_clear_object (&self->where_clause);
    g_list_free_full (self->returning, g_free);

    G_OBJECT_CLASS (orm_update_parent_class)->finalize (object);
}

static void
orm_update_class_init (OrmUpdateClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_update_finalize;
}

static void
orm_update_init (OrmUpdate *self)
{
    self->table_name = NULL;
    self->set_columns = NULL;
    self->where_clause = NULL;
    self->returning = NULL;
    self->returning_all = FALSE;
}

/*
 * Quote identifier based on dialect.
 */
static gchar *
quote_identifier (const gchar *identifier, OrmDialectType dialect)
{
    switch (dialect)
    {
    case ORM_DIALECT_MYSQL:
        return g_strdup_printf ("`%s`", identifier);
    case ORM_DIALECT_SQLITE:
    case ORM_DIALECT_POSTGRES:
    default:
        return g_strdup_printf ("\"%s\"", identifier);
    }
}

/**
 * orm_update_new:
 * @table: Table name
 *
 * Creates a new UPDATE statement builder.
 *
 * Returns: (transfer full): A new #OrmUpdate
 */
OrmUpdate *
orm_update_new (const gchar *table)
{
    OrmUpdate *self;

    g_return_val_if_fail (table != NULL, NULL);

    self = g_object_new (ORM_TYPE_UPDATE, NULL);
    self->table_name = g_strdup (table);

    return self;
}

/**
 * orm_update_set:
 * @self: A #OrmUpdate
 * @column: Column name
 * @value: (transfer full): New value
 *
 * Sets a column to a value.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmUpdate *
orm_update_set (OrmUpdate   *self,
                const gchar *column,
                OrmValue    *value)
{
    SetColumn *col;

    g_return_val_if_fail (ORM_IS_UPDATE (self), NULL);
    g_return_val_if_fail (column != NULL, NULL);
    g_return_val_if_fail (value != NULL, NULL);

    col = g_new0 (SetColumn, 1);
    col->column = g_strdup (column);
    col->value_expr = ORM_EXPRESSION (orm_literal_new (value));

    self->set_columns = g_list_append (self->set_columns, col);

    return self;
}

/**
 * orm_update_set_expr:
 * @self: A #OrmUpdate
 * @column: Column name
 * @expr: (transfer full): Expression for the value
 *
 * Sets a column to an expression value.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmUpdate *
orm_update_set_expr (OrmUpdate     *self,
                     const gchar   *column,
                     OrmExpression *expr)
{
    SetColumn *col;

    g_return_val_if_fail (ORM_IS_UPDATE (self), NULL);
    g_return_val_if_fail (column != NULL, NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (expr), NULL);

    col = g_new0 (SetColumn, 1);
    col->column = g_strdup (column);
    col->value_expr = expr;

    self->set_columns = g_list_append (self->set_columns, col);

    return self;
}

/**
 * orm_update_where:
 * @self: A #OrmUpdate
 * @condition: (transfer full): WHERE condition
 *
 * Sets the WHERE condition.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmUpdate *
orm_update_where (OrmUpdate     *self,
                  OrmExpression *condition)
{
    g_return_val_if_fail (ORM_IS_UPDATE (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (condition), NULL);

    g_clear_object (&self->where_clause);
    self->where_clause = condition;

    return self;
}

/**
 * orm_update_and_where:
 * @self: A #OrmUpdate
 * @condition: (transfer full): Additional condition
 *
 * Adds an AND condition to the WHERE clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmUpdate *
orm_update_and_where (OrmUpdate     *self,
                      OrmExpression *condition)
{
    g_return_val_if_fail (ORM_IS_UPDATE (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (condition), NULL);

    if (self->where_clause == NULL)
    {
        self->where_clause = condition;
    }
    else
    {
        self->where_clause = orm_expression_and (self->where_clause, condition);
    }

    return self;
}

/**
 * orm_update_returning:
 * @self: A #OrmUpdate
 * @...: Column names to return (NULL-terminated)
 *
 * Adds RETURNING clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmUpdate *
orm_update_returning (OrmUpdate *self,
                      ...)
{
    va_list args;
    const gchar *column;

    g_return_val_if_fail (ORM_IS_UPDATE (self), NULL);

    va_start (args, self);
    while ((column = va_arg (args, const gchar *)) != NULL)
    {
        self->returning = g_list_append (self->returning, g_strdup (column));
    }
    va_end (args);

    return self;
}

/**
 * orm_update_returning_all:
 * @self: A #OrmUpdate
 *
 * Adds RETURNING * clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmUpdate *
orm_update_returning_all (OrmUpdate *self)
{
    g_return_val_if_fail (ORM_IS_UPDATE (self), NULL);

    self->returning_all = TRUE;

    return self;
}

/**
 * orm_update_compile:
 * @self: A #OrmUpdate
 * @dialect: Database dialect
 * @params: (out) (element-type OrmValue) (optional): Parameter values
 *
 * Compiles the UPDATE statement to SQL.
 *
 * Returns: (transfer full): The compiled SQL
 */
gchar *
orm_update_compile (OrmUpdate      *self,
                    OrmDialectType  dialect,
                    GList         **params)
{
    GString *sql;
    GList *l;
    gboolean first;
    g_autofree gchar *quoted_table = NULL;

    g_return_val_if_fail (ORM_IS_UPDATE (self), NULL);
    g_return_val_if_fail (self->set_columns != NULL, NULL);

    sql = g_string_new ("UPDATE ");

    /* Table name */
    quoted_table = quote_identifier (self->table_name, dialect);
    g_string_append (sql, quoted_table);

    /* SET */
    g_string_append (sql, " SET ");
    first = TRUE;
    for (l = self->set_columns; l != NULL; l = l->next)
    {
        SetColumn *col = (SetColumn *) l->data;
        g_autofree gchar *quoted = NULL;
        g_autofree gchar *val_sql = NULL;

        if (!first)
        {
            g_string_append (sql, ", ");
        }

        quoted = quote_identifier (col->column, dialect);
        g_string_append (sql, quoted);
        g_string_append (sql, " = ");
        val_sql = orm_expression_compile (col->value_expr, dialect, params);
        g_string_append (sql, val_sql);
        first = FALSE;
    }

    /* WHERE */
    if (self->where_clause != NULL)
    {
        g_autofree gchar *where_sql = NULL;
        g_string_append (sql, " WHERE ");
        where_sql = orm_expression_compile (self->where_clause, dialect, params);
        g_string_append (sql, where_sql);
    }

    /* RETURNING */
    if (self->returning_all || self->returning != NULL)
    {
        g_string_append (sql, " RETURNING ");

        if (self->returning_all)
        {
            g_string_append (sql, "*");
        }
        else
        {
            first = TRUE;
            for (l = self->returning; l != NULL; l = l->next)
            {
                const gchar *col = (const gchar *) l->data;
                g_autofree gchar *quoted = NULL;

                if (!first)
                {
                    g_string_append (sql, ", ");
                }

                quoted = quote_identifier (col, dialect);
                g_string_append (sql, quoted);
                first = FALSE;
            }
        }
    }

    return g_string_free (sql, FALSE);
}
