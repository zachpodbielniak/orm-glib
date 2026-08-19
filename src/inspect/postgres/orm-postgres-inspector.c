/* orm-postgres-inspector.c
 *
 * Copyright 2025 Zach Podbielniak
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

#include "orm-postgres-inspector.h"
#include "../../core/orm-error.h"
#include "../../dialect/orm-dialect.h"
#include "../../engine/orm-engine.h"
#include "../../engine/orm-result.h"
#include "../../engine/orm-row.h"

#include <string.h>

/*
 * The PostgreSQL schema inspector.
 *
 * Every answer here comes from `pg_catalog` (with `information_schema` for
 * the column list, because that is where the SQL-standard spelling of a
 * column's type lives) read through the ordinary query API.  Nothing in
 * this file touches libpq: an inspector is a caller of the connection like
 * any other, and going around it would mean reimplementing parameter
 * binding and result decoding that already exist.
 *
 * Two rules run through all of it.
 *
 * Relation and schema names are always *parameters*, never text spliced
 * into the SQL.  A catalog query can compare a name as data, so the one
 * place identifier interpolation would otherwise be needed disappears --
 * and with it the escaping bug that comes with it.  The single exception
 * is the exact-count fallback in estimate_row_count(), which has to name a
 * relation in a FROM clause; that one goes through the dialect's
 * identifier quoting and refuses a name the quoting cannot represent.
 *
 * Every output column is cast to a base type (`::text`, `::int`,
 * `::bigint`).  `information_schema` columns are domains -- `sql_identifier`,
 * `character_data`, `cardinal_number` -- whose OIDs are assigned at initdb
 * time and are therefore not in the driver's OID table, so an uncast column
 * would arrive as a string of unpredictable provenance rather than the type
 * it looks like.  Casting makes the decoded #OrmValue type predictable, and
 * the readers below rely on that.
 */

struct _OrmPostgresInspector
{
    OrmInspector parent_instance;
};

G_DEFINE_FINAL_TYPE (OrmPostgresInspector, orm_postgres_inspector, ORM_TYPE_INSPECTOR)

/* ------------------------------------------------------------------ */
/* Type mapping                                                       */
/* ------------------------------------------------------------------ */

/*
 * The `information_schema.columns.data_type` spelling of the types worth
 * decoding as something other than text.
 *
 * This mirrors the driver's OID table but is keyed by name: the driver's
 * table is static to its own translation unit, and duplicating the OIDs
 * here would be a second copy to keep in step.  Names are the stabler key
 * anyway -- `information_schema` spells a type the same way in every
 * release, whereas a user-defined type's OID is per-database.
 */
typedef struct
{
    const gchar  *data_type;
    OrmValueType  value_type;
} OrmPostgresDataTypeInfo;

static const OrmPostgresDataTypeInfo orm_postgres_data_types[] = {
    { "boolean",                     ORM_VALUE_BOOLEAN  },
    { "smallint",                    ORM_VALUE_INTEGER  },
    { "integer",                     ORM_VALUE_INTEGER  },
    { "bigint",                      ORM_VALUE_INTEGER  },
    { "real",                        ORM_VALUE_FLOAT    },
    { "double precision",            ORM_VALUE_FLOAT    },
    { "numeric",                     ORM_VALUE_FLOAT    },
    { "bytea",                       ORM_VALUE_BLOB     },
    { "date",                        ORM_VALUE_DATETIME },
    { "timestamp without time zone", ORM_VALUE_DATETIME },
    { "timestamp with time zone",    ORM_VALUE_DATETIME },
    { NULL,                          ORM_VALUE_NULL     }
};

/*
 * Decides what a column's values decode to from its declared type name.
 *
 * Anything unlisted reads as text, which is what PostgreSQL sends for a
 * user-defined type, an array, an enum or a domain in the text protocol
 * the driver uses.
 */
static OrmValueType
orm_postgres_inspector_value_type (const gchar *data_type)
{
    gint i;

    if (data_type == NULL)
        return ORM_VALUE_STRING;

    for (i = 0; orm_postgres_data_types[i].data_type != NULL; i++)
    {
        if (g_strcmp0 (orm_postgres_data_types[i].data_type, data_type) == 0)
            return orm_postgres_data_types[i].value_type;
    }

    return ORM_VALUE_STRING;
}

/*
 * Reads a `pg_constraint` referential action code.
 *
 * confdeltype and confupdtype are a single `"char"`: 'a' no action,
 * 'r' restrict, 'c' cascade, 'n' set null, 'd' set default.
 */
static OrmForeignKeyAction
orm_postgres_inspector_fk_action (const gchar *code)
{
    if (code == NULL)
        return ORM_FK_NO_ACTION;

    switch (code[0])
    {
    case 'r':
        return ORM_FK_RESTRICT;

    case 'c':
        return ORM_FK_CASCADE;

    case 'n':
        return ORM_FK_SET_NULL;

    case 'd':
        return ORM_FK_SET_DEFAULT;

    default:
        return ORM_FK_NO_ACTION;
    }
}

/* ------------------------------------------------------------------ */
/* Query plumbing                                                     */
/* ------------------------------------------------------------------ */

/*
 * Runs one catalog query on the inspected connection.
 *
 * Returns: (transfer full) (nullable): The result, or %NULL on error
 */
static OrmResult *
orm_postgres_inspector_query (OrmInspector  *self,
                              const gchar   *sql,
                              GList         *params,
                              GError       **error)
{
    OrmConnection *connection;

    connection = orm_inspector_get_connection (self);

    if (connection == NULL || !orm_connection_is_open (connection))
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                     "The inspected connection is not open");
        return NULL;
    }

    return orm_connection_query_with_params (connection, sql, params, error);
}

/*
 * Builds the parameter list every query below is written against: the
 * relation name first where there is one, then the schema.
 *
 * A %NULL @schema binds SQL NULL rather than a guess, which is what lets
 * the queries write COALESCE($n, current_schema()) and have the server
 * resolve the default.  Assuming "public" here would be wrong on any
 * connection whose search_path says otherwise.
 *
 * Returns: (transfer full) (element-type OrmValue): The parameters
 */
static GList *
orm_postgres_inspector_params (const gchar *table,
                               const gchar *schema)
{
    GList *params = NULL;

    if (table != NULL)
        params = g_list_append (params, orm_value_new_string (table));

    params = g_list_append (params, (schema != NULL)
                            ? orm_value_new_string (schema)
                            : orm_value_new_null ());

    return params;
}

static void
orm_postgres_inspector_params_free (GList *params)
{
    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

/*
 * Reads a text column of @row.
 *
 * The OrmValue getters assert on the value's type, so a SQL NULL -- or a
 * column that decoded as something else -- has to be caught before asking
 * for the string.  Every caller treats %NULL as "the server had nothing to
 * say", which is the same thing it means in the catalog.
 *
 * Returns: (transfer none) (nullable): The text, or %NULL
 */
static const gchar *
orm_postgres_inspector_text (OrmRow      *row,
                             const gchar *column)
{
    OrmValue *value;

    value = orm_row_get_value_by_name (row, column);

    if (value == NULL || orm_value_get_value_type (value) != ORM_VALUE_STRING)
        return NULL;

    return orm_value_get_string (value);
}

/*
 * Reads an integer column of @row, or @fallback when it is NULL.
 */
static gint64
orm_postgres_inspector_int64 (OrmRow      *row,
                              const gchar *column,
                              gint64       fallback)
{
    OrmValue *value;

    value = orm_row_get_value_by_name (row, column);

    if (value == NULL || orm_value_get_value_type (value) != ORM_VALUE_INTEGER)
        return fallback;

    return orm_value_get_integer (value);
}

/*
 * Reads a boolean column of @row, treating NULL as %FALSE.
 *
 * An integer is accepted as well as a boolean because that is what
 * orm_value_get_boolean() itself accepts, and a catalog column reached
 * through a cast can arrive either way.
 */
static gboolean
orm_postgres_inspector_bool (OrmRow      *row,
                             const gchar *column)
{
    OrmValue     *value;
    OrmValueType  value_type;

    value = orm_row_get_value_by_name (row, column);

    if (value == NULL)
        return FALSE;

    value_type = orm_value_get_value_type (value);

    if (value_type != ORM_VALUE_BOOLEAN && value_type != ORM_VALUE_INTEGER)
        return FALSE;

    return orm_value_get_boolean (value);
}

/*
 * Turns a failed fetch into @error.
 *
 * orm_result_next() returns %FALSE both when the rows run out and when the
 * fetch broke, so a loop that has finished must ask which happened before
 * handing back what it gathered -- otherwise a truncated answer looks like
 * a complete one.
 *
 * Returns: %TRUE if iteration failed and @error was set
 */
static gboolean
orm_postgres_inspector_iteration_failed (OrmResult  *result,
                                         GError    **error)
{
    const GError *iteration_error;

    iteration_error = orm_result_get_error (result);

    if (iteration_error == NULL)
        return FALSE;

    g_propagate_error (error, g_error_copy (iteration_error));
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Schemas and relations                                              */
/* ------------------------------------------------------------------ */

static gchar **
orm_postgres_inspector_list_schemas (OrmInspector  *self,
                                     GError       **error)
{
    /*
     * The `pg_` prefix covers pg_catalog, pg_toast and every pg_temp_N /
     * pg_toast_temp_N a backend creates for itself; the underscore is
     * escaped so LIKE reads it as a literal rather than as its
     * single-character wildcard.
     */
    static const gchar sql[] =
        "SELECT n.nspname::text AS schema_name "
        "FROM pg_catalog.pg_namespace n "
        "WHERE n.nspname NOT LIKE 'pg\\_%' "
        "AND n.nspname <> 'information_schema' "
        "ORDER BY n.nspname";

    g_autoptr(OrmResult) result = NULL;
    g_autoptr(GPtrArray) names = NULL;

    result = orm_postgres_inspector_query (self, sql, NULL, error);
    if (result == NULL)
        return NULL;

    names = g_ptr_array_new_with_free_func (g_free);

    while (orm_result_next (result))
    {
        OrmRow      *row = orm_result_get_row (result);
        const gchar *name;

        if (row == NULL)
            continue;

        name = orm_postgres_inspector_text (row, "schema_name");
        if (name != NULL)
            g_ptr_array_add (names, g_strdup (name));
    }

    if (orm_postgres_inspector_iteration_failed (result, error))
        return NULL;

    g_ptr_array_add (names, NULL);

    return (gchar **) g_ptr_array_free (g_steal_pointer (&names), FALSE);
}

static GPtrArray *
orm_postgres_inspector_list_relations (OrmInspector  *self,
                                       const gchar   *schema,
                                       GError       **error)
{
    /*
     * relkind 'r' is an ordinary table and 'p' a partitioned one -- a
     * partitioned table is the thing a caller names in a query, so it
     * belongs in the list; its individual partitions are ordinary tables
     * and appear on their own.  'v' is a view and 'm' a materialized view,
     * which reads as a view because that is how you query it.
     */
    static const gchar sql[] =
        "SELECT c.relname::text AS relation_name, "
        "n.nspname::text AS schema_name, "
        "c.relkind::text AS relation_kind "
        "FROM pg_catalog.pg_class c "
        "JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace "
        "WHERE c.relkind IN ('r', 'p', 'v', 'm') "
        "AND n.nspname::text = COALESCE($1::text, current_schema()::text) "
        "ORDER BY c.relname";

    g_autoptr(OrmResult) result = NULL;
    g_autoptr(GPtrArray) relations = NULL;
    GList               *params;

    params = orm_postgres_inspector_params (NULL, schema);
    result = orm_postgres_inspector_query (self, sql, params, error);
    orm_postgres_inspector_params_free (params);

    if (result == NULL)
        return NULL;

    relations = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_table_info_unref);

    while (orm_result_next (result))
    {
        OrmRow          *row = orm_result_get_row (result);
        const gchar     *name;
        const gchar     *schema_name;
        const gchar     *relation_kind;
        OrmRelationKind  kind;

        if (row == NULL)
            continue;

        name = orm_postgres_inspector_text (row, "relation_name");
        if (name == NULL)
            continue;

        /*
         * The schema recorded is the one the row came from, not the
         * argument: the argument may have been %NULL, and the record is
         * meant to be usable on its own afterwards.
         */
        schema_name = orm_postgres_inspector_text (row, "schema_name");
        relation_kind = orm_postgres_inspector_text (row, "relation_kind");

        kind = (g_strcmp0 (relation_kind, "v") == 0 ||
                g_strcmp0 (relation_kind, "m") == 0)
            ? ORM_RELATION_VIEW
            : ORM_RELATION_TABLE;

        g_ptr_array_add (relations, orm_table_info_new (name, schema_name, kind));
    }

    if (orm_postgres_inspector_iteration_failed (result, error))
        return NULL;

    return g_steal_pointer (&relations);
}

/* ------------------------------------------------------------------ */
/* Keys                                                               */
/* ------------------------------------------------------------------ */

/*
 * Defined ahead of get_columns() because that reuses it: a column's
 * primary-key flag is the same question asked per column.
 */
static gchar **
orm_postgres_inspector_get_primary_key (OrmInspector  *self,
                                        const gchar   *table,
                                        const gchar   *schema,
                                        GError       **error)
{
    /*
     * conkey is a smallint[] whose *position* is the key position, which
     * is not the same as attribute order: PRIMARY KEY (b, a) stores
     * {2,1}.  unnest ... WITH ORDINALITY carries the array index out as a
     * column so ORDER BY can restore it; ordering by attnum instead would
     * silently reverse that key.
     */
    static const gchar sql[] =
        "SELECT a.attname::text AS column_name "
        "FROM pg_catalog.pg_constraint con "
        "JOIN pg_catalog.pg_class c ON c.oid = con.conrelid "
        "JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace "
        "JOIN LATERAL unnest(con.conkey) WITH ORDINALITY AS k(attnum, ord) "
        "ON TRUE "
        "JOIN pg_catalog.pg_attribute a "
        "ON a.attrelid = con.conrelid AND a.attnum = k.attnum "
        "WHERE con.contype = 'p' "
        "AND c.relname::text = $1::text "
        "AND n.nspname::text = COALESCE($2::text, current_schema()::text) "
        "ORDER BY k.ord";

    g_autoptr(OrmResult) result = NULL;
    g_autoptr(GPtrArray) columns = NULL;
    GList               *params;

    params = orm_postgres_inspector_params (table, schema);
    result = orm_postgres_inspector_query (self, sql, params, error);
    orm_postgres_inspector_params_free (params);

    if (result == NULL)
        return NULL;

    columns = g_ptr_array_new_with_free_func (g_free);

    while (orm_result_next (result))
    {
        OrmRow      *row = orm_result_get_row (result);
        const gchar *name;

        if (row == NULL)
            continue;

        name = orm_postgres_inspector_text (row, "column_name");
        if (name != NULL)
            g_ptr_array_add (columns, g_strdup (name));
    }

    if (orm_postgres_inspector_iteration_failed (result, error))
        return NULL;

    /* No rows means no primary key, which is an empty array, not an error. */
    g_ptr_array_add (columns, NULL);

    return (gchar **) g_ptr_array_free (g_steal_pointer (&columns), FALSE);
}

/* ------------------------------------------------------------------ */
/* Columns                                                            */
/* ------------------------------------------------------------------ */

static GPtrArray *
orm_postgres_inspector_get_columns (OrmInspector  *self,
                                    const gchar   *table,
                                    const gchar   *schema,
                                    GError       **error)
{
    /*
     * information_schema supplies the standard vocabulary -- data_type,
     * is_nullable, column_default, is_identity -- and pg_attribute
     * supplies format_type(), which spells the type the way the DDL did
     * ("character varying(80)", "numeric(10,2)") where data_type flattens
     * the modifier away.  Both are wanted, so both are joined.
     *
     * The join to pg_attribute goes through pg_class/pg_namespace because
     * information_schema hands back names, not OIDs; dropped attributes
     * are excluded so a table that has been ALTERed cannot match one.
     */
    static const gchar sql[] =
        "SELECT c.column_name::text AS column_name, "
        "c.ordinal_position::int AS ordinal_position, "
        "c.data_type::text AS data_type, "
        "c.is_nullable::text AS is_nullable, "
        "c.column_default::text AS column_default, "
        "c.is_identity::text AS is_identity, "
        "pg_catalog.format_type(a.atttypid, a.atttypmod) AS type_name "
        "FROM information_schema.columns c "
        "JOIN pg_catalog.pg_namespace n "
        "ON n.nspname::text = c.table_schema::text "
        "JOIN pg_catalog.pg_class k "
        "ON k.relnamespace = n.oid AND k.relname::text = c.table_name::text "
        "JOIN pg_catalog.pg_attribute a "
        "ON a.attrelid = k.oid AND a.attname::text = c.column_name::text "
        "AND a.attnum > 0 AND NOT a.attisdropped "
        "WHERE c.table_name::text = $1::text "
        "AND c.table_schema::text = COALESCE($2::text, current_schema()::text) "
        "ORDER BY c.ordinal_position";

    g_autoptr(OrmResult) result = NULL;
    g_autoptr(GPtrArray) columns = NULL;
    g_auto(GStrv)        primary_key = NULL;
    GList               *params;

    /*
     * The primary key is fetched as its own query rather than joined in.
     * Joining it would mean unnesting conkey inside a query that already
     * spans information_schema and three catalogs, and the answer is
     * needed in key order elsewhere anyway -- one extra round trip on a
     * metadata path buys a query each half of which is readable.
     */
    primary_key = orm_postgres_inspector_get_primary_key (self, table, schema, error);
    if (primary_key == NULL)
        return NULL;

    params = orm_postgres_inspector_params (table, schema);
    result = orm_postgres_inspector_query (self, sql, params, error);
    orm_postgres_inspector_params_free (params);

    if (result == NULL)
        return NULL;

    columns = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_column_info_unref);

    while (orm_result_next (result))
    {
        OrmRow      *row = orm_result_get_row (result);
        const gchar *name;
        const gchar *data_type;
        const gchar *type_name;
        const gchar *is_nullable;
        const gchar *is_identity;
        const gchar *default_value;
        gboolean     autoincrement;
        gint64       ordinal_position;

        if (row == NULL)
            continue;

        name = orm_postgres_inspector_text (row, "column_name");
        if (name == NULL)
            continue;

        data_type = orm_postgres_inspector_text (row, "data_type");
        type_name = orm_postgres_inspector_text (row, "type_name");
        is_nullable = orm_postgres_inspector_text (row, "is_nullable");
        is_identity = orm_postgres_inspector_text (row, "is_identity");
        default_value = orm_postgres_inspector_text (row, "column_default");

        /*
         * PostgreSQL generates a column's value two ways, and a caller
         * that wants to know whether to omit the column on INSERT cares
         * about both: the old serial idiom, which is a plain default of
         * nextval() over a sequence, and the SQL-standard identity column
         * introduced in 10, which has no default at all.
         */
        autoincrement =
            (default_value != NULL &&
             g_strstr_len (default_value, -1, "nextval(") != NULL) ||
            g_strcmp0 (is_identity, "YES") == 0;

        ordinal_position = orm_postgres_inspector_int64 (row, "ordinal_position", 0);

        g_ptr_array_add (columns,
                         orm_column_info_new (name,
                                              (type_name != NULL) ? type_name : data_type,
                                              orm_postgres_inspector_value_type (data_type),
                                              g_strcmp0 (is_nullable, "YES") == 0,
                                              default_value,
                                              g_strv_contains ((const gchar * const *) primary_key,
                                                               name),
                                              autoincrement,
                                              /* The record counts from zero,
                                               * information_schema from one. */
                                              (gint) (ordinal_position - 1)));
    }

    if (orm_postgres_inspector_iteration_failed (result, error))
        return NULL;

    return g_steal_pointer (&columns);
}

/* ------------------------------------------------------------------ */
/* Indexes                                                            */
/* ------------------------------------------------------------------ */

/*
 * Closes off the columns gathered for one index, appends the record, and
 * leaves @columns empty for the next index.
 */
static void
orm_postgres_inspector_flush_index (GPtrArray   *indexes,
                                    const gchar *name,
                                    gboolean     unique,
                                    GPtrArray   *columns)
{
    g_ptr_array_add (columns, NULL);

    g_ptr_array_add (indexes,
                     orm_index_info_new (name, unique,
                                         (const gchar * const *) columns->pdata));

    g_ptr_array_set_size (columns, 0);
}

static GPtrArray *
orm_postgres_inspector_get_indexes (OrmInspector  *self,
                                    const gchar   *table,
                                    const gchar   *schema,
                                    GError       **error)
{
    /*
     * indkey is an int2vector of attribute numbers in *index* order, which
     * has nothing to do with table order -- an index on (b, a) stores
     * {2,1}, and sorting the join by attnum would report it as (a, b),
     * describing an index that does not exist.  unnest ... WITH ORDINALITY
     * carries the array position out so ORDER BY can restore it.
     *
     * An attnum of 0 marks an expression column of a functional index,
     * which has no pg_attribute row; the LEFT JOIN lets it through and
     * pg_get_indexdef() names it by its source text, so an expression
     * index reports its expression rather than a hole in the column list.
     *
     * The primary key's index is included: it is a real index, and a
     * caller asking what indexes exist wants to be told about it.
     */
    static const gchar sql[] =
        "SELECT ic.relname::text AS index_name, "
        "i.indexrelid::bigint AS index_oid, "
        "i.indisunique AS is_unique, "
        "COALESCE(a.attname::text, "
        "pg_catalog.pg_get_indexdef(i.indexrelid, k.ord::int, TRUE)) "
        "AS column_name "
        "FROM pg_catalog.pg_index i "
        "JOIN pg_catalog.pg_class c ON c.oid = i.indrelid "
        "JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace "
        "JOIN pg_catalog.pg_class ic ON ic.oid = i.indexrelid "
        "JOIN LATERAL unnest(i.indkey::int2[]) WITH ORDINALITY AS k(attnum, ord) "
        "ON TRUE "
        "LEFT JOIN pg_catalog.pg_attribute a "
        "ON a.attrelid = c.oid AND a.attnum = k.attnum "
        "WHERE c.relname::text = $1::text "
        "AND n.nspname::text = COALESCE($2::text, current_schema()::text) "
        "ORDER BY ic.relname, i.indexrelid, k.ord";

    g_autoptr(OrmResult) result = NULL;
    g_autoptr(GPtrArray) indexes = NULL;
    g_autoptr(GPtrArray) columns = NULL;
    g_autofree gchar    *current_name = NULL;
    GList               *params;
    gint64               current_oid = 0;
    gboolean             current_unique = FALSE;
    gboolean             have_index = FALSE;

    params = orm_postgres_inspector_params (table, schema);
    result = orm_postgres_inspector_query (self, sql, params, error);
    orm_postgres_inspector_params_free (params);

    if (result == NULL)
        return NULL;

    indexes = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_index_info_unref);
    columns = g_ptr_array_new_with_free_func (g_free);

    /*
     * One row per index column, ordered so an index's rows are adjacent:
     * the record is emitted when the index OID changes, and once more
     * after the last row.  The OID rather than the name is the group key
     * because it is what actually identifies the index.
     */
    while (orm_result_next (result))
    {
        OrmRow      *row = orm_result_get_row (result);
        const gchar *index_name;
        const gchar *column_name;
        gint64       index_oid;

        if (row == NULL)
            continue;

        index_name = orm_postgres_inspector_text (row, "index_name");
        if (index_name == NULL)
            continue;

        index_oid = orm_postgres_inspector_int64 (row, "index_oid", 0);

        if (!have_index || index_oid != current_oid)
        {
            if (have_index)
                orm_postgres_inspector_flush_index (indexes, current_name,
                                                    current_unique, columns);

            /*
             * Copied because the row -- and every string in it -- is freed
             * by the next orm_result_next().
             */
            g_free (current_name);
            current_name = g_strdup (index_name);
            current_oid = index_oid;
            current_unique = orm_postgres_inspector_bool (row, "is_unique");
            have_index = TRUE;
        }

        column_name = orm_postgres_inspector_text (row, "column_name");
        if (column_name != NULL)
            g_ptr_array_add (columns, g_strdup (column_name));
    }

    if (orm_postgres_inspector_iteration_failed (result, error))
        return NULL;

    if (have_index)
        orm_postgres_inspector_flush_index (indexes, current_name,
                                            current_unique, columns);

    return g_steal_pointer (&indexes);
}

/* ------------------------------------------------------------------ */
/* Foreign keys                                                       */
/* ------------------------------------------------------------------ */

/*
 * Closes off one foreign key, appends the record, and empties both column
 * buffers for the next one.
 */
static void
orm_postgres_inspector_flush_foreign_key (GPtrArray           *keys,
                                          const gchar         *name,
                                          GPtrArray           *columns,
                                          const gchar         *ref_table,
                                          const gchar         *ref_schema,
                                          GPtrArray           *ref_columns,
                                          OrmForeignKeyAction  on_delete,
                                          OrmForeignKeyAction  on_update)
{
    g_ptr_array_add (columns, NULL);
    g_ptr_array_add (ref_columns, NULL);

    if (ref_table != NULL)
        g_ptr_array_add (keys,
                         orm_foreign_key_info_new (name,
                                                   (const gchar * const *) columns->pdata,
                                                   ref_table,
                                                   ref_schema,
                                                   (const gchar * const *) ref_columns->pdata,
                                                   on_delete,
                                                   on_update));

    g_ptr_array_set_size (columns, 0);
    g_ptr_array_set_size (ref_columns, 0);
}

static GPtrArray *
orm_postgres_inspector_get_foreign_keys (OrmInspector  *self,
                                         const gchar   *table,
                                         const gchar   *schema,
                                         GError       **error)
{
    /*
     * conkey and confkey are smallint[] that pair by *position*: the i-th
     * referring column matches the i-th referenced one.  Unnesting them
     * separately, or resolving either through a join that sorts by attnum,
     * pairs the wrong columns as soon as the declaration order differs
     * from the table order -- and the result still looks plausible, which
     * is what makes it worth being careful about.
     *
     * generate_subscripts() walks the shared index once and both arrays
     * are subscripted with it, so the pairing is structural rather than
     * something the ORDER BY has to be trusted to preserve.
     */
    static const gchar sql[] =
        "SELECT con.oid::bigint AS constraint_oid, "
        "con.conname::text AS constraint_name, "
        "att.attname::text AS column_name, "
        "fatt.attname::text AS ref_column_name, "
        "fc.relname::text AS ref_table, "
        "fn.nspname::text AS ref_schema, "
        "con.confdeltype::text AS on_delete_code, "
        "con.confupdtype::text AS on_update_code "
        "FROM pg_catalog.pg_constraint con "
        "JOIN pg_catalog.pg_class c ON c.oid = con.conrelid "
        "JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace "
        "JOIN pg_catalog.pg_class fc ON fc.oid = con.confrelid "
        "JOIN pg_catalog.pg_namespace fn ON fn.oid = fc.relnamespace "
        "JOIN LATERAL generate_subscripts(con.conkey, 1) AS k(i) ON TRUE "
        "JOIN pg_catalog.pg_attribute att "
        "ON att.attrelid = con.conrelid AND att.attnum = con.conkey[k.i] "
        "JOIN pg_catalog.pg_attribute fatt "
        "ON fatt.attrelid = con.confrelid AND fatt.attnum = con.confkey[k.i] "
        "WHERE con.contype = 'f' "
        "AND c.relname::text = $1::text "
        "AND n.nspname::text = COALESCE($2::text, current_schema()::text) "
        "ORDER BY con.conname, con.oid, k.i";

    g_autoptr(OrmResult) result = NULL;
    g_autoptr(GPtrArray) keys = NULL;
    g_autoptr(GPtrArray) columns = NULL;
    g_autoptr(GPtrArray) ref_columns = NULL;
    g_autofree gchar    *current_name = NULL;
    g_autofree gchar    *current_ref_table = NULL;
    g_autofree gchar    *current_ref_schema = NULL;
    GList               *params;
    OrmForeignKeyAction  current_on_delete = ORM_FK_NO_ACTION;
    OrmForeignKeyAction  current_on_update = ORM_FK_NO_ACTION;
    gint64               current_oid = 0;
    gboolean             have_key = FALSE;

    params = orm_postgres_inspector_params (table, schema);
    result = orm_postgres_inspector_query (self, sql, params, error);
    orm_postgres_inspector_params_free (params);

    if (result == NULL)
        return NULL;

    keys = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_foreign_key_info_unref);
    columns = g_ptr_array_new_with_free_func (g_free);
    ref_columns = g_ptr_array_new_with_free_func (g_free);

    while (orm_result_next (result))
    {
        OrmRow      *row = orm_result_get_row (result);
        const gchar *column_name;
        const gchar *ref_column_name;
        gint64       constraint_oid;

        if (row == NULL)
            continue;

        constraint_oid = orm_postgres_inspector_int64 (row, "constraint_oid", 0);

        if (!have_key || constraint_oid != current_oid)
        {
            if (have_key)
                orm_postgres_inspector_flush_foreign_key (keys, current_name, columns,
                                                          current_ref_table,
                                                          current_ref_schema,
                                                          ref_columns,
                                                          current_on_delete,
                                                          current_on_update);

            /* Copied: the row is freed by the next orm_result_next(). */
            g_free (current_name);
            g_free (current_ref_table);
            g_free (current_ref_schema);

            current_name = g_strdup (orm_postgres_inspector_text (row, "constraint_name"));
            current_ref_table = g_strdup (orm_postgres_inspector_text (row, "ref_table"));
            current_ref_schema = g_strdup (orm_postgres_inspector_text (row, "ref_schema"));
            current_on_delete = orm_postgres_inspector_fk_action
                (orm_postgres_inspector_text (row, "on_delete_code"));
            current_on_update = orm_postgres_inspector_fk_action
                (orm_postgres_inspector_text (row, "on_update_code"));
            current_oid = constraint_oid;
            have_key = TRUE;
        }

        column_name = orm_postgres_inspector_text (row, "column_name");
        ref_column_name = orm_postgres_inspector_text (row, "ref_column_name");

        /*
         * Added as a pair or not at all: the two arrays are read
         * positionally by whoever consumes the record, so letting one grow
         * without the other would misalign every column after it.
         */
        if (column_name != NULL && ref_column_name != NULL)
        {
            g_ptr_array_add (columns, g_strdup (column_name));
            g_ptr_array_add (ref_columns, g_strdup (ref_column_name));
        }
    }

    if (orm_postgres_inspector_iteration_failed (result, error))
        return NULL;

    if (have_key)
        orm_postgres_inspector_flush_foreign_key (keys, current_name, columns,
                                                  current_ref_table,
                                                  current_ref_schema,
                                                  ref_columns,
                                                  current_on_delete,
                                                  current_on_update);

    return g_steal_pointer (&keys);
}

/* ------------------------------------------------------------------ */
/* Row count                                                          */
/* ------------------------------------------------------------------ */

/*
 * Counts the rows of @schema.@table exactly.
 *
 * This is the one place a relation has to be named in SQL rather than
 * bound as a parameter, because a FROM clause takes an identifier.  The
 * name comes back from the catalog query that already matched it, and
 * goes through the dialect's quoting, which doubles any quote character
 * inside it -- so a relation named with one is counted like any other
 * rather than closing its own quoting.
 *
 * Returns: The count, or -1 on error
 */
static gint64
orm_postgres_inspector_exact_row_count (OrmInspector  *self,
                                        const gchar   *table,
                                        const gchar   *schema,
                                        GError       **error)
{
    g_autoptr(OrmResult) result = NULL;
    g_autoptr(OrmValue)  scalar = NULL;
    g_autofree gchar    *quoted_schema = NULL;
    g_autofree gchar    *quoted_table = NULL;
    g_autofree gchar    *sql = NULL;
    OrmConnection       *connection;
    OrmEngine           *engine;
    OrmDialect          *dialect;

    connection = orm_inspector_get_connection (self);
    engine = (connection != NULL) ? orm_connection_get_engine (connection) : NULL;
    dialect = (engine != NULL) ? orm_engine_get_dialect (engine) : NULL;

    if (dialect == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                     "Cannot quote an identifier without a dialect");
        return -1;
    }

    quoted_table = orm_dialect_quote_identifier (dialect, table);

    if (schema != NULL)
    {
        quoted_schema = orm_dialect_quote_identifier (dialect, schema);
        sql = g_strdup_printf ("SELECT COUNT(*) AS row_count FROM %s.%s",
                               quoted_schema, quoted_table);
    }
    else
    {
        sql = g_strdup_printf ("SELECT COUNT(*) AS row_count FROM %s",
                               quoted_table);
    }

    result = orm_connection_query (connection, sql, error);
    if (result == NULL)
        return -1;

    scalar = orm_result_get_scalar (result);

    if (scalar == NULL || orm_value_get_value_type (scalar) != ORM_VALUE_INTEGER)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_QUERY_FAILED,
                     "COUNT(*) on \"%s\" returned no number", table);
        return -1;
    }

    return orm_value_get_integer (scalar);
}

static gint64
orm_postgres_inspector_estimate_row_count (OrmInspector  *self,
                                           const gchar   *table,
                                           const gchar   *schema,
                                           gboolean      *is_estimate,
                                           GError       **error)
{
    /*
     * reltuples is what the planner itself uses, so it costs one catalog
     * lookup no matter how large the table is -- which is the whole point,
     * since an exact COUNT(*) scans every row and a table listing cannot
     * afford that.
     */
    static const gchar sql[] =
        "SELECT n.nspname::text AS schema_name, "
        "c.reltuples::bigint AS row_estimate "
        "FROM pg_catalog.pg_class c "
        "JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace "
        "WHERE c.relname::text = $1::text "
        "AND n.nspname::text = COALESCE($2::text, current_schema()::text)";

    g_autoptr(OrmResult) result = NULL;
    g_autofree gchar    *actual_schema = NULL;
    OrmRow              *row;
    GList               *params;
    gint64               estimate;
    gint64               count;

    params = orm_postgres_inspector_params (table, schema);
    result = orm_postgres_inspector_query (self, sql, params, error);
    orm_postgres_inspector_params_free (params);

    if (result == NULL)
        return -1;

    if (!orm_result_next (result))
    {
        if (orm_postgres_inspector_iteration_failed (result, error))
            return -1;

        g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_FOUND,
                     "No relation named \"%s\" in this schema", table);
        return -1;
    }

    row = orm_result_get_row (result);
    if (row == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_QUERY_FAILED,
                     "The catalog lookup for \"%s\" produced no row", table);
        return -1;
    }

    actual_schema = g_strdup (orm_postgres_inspector_text (row, "schema_name"));
    estimate = orm_postgres_inspector_int64 (row, "row_estimate", -1);

    /*
     * A negative reltuples means the relation has never been analyzed --
     * PostgreSQL 14 and later store -1 for that, where older releases
     * stored 0 and were indistinguishable from a genuinely empty table.
     * There is no statistic to report, so the exact count is the only
     * honest answer.
     */
    if (estimate >= 0)
    {
        if (is_estimate != NULL)
            *is_estimate = TRUE;

        return estimate;
    }

    count = orm_postgres_inspector_exact_row_count (self, table, actual_schema, error);
    if (count < 0)
        return -1;

    if (is_estimate != NULL)
        *is_estimate = FALSE;

    return count;
}

/* ------------------------------------------------------------------ */
/* Type                                                               */
/* ------------------------------------------------------------------ */

static void
orm_postgres_inspector_class_init (OrmPostgresInspectorClass *klass)
{
    OrmInspectorClass *inspector_class = ORM_INSPECTOR_CLASS (klass);

    inspector_class->list_schemas = orm_postgres_inspector_list_schemas;
    inspector_class->list_relations = orm_postgres_inspector_list_relations;
    inspector_class->get_columns = orm_postgres_inspector_get_columns;
    inspector_class->get_indexes = orm_postgres_inspector_get_indexes;
    inspector_class->get_foreign_keys = orm_postgres_inspector_get_foreign_keys;
    inspector_class->get_primary_key = orm_postgres_inspector_get_primary_key;
    inspector_class->estimate_row_count = orm_postgres_inspector_estimate_row_count;
}

static void
orm_postgres_inspector_init (OrmPostgresInspector *self)
{
}

/**
 * orm_postgres_inspector_new:
 * @connection: An open #OrmConnection to a PostgreSQL database
 *
 * Creates the PostgreSQL schema inspector.
 *
 * Returns: (transfer full): A new #OrmPostgresInspector
 */
OrmPostgresInspector *
orm_postgres_inspector_new (OrmConnection *connection)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (connection), NULL);

    return g_object_new (ORM_TYPE_POSTGRES_INSPECTOR,
                         "connection", connection,
                         NULL);
}
