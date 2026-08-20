/* orm-query.c
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

#include "orm-query.h"
#include "orm-session.h"
#include "orm-mapper.h"
#include "orm-serializable.h"
#include "../engine/orm-connection.h"
#include "../engine/orm-result.h"
#include "../engine/orm-row.h"
#include "../core/orm-error.h"

/*
 * GType registration for OrmSortOrder.
 * Declared in this module's header, so it registers here (see the note
 * in orm-session.c).
 */
GType
orm_sort_order_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_SORT_ASC, "ORM_SORT_ASC", "asc" },
            { ORM_SORT_DESC, "ORM_SORT_DESC", "desc" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmSortOrder", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * OrmQuery - High-level ORM query builder.
 *
 * Provides a fluent interface for building queries that return
 * ORM objects through the session's identity map.
 */

/*
 * FilterClause - Represents a single filter condition.
 */
typedef struct
{
    gchar       *property_name;
    gchar       *column_name;
    OrmCompareOp  op;
    OrmValue    *value;
} FilterClause;

static FilterClause *
filter_clause_new (const gchar *property_name,
                   const gchar *column_name,
                   OrmCompareOp  op,
                   OrmValue    *value)
{
    FilterClause *clause = g_new (FilterClause, 1);
    clause->property_name = g_strdup (property_name);
    clause->column_name = g_strdup (column_name);
    clause->op = op;
    clause->value = value != NULL ? orm_value_copy (value) : NULL;
    return clause;
}

static void
filter_clause_free (FilterClause *clause)
{
    if (clause != NULL)
    {
        g_free (clause->property_name);
        g_free (clause->column_name);
        orm_value_free (clause->value);
        g_free (clause);
    }
}

/*
 * OrderClause - Represents an ORDER BY clause.
 */
typedef struct
{
    gchar        *column_name;
    OrmSortOrder  order;
} OrderClause;

static OrderClause *
order_clause_new (const gchar  *column_name,
                  OrmSortOrder  order)
{
    OrderClause *clause = g_new (OrderClause, 1);
    clause->column_name = g_strdup (column_name);
    clause->order = order;
    return clause;
}

static void
order_clause_free (OrderClause *clause)
{
    if (clause != NULL)
    {
        g_free (clause->column_name);
        g_free (clause);
    }
}

struct _OrmQuery
{
    GObject parent_instance;

    OrmSession *session;  /* Weak reference */
    GType       gtype;
    OrmMapper  *mapper;

    /* Query clauses */
    GList      *filters;      /* List of FilterClause */
    GList      *order_by;     /* List of OrderClause */
    gint        limit_value;
    gint        offset_value;
};

G_DEFINE_TYPE (OrmQuery, orm_query, G_TYPE_OBJECT)

static void
orm_query_finalize (GObject *object)
{
    OrmQuery *self = ORM_QUERY (object);

    g_list_free_full (self->filters, (GDestroyNotify) filter_clause_free);
    g_list_free_full (self->order_by, (GDestroyNotify) order_clause_free);

    G_OBJECT_CLASS (orm_query_parent_class)->finalize (object);
}

static void
orm_query_class_init (OrmQueryClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_query_finalize;
}

static void
orm_query_init (OrmQuery *self)
{
    self->session = NULL;
    self->gtype = G_TYPE_NONE;
    self->mapper = NULL;
    self->filters = NULL;
    self->order_by = NULL;
    self->limit_value = -1;
    self->offset_value = 0;
}

/**
 * orm_query_new:
 * @session: The session to query through
 * @gtype: The GType to query
 *
 * Creates a new query for the given type.
 *
 * Returns: (transfer full): A new #OrmQuery
 */
OrmQuery *
orm_query_new (OrmSession *session,
               GType       gtype)
{
    OrmQuery *self;

    g_return_val_if_fail (ORM_IS_SESSION (session), NULL);
    g_return_val_if_fail (gtype != G_TYPE_NONE, NULL);

    self = g_object_new (ORM_TYPE_QUERY, NULL);
    self->session = session;  /* Weak reference */
    self->gtype = gtype;
    self->mapper = orm_session_get_mapper (session, gtype);

    /* Auto-register mapper if needed */
    if (self->mapper == NULL && g_type_is_a (gtype, ORM_TYPE_SERIALIZABLE))
    {
        g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (gtype);
        orm_session_register_mapper (session, mapper);
        self->mapper = orm_session_get_mapper (session, gtype);
    }

    return self;
}

/**
 * orm_query_filter:
 * @self: An #OrmQuery
 * @property_name: The property to filter on
 * @op: The comparison operator
 * @value: The value to compare against
 *
 * Adds a filter condition to the query.
 * Returns self for method chaining.
 *
 * Returns: (transfer none): The query
 */
OrmQuery *
orm_query_filter (OrmQuery    *self,
                  const gchar *property_name,
                  OrmCompareOp  op,
                  OrmValue    *value)
{
    OrmProperty *prop;
    FilterClause *clause;
    const gchar *col_name;

    g_return_val_if_fail (ORM_IS_QUERY (self), NULL);
    g_return_val_if_fail (property_name != NULL, self);

    if (self->mapper == NULL)
    {
        g_warning ("No mapper for query type");
        return self;
    }

    prop = orm_mapper_get_property (self->mapper, property_name);
    col_name = prop != NULL ? orm_property_get_column_name (prop) : property_name;

    clause = filter_clause_new (property_name, col_name, op, value);
    self->filters = g_list_append (self->filters, clause);

    return self;
}

/**
 * orm_query_filter_by:
 * @self: An #OrmQuery
 * @property_name: The property to filter on
 * @value: The value to compare (equality)
 *
 * Adds an equality filter condition.
 * Shorthand for orm_query_filter(query, name, ORM_OP_EQ, value).
 *
 * Returns: (transfer none): The query
 */
OrmQuery *
orm_query_filter_by (OrmQuery    *self,
                     const gchar *property_name,
                     OrmValue    *value)
{
    return orm_query_filter (self, property_name, ORM_OP_EQ, value);
}

/**
 * orm_query_order_by:
 * @self: An #OrmQuery
 * @property_name: The property to order by
 * @order: The sort order
 *
 * Adds an ordering to the query.
 * Returns self for method chaining.
 *
 * Returns: (transfer none): The query
 */
OrmQuery *
orm_query_order_by (OrmQuery     *self,
                    const gchar  *property_name,
                    OrmSortOrder  order)
{
    OrmProperty *prop;
    OrderClause *clause;
    const gchar *col_name;

    g_return_val_if_fail (ORM_IS_QUERY (self), NULL);
    g_return_val_if_fail (property_name != NULL, self);

    if (self->mapper != NULL)
    {
        prop = orm_mapper_get_property (self->mapper, property_name);
        col_name = prop != NULL ? orm_property_get_column_name (prop) : property_name;
    }
    else
    {
        col_name = property_name;
    }

    clause = order_clause_new (col_name, order);
    self->order_by = g_list_append (self->order_by, clause);

    return self;
}

/**
 * orm_query_limit:
 * @self: An #OrmQuery
 * @limit: Maximum number of results
 *
 * Sets the maximum number of results to return.
 *
 * Returns: (transfer none): The query
 */
OrmQuery *
orm_query_limit (OrmQuery *self,
                 gint      limit)
{
    g_return_val_if_fail (ORM_IS_QUERY (self), NULL);
    self->limit_value = limit;
    return self;
}

/**
 * orm_query_offset:
 * @self: An #OrmQuery
 * @offset: Number of results to skip
 *
 * Sets the number of results to skip.
 *
 * Returns: (transfer none): The query
 */
OrmQuery *
orm_query_offset (OrmQuery *self,
                  gint      offset)
{
    g_return_val_if_fail (ORM_IS_QUERY (self), NULL);
    self->offset_value = offset;
    return self;
}

/*
 * Get the SQL operator string for a filter op.
 */
static const gchar *
filter_op_to_sql (OrmCompareOp op)
{
    switch (op)
    {
    case ORM_OP_EQ:          return "=";
    case ORM_OP_NE:          return "!=";
    case ORM_OP_LT:          return "<";
    case ORM_OP_LE:          return "<=";
    case ORM_OP_GT:          return ">";
    case ORM_OP_GE:          return ">=";
    case ORM_OP_LIKE:        return "LIKE";
    case ORM_OP_IN:          return "IN";
    case ORM_OP_IS_NULL:     return "IS NULL";
    case ORM_OP_IS_NOT_NULL: return "IS NOT NULL";
    default:                 return "=";
    }
}

/*
 * Build the SELECT SQL for this query.
 */
static gchar *
build_select_sql (OrmQuery *self,
                  GList   **out_params)
{
    GString *sql;
    GList *params = NULL;
    GList *l;
    gboolean first;

    if (self->mapper == NULL)
    {
        return NULL;
    }

    sql = g_string_new ("SELECT * FROM ");
    g_string_append_printf (sql, "\"%s\"", orm_mapper_get_table_name (self->mapper));

    /* WHERE clause */
    if (self->filters != NULL)
    {
        g_string_append (sql, " WHERE ");
        first = TRUE;

        for (l = self->filters; l != NULL; l = l->next)
        {
            FilterClause *clause = l->data;

            if (!first)
            {
                g_string_append (sql, " AND ");
            }
            first = FALSE;

            g_string_append_printf (sql, "\"%s\" %s",
                                    clause->column_name,
                                    filter_op_to_sql (clause->op));

            /* Add parameter placeholder if needed */
            if (clause->op != ORM_OP_IS_NULL && clause->op != ORM_OP_IS_NOT_NULL)
            {
                g_string_append (sql, " ?");
                params = g_list_append (params,
                                        clause->value != NULL
                                            ? orm_value_copy (clause->value)
                                            : orm_value_new_null ());
            }
        }
    }

    /* ORDER BY clause */
    if (self->order_by != NULL)
    {
        g_string_append (sql, " ORDER BY ");
        first = TRUE;

        for (l = self->order_by; l != NULL; l = l->next)
        {
            OrderClause *clause = l->data;

            if (!first)
            {
                g_string_append (sql, ", ");
            }
            first = FALSE;

            g_string_append_printf (sql, "\"%s\" %s",
                                    clause->column_name,
                                    clause->order == ORM_SORT_DESC ? "DESC" : "ASC");
        }
    }

    /* LIMIT and OFFSET */
    if (self->limit_value >= 0)
    {
        g_string_append_printf (sql, " LIMIT %d", self->limit_value);
    }
    if (self->offset_value > 0)
    {
        g_string_append_printf (sql, " OFFSET %d", self->offset_value);
    }

    *out_params = params;
    return g_string_free (sql, FALSE);
}

/*
 * Create an object from a result row.
 */
static GObject *
row_to_object (OrmQuery *self,
               OrmRow   *row)
{
    GObject *object;
    GList *props;
    GList *l;
    OrmProperty *pk_prop;
    OrmValue *pk_value = NULL;

    /* Get primary key value to check identity map */
    pk_prop = orm_mapper_get_primary_key_property (self->mapper);
    if (pk_prop != NULL)
    {
        const gchar *pk_col = orm_property_get_column_name (pk_prop);
        pk_value = orm_row_get_value_by_name (row, pk_col);

        /*
         * TODO: Check identity map via session for existing object.
         * For now, we always create a new object.
         *
         * Note: pk_value is (transfer none) from orm_row_get_value_by_name,
         * so we don't need to free it - the row owns the memory.
         */
    }
    (void) pk_value; /* Suppress unused variable warning until identity map is implemented */

    /* Create new object */
    object = g_object_new (self->gtype, NULL);
    props = orm_mapper_get_properties (self->mapper);

    for (l = props; l != NULL; l = l->next)
    {
        OrmProperty *prop = ORM_PROPERTY (l->data);
        const gchar *col_name = orm_property_get_column_name (prop);
        const gchar *prop_name = orm_property_get_property_name (prop);
        OrmValue *value = orm_row_get_value_by_name (row, col_name);

        if (value != NULL && ORM_IS_SERIALIZABLE (object))
        {
            gboolean success;

            success = orm_serializable_set_property_value (ORM_SERIALIZABLE (object),
                                                            prop_name, value);
            if (!success)
            {
                g_warning ("Failed to deserialize property '%s' from column '%s'",
                           prop_name, col_name);
                /*
                 * Continue to other properties rather than failing entirely.
                 * Some properties may be optional or have default values.
                 */
            }
        }
    }

    return object;
}

/**
 * orm_query_all:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Executes the query and returns all matching objects.
 *
 * Returns: (transfer full) (element-type GObject) (nullable): List of objects
 */
GList *
orm_query_all (OrmQuery  *self,
               GError   **error)
{
    g_autofree gchar *sql = NULL;
    GList *params = NULL;
    g_autoptr(OrmResult) result = NULL;
    GList *objects = NULL;
    OrmConnection *conn;

    g_return_val_if_fail (ORM_IS_QUERY (self), NULL);

    sql = build_select_sql (self, &params);
    if (sql == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_MAPPER_ERROR,
                     "No mapper configured for query");
        return NULL;
    }

    conn = orm_session_get_connection (self->session);
    result = orm_connection_query_with_params (conn, sql, params, error);
    g_list_free_full (params, (GDestroyNotify) orm_value_free);

    if (result == NULL)
    {
        return NULL;
    }

    while (orm_result_next (result))
    {
        OrmRow *row = orm_result_get_row (result);
        GObject *object = row_to_object (self, row);
        objects = g_list_append (objects, object);
    }

    return objects;
}

/**
 * orm_query_first:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Executes the query and returns the first matching object.
 *
 * Returns: (transfer full) (nullable): The first object, or %NULL
 */
GObject *
orm_query_first (OrmQuery  *self,
                 GError   **error)
{
    gint original_limit;
    GList *results;
    GObject *object = NULL;

    g_return_val_if_fail (ORM_IS_QUERY (self), NULL);

    /* Temporarily set limit to 1 */
    original_limit = self->limit_value;
    self->limit_value = 1;

    results = orm_query_all (self, error);

    self->limit_value = original_limit;

    if (results != NULL)
    {
        object = g_object_ref (results->data);
        g_list_free_full (results, g_object_unref);
    }

    return object;
}

/**
 * orm_query_one:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Executes the query and returns exactly one object.
 * Sets an error if no object or multiple objects are found.
 *
 * Returns: (transfer full) (nullable): The single object
 */
GObject *
orm_query_one (OrmQuery  *self,
               GError   **error)
{
    GList *results;
    GObject *object = NULL;

    g_return_val_if_fail (ORM_IS_QUERY (self), NULL);

    results = orm_query_all (self, error);
    if (results == NULL && error != NULL && *error == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_FOUND,
                     "No result found");
        return NULL;
    }

    if (g_list_length (results) != 1)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_QUERY_FAILED,
                     "Expected exactly one result, got %u",
                     g_list_length (results));
        g_list_free_full (results, g_object_unref);
        return NULL;
    }

    object = g_object_ref (results->data);
    g_list_free_full (results, g_object_unref);

    return object;
}

/**
 * orm_query_one_or_none:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Executes the query and returns one object or none.
 * Sets an error if multiple objects are found.
 *
 * Returns: (transfer full) (nullable): The object or %NULL
 */
GObject *
orm_query_one_or_none (OrmQuery  *self,
                       GError   **error)
{
    GList *results;
    GObject *object = NULL;

    g_return_val_if_fail (ORM_IS_QUERY (self), NULL);

    results = orm_query_all (self, error);
    if (results == NULL)
    {
        return NULL;
    }

    if (g_list_length (results) > 1)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_QUERY_FAILED,
                     "Expected at most one result, got %u",
                     g_list_length (results));
        g_list_free_full (results, g_object_unref);
        return NULL;
    }

    if (results != NULL)
    {
        object = g_object_ref (results->data);
    }
    g_list_free_full (results, g_object_unref);

    return object;
}

/**
 * orm_query_count:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Returns the count of matching objects.
 *
 * Returns: The count, or -1 on error
 */
gint64
orm_query_count (OrmQuery  *self,
                 GError   **error)
{
    GString *sql;
    g_autofree gchar *sql_text = NULL;
    GList *params = NULL;
    g_autoptr(OrmResult) result = NULL;
    OrmConnection *conn;
    OrmValue *value;
    GList *l;
    gboolean first;
    gint64 count;

    g_return_val_if_fail (ORM_IS_QUERY (self), -1);

    if (self->mapper == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_MAPPER_ERROR,
                     "No mapper configured");
        return -1;
    }

    sql = g_string_new ("SELECT COUNT(*) FROM ");
    g_string_append_printf (sql, "\"%s\"", orm_mapper_get_table_name (self->mapper));

    /* WHERE clause */
    if (self->filters != NULL)
    {
        g_string_append (sql, " WHERE ");
        first = TRUE;

        for (l = self->filters; l != NULL; l = l->next)
        {
            FilterClause *clause = l->data;

            if (!first)
            {
                g_string_append (sql, " AND ");
            }
            first = FALSE;

            g_string_append_printf (sql, "\"%s\" %s",
                                    clause->column_name,
                                    filter_op_to_sql (clause->op));

            if (clause->op != ORM_OP_IS_NULL && clause->op != ORM_OP_IS_NOT_NULL)
            {
                g_string_append (sql, " ?");
                params = g_list_append (params,
                                        clause->value != NULL
                                            ? orm_value_copy (clause->value)
                                            : orm_value_new_null ());
            }
        }
    }

    conn = orm_session_get_connection (self->session);

    /*
     * g_string_free with FALSE hands back the buffer, so it needs an
     * owner.  Passed straight as an argument it has none and leaks on
     * every call.
     */
    sql_text = g_string_free (sql, FALSE);
    sql = NULL;

    result = orm_connection_query_with_params (conn, sql_text, params, error);
    g_list_free_full (params, (GDestroyNotify) orm_value_free);

    if (result == NULL)
    {
        return -1;
    }

    value = orm_result_get_scalar (result);
    if (value == NULL)
    {
        return 0;
    }

    count = orm_value_get_integer (value);
    orm_value_free (value);

    return count;
}

/**
 * orm_query_exists:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Checks if any matching objects exist.
 *
 * Returns: %TRUE if at least one object matches
 */
gboolean
orm_query_exists (OrmQuery  *self,
                  GError   **error)
{
    gint64 count;

    g_return_val_if_fail (ORM_IS_QUERY (self), FALSE);

    count = orm_query_count (self, error);
    return count > 0;
}

/**
 * orm_query_delete:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Deletes all matching objects.
 *
 * Returns: Number of deleted objects, or -1 on error
 */
gint64
orm_query_delete (OrmQuery  *self,
                  GError   **error)
{
    GString *sql;
    GList *params = NULL;
    OrmConnection *conn;
    GList *l;
    gboolean first;

    g_return_val_if_fail (ORM_IS_QUERY (self), -1);

    if (self->mapper == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_MAPPER_ERROR,
                     "No mapper configured");
        return -1;
    }

    sql = g_string_new ("DELETE FROM ");
    g_string_append_printf (sql, "\"%s\"", orm_mapper_get_table_name (self->mapper));

    /* WHERE clause */
    if (self->filters != NULL)
    {
        g_string_append (sql, " WHERE ");
        first = TRUE;

        for (l = self->filters; l != NULL; l = l->next)
        {
            FilterClause *clause = l->data;

            if (!first)
            {
                g_string_append (sql, " AND ");
            }
            first = FALSE;

            g_string_append_printf (sql, "\"%s\" %s",
                                    clause->column_name,
                                    filter_op_to_sql (clause->op));

            if (clause->op != ORM_OP_IS_NULL && clause->op != ORM_OP_IS_NOT_NULL)
            {
                g_string_append (sql, " ?");
                params = g_list_append (params,
                                        clause->value != NULL
                                            ? orm_value_copy (clause->value)
                                            : orm_value_new_null ());
            }
        }
    }

    conn = orm_session_get_connection (self->session);
    if (!orm_connection_execute_with_params (conn, sql->str, params, error))
    {
        g_string_free (sql, TRUE);
        g_list_free_full (params, (GDestroyNotify) orm_value_free);
        return -1;
    }

    g_string_free (sql, TRUE);
    g_list_free_full (params, (GDestroyNotify) orm_value_free);

    /* Return affected rows (not all backends support this) */
    return 0;
}

/**
 * orm_query_get_sql:
 * @self: An #OrmQuery
 *
 * Gets the generated SQL for debugging.
 *
 * Returns: (transfer full): The SQL string
 */
gchar *
orm_query_get_sql (OrmQuery *self)
{
    GList *params = NULL;
    gchar *sql;

    g_return_val_if_fail (ORM_IS_QUERY (self), NULL);

    sql = build_select_sql (self, &params);
    g_list_free_full (params, (GDestroyNotify) orm_value_free);

    return sql;
}
