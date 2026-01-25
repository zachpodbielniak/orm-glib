/* orm-delete.c
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

#include "orm-delete.h"

#include <stdarg.h>

/*
 * OrmDelete - DELETE statement builder.
 */

struct _OrmDelete
{
    GObject parent_instance;

    gchar         *table_name;
    OrmExpression *where_clause;
    GList         *returning;       /* List of gchar* */
    gboolean       returning_all;
};

G_DEFINE_TYPE (OrmDelete, orm_delete, G_TYPE_OBJECT)

static void
orm_delete_finalize (GObject *object)
{
    OrmDelete *self = ORM_DELETE (object);

    g_free (self->table_name);
    g_clear_object (&self->where_clause);
    g_list_free_full (self->returning, g_free);

    G_OBJECT_CLASS (orm_delete_parent_class)->finalize (object);
}

static void
orm_delete_class_init (OrmDeleteClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_delete_finalize;
}

static void
orm_delete_init (OrmDelete *self)
{
    self->table_name = NULL;
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
 * orm_delete_new:
 * @table: Table name
 *
 * Creates a new DELETE statement builder.
 *
 * Returns: (transfer full): A new #OrmDelete
 */
OrmDelete *
orm_delete_new (const gchar *table)
{
    OrmDelete *self;

    g_return_val_if_fail (table != NULL, NULL);

    self = g_object_new (ORM_TYPE_DELETE, NULL);
    self->table_name = g_strdup (table);

    return self;
}

/**
 * orm_delete_where:
 * @self: A #OrmDelete
 * @condition: (transfer full): WHERE condition
 *
 * Sets the WHERE condition.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmDelete *
orm_delete_where (OrmDelete     *self,
                  OrmExpression *condition)
{
    g_return_val_if_fail (ORM_IS_DELETE (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (condition), NULL);

    g_clear_object (&self->where_clause);
    self->where_clause = condition;

    return self;
}

/**
 * orm_delete_and_where:
 * @self: A #OrmDelete
 * @condition: (transfer full): Additional condition
 *
 * Adds an AND condition to the WHERE clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmDelete *
orm_delete_and_where (OrmDelete     *self,
                      OrmExpression *condition)
{
    g_return_val_if_fail (ORM_IS_DELETE (self), NULL);
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
 * orm_delete_returning:
 * @self: A #OrmDelete
 * @...: Column names to return (NULL-terminated)
 *
 * Adds RETURNING clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmDelete *
orm_delete_returning (OrmDelete *self,
                      ...)
{
    va_list args;
    const gchar *column;

    g_return_val_if_fail (ORM_IS_DELETE (self), NULL);

    va_start (args, self);
    while ((column = va_arg (args, const gchar *)) != NULL)
    {
        self->returning = g_list_append (self->returning, g_strdup (column));
    }
    va_end (args);

    return self;
}

/**
 * orm_delete_returning_all:
 * @self: A #OrmDelete
 *
 * Adds RETURNING * clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmDelete *
orm_delete_returning_all (OrmDelete *self)
{
    g_return_val_if_fail (ORM_IS_DELETE (self), NULL);

    self->returning_all = TRUE;

    return self;
}

/**
 * orm_delete_compile:
 * @self: A #OrmDelete
 * @dialect: Database dialect
 * @params: (out) (element-type OrmValue) (optional): Parameter values
 *
 * Compiles the DELETE statement to SQL.
 *
 * Returns: (transfer full): The compiled SQL
 */
gchar *
orm_delete_compile (OrmDelete      *self,
                    OrmDialectType  dialect,
                    GList         **params)
{
    GString *sql;
    GList *l;
    gboolean first;
    g_autofree gchar *quoted_table = NULL;

    g_return_val_if_fail (ORM_IS_DELETE (self), NULL);

    sql = g_string_new ("DELETE FROM ");

    /* Table name */
    quoted_table = quote_identifier (self->table_name, dialect);
    g_string_append (sql, quoted_table);

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
