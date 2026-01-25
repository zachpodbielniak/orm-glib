/* orm-table-clause.c
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

#include "orm-table-clause.h"

/*
 * OrmTableClause - Table reference expression.
 *
 * Represents a reference to a database table, optionally with an alias.
 * Used in FROM clauses and JOIN operations.
 */

struct _OrmTableClause
{
    OrmExpression parent_instance;

    gchar *table_name;
    gchar *alias;
};

G_DEFINE_TYPE (OrmTableClause, orm_table_clause, ORM_TYPE_EXPRESSION)

/*
 * Quote an identifier based on dialect.
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

/*
 * Compile the table clause to SQL.
 */
static gchar *
orm_table_clause_compile_impl (OrmExpression  *expr,
                               OrmDialectType  dialect,
                               GList         **params)
{
    OrmTableClause *self = ORM_TABLE_CLAUSE (expr);
    g_autofree gchar *quoted_name = NULL;

    (void) params;

    quoted_name = quote_identifier (self->table_name, dialect);

    if (self->alias != NULL)
    {
        g_autofree gchar *quoted_alias = NULL;
        quoted_alias = quote_identifier (self->alias, dialect);
        return g_strdup_printf ("%s AS %s", quoted_name, quoted_alias);
    }

    return g_steal_pointer (&quoted_name);
}

/*
 * Clone the table clause.
 */
static OrmExpression *
orm_table_clause_clone_impl (OrmExpression *expr)
{
    OrmTableClause *self = ORM_TABLE_CLAUSE (expr);

    if (self->alias != NULL)
    {
        return ORM_EXPRESSION (orm_table_clause_new_with_alias (self->table_name,
                                                                self->alias));
    }

    return ORM_EXPRESSION (orm_table_clause_new (self->table_name));
}

static void
orm_table_clause_finalize (GObject *object)
{
    OrmTableClause *self = ORM_TABLE_CLAUSE (object);

    g_clear_pointer (&self->table_name, g_free);
    g_clear_pointer (&self->alias, g_free);

    G_OBJECT_CLASS (orm_table_clause_parent_class)->finalize (object);
}

static void
orm_table_clause_class_init (OrmTableClauseClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    OrmExpressionClass *expr_class = ORM_EXPRESSION_CLASS (klass);

    object_class->finalize = orm_table_clause_finalize;

    expr_class->compile = orm_table_clause_compile_impl;
    expr_class->clone = orm_table_clause_clone_impl;
}

static void
orm_table_clause_init (OrmTableClause *self)
{
    self->table_name = NULL;
    self->alias = NULL;
}

/**
 * orm_table_clause_new:
 * @table_name: Name of the table
 *
 * Creates a new table clause without an alias.
 *
 * Returns: (transfer full): A new #OrmTableClause
 */
OrmTableClause *
orm_table_clause_new (const gchar *table_name)
{
    OrmTableClause *self;

    g_return_val_if_fail (table_name != NULL, NULL);

    self = g_object_new (ORM_TYPE_TABLE_CLAUSE, NULL);
    self->table_name = g_strdup (table_name);

    return self;
}

/**
 * orm_table_clause_new_with_alias:
 * @table_name: Name of the table
 * @alias: Table alias
 *
 * Creates a new table clause with an alias.
 *
 * Returns: (transfer full): A new #OrmTableClause
 */
OrmTableClause *
orm_table_clause_new_with_alias (const gchar *table_name,
                                 const gchar *alias)
{
    OrmTableClause *self;

    g_return_val_if_fail (table_name != NULL, NULL);
    g_return_val_if_fail (alias != NULL, NULL);

    self = g_object_new (ORM_TYPE_TABLE_CLAUSE, NULL);
    self->table_name = g_strdup (table_name);
    self->alias = g_strdup (alias);

    return self;
}

/**
 * orm_table_clause_get_name:
 * @self: A #OrmTableClause
 *
 * Gets the table name.
 *
 * Returns: (transfer none): The table name
 */
const gchar *
orm_table_clause_get_name (OrmTableClause *self)
{
    g_return_val_if_fail (ORM_IS_TABLE_CLAUSE (self), NULL);
    return self->table_name;
}

/**
 * orm_table_clause_get_alias:
 * @self: A #OrmTableClause
 *
 * Gets the table alias, if set.
 *
 * Returns: (transfer none) (nullable): The alias, or NULL
 */
const gchar *
orm_table_clause_get_alias (OrmTableClause *self)
{
    g_return_val_if_fail (ORM_IS_TABLE_CLAUSE (self), NULL);
    return self->alias;
}

/**
 * orm_table_clause_get_effective_name:
 * @self: A #OrmTableClause
 *
 * Gets the alias if set, otherwise the table name. This is useful
 * for referencing the table in column qualifiers.
 *
 * Returns: (transfer none): The effective name
 */
const gchar *
orm_table_clause_get_effective_name (OrmTableClause *self)
{
    g_return_val_if_fail (ORM_IS_TABLE_CLAUSE (self), NULL);

    if (self->alias != NULL)
    {
        return self->alias;
    }

    return self->table_name;
}
