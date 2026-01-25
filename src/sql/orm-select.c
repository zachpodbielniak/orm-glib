/* orm-select.c
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

#include "orm-select.h"
#include "orm-literal.h"

#include <stdarg.h>

/*
 * OrmSelect - SELECT statement builder.
 *
 * Builds SELECT statements with support for all common clauses.
 */

typedef struct {
    gchar         *table;
    OrmExpression *condition;
    OrmJoinType    join_type;
} JoinInfo;

typedef struct {
    gchar             *column;
    OrmOrderDirection  direction;
} OrderInfo;

struct _OrmSelect
{
    GObject parent_instance;

    OrmTableClause *from_table;
    GList          *columns;        /* List of OrmExpression */
    OrmExpression  *where_clause;
    GList          *order_by;       /* List of OrderInfo */
    GList          *group_by;       /* List of gchar* */
    OrmExpression  *having_clause;
    GList          *joins;          /* List of JoinInfo */
    gint64          limit_value;
    gint64          offset_value;
    gboolean        use_distinct;
    gboolean        has_limit;
    gboolean        has_offset;
};

G_DEFINE_TYPE (OrmSelect, orm_select, G_TYPE_OBJECT)

static void
join_info_free (JoinInfo *info)
{
    if (info)
    {
        g_free (info->table);
        g_clear_object (&info->condition);
        g_free (info);
    }
}

static void
order_info_free (OrderInfo *info)
{
    if (info)
    {
        g_free (info->column);
        g_free (info);
    }
}

static void
orm_select_finalize (GObject *object)
{
    OrmSelect *self = ORM_SELECT (object);

    g_clear_object (&self->from_table);
    g_list_free_full (self->columns, g_object_unref);
    g_clear_object (&self->where_clause);
    g_list_free_full (self->order_by, (GDestroyNotify) order_info_free);
    g_list_free_full (self->group_by, g_free);
    g_clear_object (&self->having_clause);
    g_list_free_full (self->joins, (GDestroyNotify) join_info_free);

    G_OBJECT_CLASS (orm_select_parent_class)->finalize (object);
}

static void
orm_select_class_init (OrmSelectClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_select_finalize;
}

static void
orm_select_init (OrmSelect *self)
{
    self->from_table = NULL;
    self->columns = NULL;
    self->where_clause = NULL;
    self->order_by = NULL;
    self->group_by = NULL;
    self->having_clause = NULL;
    self->joins = NULL;
    self->limit_value = 0;
    self->offset_value = 0;
    self->use_distinct = FALSE;
    self->has_limit = FALSE;
    self->has_offset = FALSE;
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
 * orm_select_new:
 *
 * Creates a new SELECT statement builder.
 *
 * Returns: (transfer full): A new #OrmSelect
 */
OrmSelect *
orm_select_new (void)
{
    return g_object_new (ORM_TYPE_SELECT, NULL);
}

/**
 * orm_select_from:
 * @self: A #OrmSelect
 * @table: Table name
 *
 * Sets the FROM table.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_from (OrmSelect   *self,
                 const gchar *table)
{
    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);
    g_return_val_if_fail (table != NULL, NULL);

    g_clear_object (&self->from_table);
    self->from_table = orm_table_clause_new (table);

    return self;
}

/**
 * orm_select_from_clause:
 * @self: A #OrmSelect
 * @table: (transfer full): Table clause with alias
 *
 * Sets the FROM table using a table clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_from_clause (OrmSelect      *self,
                        OrmTableClause *table)
{
    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);
    g_return_val_if_fail (ORM_IS_TABLE_CLAUSE (table), NULL);

    g_clear_object (&self->from_table);
    self->from_table = table;

    return self;
}

/**
 * orm_select_columns:
 * @self: A #OrmSelect
 * @...: Column names (NULL-terminated)
 *
 * Sets the columns to select.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_columns (OrmSelect *self,
                    ...)
{
    va_list args;
    const gchar *column;

    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);

    va_start (args, self);
    while ((column = va_arg (args, const gchar *)) != NULL)
    {
        self->columns = g_list_append (self->columns,
                                       orm_column_element_new (column));
    }
    va_end (args);

    return self;
}

/**
 * orm_select_column:
 * @self: A #OrmSelect
 * @column: Column name
 *
 * Adds a single column to select.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_column (OrmSelect   *self,
                   const gchar *column)
{
    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);
    g_return_val_if_fail (column != NULL, NULL);

    self->columns = g_list_append (self->columns,
                                   orm_column_element_new (column));
    return self;
}

/**
 * orm_select_column_expr:
 * @self: A #OrmSelect
 * @expr: (transfer full): Column expression
 *
 * Adds an expression to the select list.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_column_expr (OrmSelect     *self,
                        OrmExpression *expr)
{
    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (expr), NULL);

    self->columns = g_list_append (self->columns, expr);
    return self;
}

/**
 * orm_select_where:
 * @self: A #OrmSelect
 * @condition: (transfer full): WHERE condition
 *
 * Sets the WHERE condition.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_where (OrmSelect     *self,
                  OrmExpression *condition)
{
    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (condition), NULL);

    g_clear_object (&self->where_clause);
    self->where_clause = condition;

    return self;
}

/**
 * orm_select_and_where:
 * @self: A #OrmSelect
 * @condition: (transfer full): Additional condition
 *
 * Adds an AND condition to the WHERE clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_and_where (OrmSelect     *self,
                      OrmExpression *condition)
{
    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);
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
 * orm_select_or_where:
 * @self: A #OrmSelect
 * @condition: (transfer full): Additional condition
 *
 * Adds an OR condition to the WHERE clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_or_where (OrmSelect     *self,
                     OrmExpression *condition)
{
    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (condition), NULL);

    if (self->where_clause == NULL)
    {
        self->where_clause = condition;
    }
    else
    {
        self->where_clause = orm_expression_or (self->where_clause, condition);
    }

    return self;
}

/**
 * orm_select_order_by:
 * @self: A #OrmSelect
 * @column: Column name
 * @direction: Sort direction
 *
 * Adds an ORDER BY clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_order_by (OrmSelect         *self,
                     const gchar       *column,
                     OrmOrderDirection  direction)
{
    OrderInfo *info;

    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);
    g_return_val_if_fail (column != NULL, NULL);

    info = g_new0 (OrderInfo, 1);
    info->column = g_strdup (column);
    info->direction = direction;

    self->order_by = g_list_append (self->order_by, info);

    return self;
}

/**
 * orm_select_limit:
 * @self: A #OrmSelect
 * @limit: Maximum rows to return
 *
 * Sets the LIMIT.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_limit (OrmSelect *self,
                  gint64     limit)
{
    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);

    self->limit_value = limit;
    self->has_limit = TRUE;

    return self;
}

/**
 * orm_select_offset:
 * @self: A #OrmSelect
 * @offset: Rows to skip
 *
 * Sets the OFFSET.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_offset (OrmSelect *self,
                   gint64     offset)
{
    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);

    self->offset_value = offset;
    self->has_offset = TRUE;

    return self;
}

/**
 * orm_select_distinct:
 * @self: A #OrmSelect
 * @distinct: Whether to use DISTINCT
 *
 * Enables or disables DISTINCT.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_distinct (OrmSelect *self,
                     gboolean   distinct)
{
    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);

    self->use_distinct = distinct;

    return self;
}

/**
 * orm_select_group_by:
 * @self: A #OrmSelect
 * @column: Column to group by
 *
 * Adds a GROUP BY column.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_group_by (OrmSelect   *self,
                     const gchar *column)
{
    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);
    g_return_val_if_fail (column != NULL, NULL);

    self->group_by = g_list_append (self->group_by, g_strdup (column));

    return self;
}

/**
 * orm_select_having:
 * @self: A #OrmSelect
 * @condition: (transfer full): HAVING condition
 *
 * Sets the HAVING condition.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_having (OrmSelect     *self,
                   OrmExpression *condition)
{
    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (condition), NULL);

    g_clear_object (&self->having_clause);
    self->having_clause = condition;

    return self;
}

/**
 * orm_select_join:
 * @self: A #OrmSelect
 * @table: Table to join
 * @condition: (transfer full): Join condition
 * @join_type: Type of join
 *
 * Adds a JOIN clause.
 *
 * Returns: (transfer none): @self for chaining
 */
OrmSelect *
orm_select_join (OrmSelect     *self,
                 const gchar   *table,
                 OrmExpression *condition,
                 OrmJoinType    join_type)
{
    JoinInfo *info;

    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);
    g_return_val_if_fail (table != NULL, NULL);
    g_return_val_if_fail (ORM_IS_EXPRESSION (condition), NULL);

    info = g_new0 (JoinInfo, 1);
    info->table = g_strdup (table);
    info->condition = condition;
    info->join_type = join_type;

    self->joins = g_list_append (self->joins, info);

    return self;
}

/*
 * Get join type string.
 */
static const gchar *
join_type_to_sql (OrmJoinType type)
{
    switch (type)
    {
    case ORM_JOIN_INNER:
        return "INNER JOIN";
    case ORM_JOIN_LEFT:
        return "LEFT JOIN";
    case ORM_JOIN_RIGHT:
        return "RIGHT JOIN";
    case ORM_JOIN_FULL:
        return "FULL OUTER JOIN";
    case ORM_JOIN_CROSS:
        return "CROSS JOIN";
    default:
        return "JOIN";
    }
}

/**
 * orm_select_compile:
 * @self: A #OrmSelect
 * @dialect: Database dialect
 * @params: (out) (element-type OrmValue) (optional): Parameter values
 *
 * Compiles the SELECT statement to SQL.
 *
 * Returns: (transfer full): The compiled SQL
 */
gchar *
orm_select_compile (OrmSelect      *self,
                    OrmDialectType  dialect,
                    GList         **params)
{
    GString *sql;
    GList *l;
    gboolean first;

    g_return_val_if_fail (ORM_IS_SELECT (self), NULL);
    g_return_val_if_fail (self->from_table != NULL, NULL);

    sql = g_string_new ("SELECT ");

    /* DISTINCT */
    if (self->use_distinct)
    {
        g_string_append (sql, "DISTINCT ");
    }

    /* Columns */
    if (self->columns == NULL)
    {
        g_string_append (sql, "*");
    }
    else
    {
        first = TRUE;
        for (l = self->columns; l != NULL; l = l->next)
        {
            OrmExpression *expr = ORM_EXPRESSION (l->data);
            g_autofree gchar *col_sql = NULL;

            if (!first)
            {
                g_string_append (sql, ", ");
            }

            col_sql = orm_expression_compile (expr, dialect, params);
            g_string_append (sql, col_sql);
            first = FALSE;
        }
    }

    /* FROM */
    {
        g_autofree gchar *from_sql = NULL;
        g_string_append (sql, " FROM ");
        from_sql = orm_expression_compile (ORM_EXPRESSION (self->from_table),
                                           dialect, params);
        g_string_append (sql, from_sql);
    }

    /* JOINs */
    for (l = self->joins; l != NULL; l = l->next)
    {
        JoinInfo *join = (JoinInfo *) l->data;
        g_autofree gchar *quoted_table = NULL;
        g_autofree gchar *cond_sql = NULL;

        g_string_append_c (sql, ' ');
        g_string_append (sql, join_type_to_sql (join->join_type));
        g_string_append_c (sql, ' ');

        quoted_table = quote_identifier (join->table, dialect);
        g_string_append (sql, quoted_table);

        if (join->join_type != ORM_JOIN_CROSS)
        {
            g_string_append (sql, " ON ");
            cond_sql = orm_expression_compile (join->condition, dialect, params);
            g_string_append (sql, cond_sql);
        }
    }

    /* WHERE */
    if (self->where_clause != NULL)
    {
        g_autofree gchar *where_sql = NULL;
        g_string_append (sql, " WHERE ");
        where_sql = orm_expression_compile (self->where_clause, dialect, params);
        g_string_append (sql, where_sql);
    }

    /* GROUP BY */
    if (self->group_by != NULL)
    {
        g_string_append (sql, " GROUP BY ");
        first = TRUE;
        for (l = self->group_by; l != NULL; l = l->next)
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

    /* HAVING */
    if (self->having_clause != NULL)
    {
        g_autofree gchar *having_sql = NULL;
        g_string_append (sql, " HAVING ");
        having_sql = orm_expression_compile (self->having_clause, dialect, params);
        g_string_append (sql, having_sql);
    }

    /* ORDER BY */
    if (self->order_by != NULL)
    {
        g_string_append (sql, " ORDER BY ");
        first = TRUE;
        for (l = self->order_by; l != NULL; l = l->next)
        {
            OrderInfo *order = (OrderInfo *) l->data;
            g_autofree gchar *quoted = NULL;

            if (!first)
            {
                g_string_append (sql, ", ");
            }

            quoted = quote_identifier (order->column, dialect);
            g_string_append (sql, quoted);
            g_string_append (sql, order->direction == ORM_ORDER_ASC ? " ASC" : " DESC");
            first = FALSE;
        }
    }

    /* LIMIT */
    if (self->has_limit)
    {
        g_string_append_printf (sql, " LIMIT %" G_GINT64_FORMAT, self->limit_value);
    }

    /* OFFSET */
    if (self->has_offset)
    {
        g_string_append_printf (sql, " OFFSET %" G_GINT64_FORMAT, self->offset_value);
    }

    return g_string_free (sql, FALSE);
}
