/* orm-insert.c
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

#include "orm-insert.h"
#include "orm-literal.h"

#include <stdarg.h>

/*
 * OrmInsert - INSERT statement builder.
 */

typedef struct {
    gchar         *column;
    OrmExpression *value_expr;
} InsertColumn;

struct _OrmInsert
{
    GObject parent_instance;

    gchar   *table_name;
    GList   *columns;       /* List of InsertColumn */
    GList   *returning;     /* List of gchar* */
    gboolean returning_all;
};

G_DEFINE_TYPE (OrmInsert, orm_insert, G_TYPE_OBJECT)

static void
insert_column_free (InsertColumn *col)
{
    if (col)
    {
        g_free (col->column);
        g_clear_object (&col->value_expr);
        g_free (col);
    }
}

static void
orm_insert_finalize (GObject *object)
{
    OrmInsert *self = ORM_INSERT (object);

    g_free (self->table_name);
    g_list_free_full (self->columns, (GDestroyNotify) insert_column_free);
    g_list_free_full (self->returning, g_free);

    G_OBJECT_CLASS (orm_insert_parent_class)->finalize (object);
}

static void
orm_insert_class_init (OrmInsertClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_insert_finalize;
}

static void
orm_insert_init (OrmInsert *self)
{
    self->table_name = NULL;
    self->columns = NULL;
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
 * orm_insert_new:
 * @table: Table name
 *
 * Creates a new INSERT statement builder.
 *
 * Returns: (transfer full): A new #OrmInsert
 */
OrmInsert *
orm_insert_new (const gchar *table)
{
    OrmInsert *self;

    g_return_val_if_fail (table != NULL, NULL);

    self = g_object_new (ORM_TYPE_INSERT, NULL);
    self->table_name = g_strdup (table);

    return self;
}

/**
 * orm_insert_column:
 * @self: A #OrmInsert
 * @column: Column name
 * @value: (transfer full): Value to insert
 *
 * Adds a column-value pair for the insert.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmInsert *
orm_insert_column (OrmInsert   *self,
                   const gchar *column,
                   OrmValue    *value)
{
    InsertColumn *col;

    g_return_val_if_fail (ORM_IS_INSERT (self), NULL);
    g_return_val_if_fail (column != NULL, NULL);
    g_return_val_if_fail (value != NULL, NULL);

    col = g_new0 (InsertColumn, 1);
    col->column = g_strdup (column);
    col->value_expr = ORM_EXPRESSION (orm_literal_new (value));

    self->columns = g_list_append (self->columns, col);

    return self;
}

/**
 * orm_insert_column_expr:
 * @self: A #OrmInsert
 * @column: Column name
 * @expr: (transfer full): Expression for the value
 *
 * Adds a column with an expression value.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmInsert *
orm_insert_column_expr (OrmInsert     *self,
                        const gchar   *column,
                        OrmExpression *expr)
{
    InsertColumn *col;

    g_return_val_if_fail (ORM_IS_INSERT (self), NULL);
    g_return_val_if_fail (column != NULL, NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (expr), NULL);

    col = g_new0 (InsertColumn, 1);
    col->column = g_strdup (column);
    col->value_expr = expr;

    self->columns = g_list_append (self->columns, col);

    return self;
}

/**
 * orm_insert_returning:
 * @self: A #OrmInsert
 * @...: Column names to return (NULL-terminated)
 *
 * Adds RETURNING clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmInsert *
orm_insert_returning (OrmInsert *self,
                      ...)
{
    va_list args;
    const gchar *column;

    g_return_val_if_fail (ORM_IS_INSERT (self), NULL);

    va_start (args, self);
    while ((column = va_arg (args, const gchar *)) != NULL)
    {
        self->returning = g_list_append (self->returning, g_strdup (column));
    }
    va_end (args);

    return self;
}

/**
 * orm_insert_returning_all:
 * @self: A #OrmInsert
 *
 * Adds RETURNING * clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmInsert *
orm_insert_returning_all (OrmInsert *self)
{
    g_return_val_if_fail (ORM_IS_INSERT (self), NULL);

    self->returning_all = TRUE;

    return self;
}

/**
 * orm_insert_compile:
 * @self: A #OrmInsert
 * @dialect: Database dialect
 * @params: (out) (element-type OrmValue) (optional): Parameter values
 *
 * Compiles the INSERT statement to SQL.
 *
 * Returns: (transfer full): The compiled SQL
 */
gchar *
orm_insert_compile (OrmInsert      *self,
                    OrmDialectType  dialect,
                    GList         **params)
{
    GString *sql;
    GList *l;
    gboolean first;
    g_autofree gchar *quoted_table = NULL;

    g_return_val_if_fail (ORM_IS_INSERT (self), NULL);
    g_return_val_if_fail (self->columns != NULL, NULL);

    sql = g_string_new ("INSERT INTO ");

    /* Table name */
    quoted_table = quote_identifier (self->table_name, dialect);
    g_string_append (sql, quoted_table);

    /* Columns */
    g_string_append (sql, " (");
    first = TRUE;
    for (l = self->columns; l != NULL; l = l->next)
    {
        InsertColumn *col = (InsertColumn *) l->data;
        g_autofree gchar *quoted = NULL;

        if (!first)
        {
            g_string_append (sql, ", ");
        }

        quoted = quote_identifier (col->column, dialect);
        g_string_append (sql, quoted);
        first = FALSE;
    }
    g_string_append (sql, ")");

    /* VALUES */
    g_string_append (sql, " VALUES (");
    first = TRUE;
    for (l = self->columns; l != NULL; l = l->next)
    {
        InsertColumn *col = (InsertColumn *) l->data;
        g_autofree gchar *val_sql = NULL;

        if (!first)
        {
            g_string_append (sql, ", ");
        }

        val_sql = orm_expression_compile (col->value_expr, dialect, params);
        g_string_append (sql, val_sql);
        first = FALSE;
    }
    g_string_append (sql, ")");

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
