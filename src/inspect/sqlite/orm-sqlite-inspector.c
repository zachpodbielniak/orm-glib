/* orm-sqlite-inspector.c
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

#include "orm-sqlite-inspector.h"
#include "../../core/orm-error.h"
#include "../../dialect/orm-dialect.h"
#include "../../engine/orm-engine.h"
#include "../../engine/orm-result.h"
#include "../../engine/orm-row.h"

#include <string.h>

/*
 * The SQLite schema inspector.
 *
 * SQLite answers every question here from one of two places: `sqlite_master`,
 * the catalog table holding the text of each CREATE statement, or one of the
 * introspection PRAGMAs, which parse that text so nobody else has to.
 *
 * Both are read through the ordinary query API rather than through the
 * sqlite3 handle, and this file includes no <sqlite3.h> at all. Nothing here
 * needs more than a statement and its rows, and staying on the public
 * interface is what will let the inspector keep working when a connection
 * stops being a bare handle -- an async or pooled one -- without a rewrite.
 *
 * The one thing that cannot be a bound parameter is a PRAGMA argument, or a
 * table name in a FROM clause: SQLite parses both as name tokens rather than
 * expressions, so a placeholder there is a syntax error. Those go through the
 * dialect's identifier quoting. Wherever a name lands in an expression
 * instead, it is bound.
 */

struct _OrmSqliteInspector
{
    OrmInspector parent_instance;
};

G_DEFINE_FINAL_TYPE (OrmSqliteInspector, orm_sqlite_inspector, ORM_TYPE_INSPECTOR)

/* ------------------------------------------------------------------ */
/* Query plumbing                                                     */
/* ------------------------------------------------------------------ */

/*
 * Resolves what every vfunc needs before it can run anything: the
 * connection to query and the dialect to quote with.  @dialect may be
 * %NULL for the callers that build no identifiers.
 */
static gboolean
orm_sqlite_inspector_context (OrmSqliteInspector  *self,
                              OrmConnection      **connection,
                              OrmDialect         **dialect,
                              GError             **error)
{
    OrmConnection *conn;
    OrmEngine     *engine;

    conn = orm_inspector_get_connection (ORM_INSPECTOR (self));

    if (conn == NULL || !orm_connection_is_open (conn))
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                     "The inspected connection is closed");
        return FALSE;
    }

    engine = orm_connection_get_engine (conn);
    if (engine == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                     "The inspected connection has no engine");
        return FALSE;
    }

    *connection = conn;

    if (dialect != NULL)
        *dialect = orm_engine_get_dialect (engine);

    return TRUE;
}

/*
 * orm_result_next() returns %FALSE both at the end of the rows and when a
 * fetch failed, so a loop that trusts it alone reports a broken query as an
 * empty schema.  Every iteration here ends with this check.
 */
static gboolean
orm_sqlite_inspector_result_ok (OrmResult  *result,
                                GError    **error)
{
    const GError *result_error;

    result_error = orm_result_get_error (result);
    if (result_error == NULL)
        return TRUE;

    g_propagate_error (error, g_error_copy (result_error));
    return FALSE;
}

/*
 * "main" is the schema a SQLite connection already resolves to, and an
 * unqualified statement is the form that works on every SQLite in the wild,
 * so it is the one used unless an ATTACHed database was actually named.
 */
static gboolean
orm_sqlite_inspector_schema_is_default (const gchar *schema)
{
    return schema == NULL || *schema == '\0' ||
           g_ascii_strcasecmp (schema, "main") == 0;
}

/*
 * Builds `"schema"."name"`, or just `"name"` for the default schema.
 */
static gchar *
orm_sqlite_inspector_qualify (OrmDialect  *dialect,
                              const gchar *schema,
                              const gchar *name)
{
    g_autofree gchar *quoted_name = NULL;
    g_autofree gchar *quoted_schema = NULL;

    quoted_name = orm_dialect_quote_identifier (dialect, name);

    if (orm_sqlite_inspector_schema_is_default (schema))
        return g_steal_pointer (&quoted_name);

    quoted_schema = orm_dialect_quote_identifier (dialect, schema);

    return g_strdup_printf ("%s.%s", quoted_schema, quoted_name);
}

/*
 * Builds `PRAGMA "schema".name("argument")`.  The schema qualifies the
 * pragma itself rather than its argument, which is where SQLite's grammar
 * puts it.
 */
static gchar *
orm_sqlite_inspector_pragma (OrmDialect  *dialect,
                             const gchar *schema,
                             const gchar *pragma,
                             const gchar *argument)
{
    g_autofree gchar *quoted_argument = NULL;
    g_autofree gchar *quoted_schema = NULL;

    quoted_argument = orm_dialect_quote_identifier (dialect, argument);

    if (orm_sqlite_inspector_schema_is_default (schema))
        return g_strdup_printf ("PRAGMA %s(%s)", pragma, quoted_argument);

    quoted_schema = orm_dialect_quote_identifier (dialect, schema);

    return g_strdup_printf ("PRAGMA %s.%s(%s)",
                            quoted_schema, pragma, quoted_argument);
}

/*
 * SQLite types values, not columns, so a catalog column that holds text in
 * one row can hold NULL in the next -- `dflt_value` and a foreign key's `to`
 * both do.  The typed getters warn on a mismatch, so both readers below
 * check the type first and treat anything unexpected as absent.
 */
static const gchar *
orm_sqlite_inspector_row_string (OrmRow      *row,
                                 const gchar *column)
{
    OrmValue *value;

    value = orm_row_get_value_by_name (row, column);
    if (value == NULL || orm_value_get_value_type (value) != ORM_VALUE_STRING)
        return NULL;

    return orm_value_get_string (value);
}

static gint64
orm_sqlite_inspector_row_integer (OrmRow      *row,
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
 * Reads a column that holds a fragment of SQL text -- a column default.
 *
 * SQLite hands the default back as the source text of the expression, so
 * `DEFAULT 0` arrives as the string "0" rather than the number, but the
 * numeric cases are handled anyway: reporting no default for a column that
 * has one would be a silent lie, and the alternative costs three lines.
 *
 * Returns: (transfer full) (nullable): The text, or %NULL when the column is
 *   SQL NULL, which is how SQLite says "no default"
 */
static gchar *
orm_sqlite_inspector_row_sql_text (OrmRow      *row,
                                   const gchar *column)
{
    OrmValue *value;

    value = orm_row_get_value_by_name (row, column);
    if (value == NULL)
        return NULL;

    switch (orm_value_get_value_type (value))
    {
    case ORM_VALUE_STRING:
        return g_strdup (orm_value_get_string (value));

    case ORM_VALUE_INTEGER:
        return g_strdup_printf ("%" G_GINT64_FORMAT,
                                orm_value_get_integer (value));

    case ORM_VALUE_FLOAT:
        {
            gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];

            g_ascii_dtostr (buffer, sizeof (buffer),
                            orm_value_get_float (value));

            return g_strdup (buffer);
        }

    default:
        return NULL;
    }
}

/*
 * Maps a declared type onto a value type by SQLite's affinity rules.
 *
 * This deliberately repeats what the SQLite driver does with
 * sqlite3_column_decltype: the two reach the declared type by different
 * routes -- the driver from a prepared statement, this from a PRAGMA row --
 * and the inspector links no sqlite3 to share the code through.  The tests
 * and their order have to stay identical to the driver's, because a column
 * described one way by a query and another way by the inspector would be
 * worse than either answer on its own.
 */
static OrmValueType
orm_sqlite_inspector_affinity_of (const gchar *decl)
{
    g_autofree gchar *upper = NULL;

    /* A column declared with no type at all has BLOB affinity. */
    if (decl == NULL || *decl == '\0')
        return ORM_VALUE_BLOB;

    upper = g_ascii_strup (decl, -1);

    if (strstr (upper, "INT") != NULL)
        return ORM_VALUE_INTEGER;

    if (strstr (upper, "CHAR") != NULL ||
        strstr (upper, "CLOB") != NULL ||
        strstr (upper, "TEXT") != NULL)
        return ORM_VALUE_STRING;

    if (strstr (upper, "BLOB") != NULL)
        return ORM_VALUE_BLOB;

    if (strstr (upper, "REAL") != NULL ||
        strstr (upper, "FLOA") != NULL ||
        strstr (upper, "DOUB") != NULL)
        return ORM_VALUE_FLOAT;

    /*
     * Past this point SQLite would say NUMERIC.  These two come after the
     * affinity rules rather than before because BOOLEAN and DATETIME are
     * numeric as far as SQLite is concerned, while orm-glib has real types
     * for both.
     */
    if (strstr (upper, "BOOL") != NULL)
        return ORM_VALUE_BOOLEAN;

    if (strstr (upper, "DATE") != NULL || strstr (upper, "TIME") != NULL)
        return ORM_VALUE_DATETIME;

    return ORM_VALUE_FLOAT;
}

/* ------------------------------------------------------------------ */
/* Schemas and relations                                              */
/* ------------------------------------------------------------------ */

static gchar **
orm_sqlite_inspector_list_schemas (OrmInspector  *inspector,
                                   GError       **error)
{
    OrmSqliteInspector   *self = ORM_SQLITE_INSPECTOR (inspector);
    OrmConnection        *connection;
    g_autoptr(OrmResult)  result = NULL;
    GPtrArray            *names;

    if (!orm_sqlite_inspector_context (self, &connection, NULL, error))
        return NULL;

    /*
     * Always at least "main", plus "temp" once a temporary table exists and
     * one row per ATTACHed database.  There is no catalog of schemas to
     * query -- attachment is a property of the connection, not the file.
     */
    result = orm_connection_query_with_params (connection, "PRAGMA database_list",
                                               NULL, error);
    if (result == NULL)
        return NULL;

    names = g_ptr_array_new_with_free_func (g_free);

    while (orm_result_next (result))
    {
        OrmRow      *row = orm_result_get_row (result);
        const gchar *name = orm_sqlite_inspector_row_string (row, "name");

        if (name != NULL)
            g_ptr_array_add (names, g_strdup (name));
    }

    if (!orm_sqlite_inspector_result_ok (result, error))
    {
        g_ptr_array_unref (names);
        return NULL;
    }

    g_ptr_array_add (names, NULL);

    return (gchar **) g_ptr_array_free (names, FALSE);
}

static GPtrArray *
orm_sqlite_inspector_list_relations (OrmInspector  *inspector,
                                     const gchar   *schema,
                                     GError       **error)
{
    OrmSqliteInspector   *self = ORM_SQLITE_INSPECTOR (inspector);
    OrmConnection        *connection;
    OrmDialect           *dialect;
    g_autofree gchar     *catalog = NULL;
    g_autofree gchar     *sql = NULL;
    g_autoptr(OrmResult)  result = NULL;
    g_autoptr(GPtrArray)  relations = NULL;

    if (!orm_sqlite_inspector_context (self, &connection, &dialect, error))
        return NULL;

    catalog = orm_sqlite_inspector_qualify (dialect, schema, "sqlite_master");

    /*
     * The `sqlite_%` names are SQLite's own bookkeeping -- sqlite_sequence,
     * sqlite_stat1, the automatic indexes -- and are not part of anybody's
     * schema.  Indexes and triggers live in the same table and are filtered
     * out here because they are not relations.
     */
    sql = g_strdup_printf ("SELECT name, type FROM %s "
                           "WHERE type IN ('table', 'view') "
                           "AND name NOT LIKE 'sqlite_%%' "
                           "ORDER BY name",
                           catalog);

    result = orm_connection_query_with_params (connection, sql, NULL, error);
    if (result == NULL)
        return NULL;

    relations = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_table_info_unref);

    while (orm_result_next (result))
    {
        OrmRow          *row = orm_result_get_row (result);
        const gchar     *name = orm_sqlite_inspector_row_string (row, "name");
        const gchar     *type = orm_sqlite_inspector_row_string (row, "type");
        OrmRelationKind  kind;

        if (name == NULL)
            continue;

        kind = (g_strcmp0 (type, "view") == 0) ? ORM_RELATION_VIEW
                                               : ORM_RELATION_TABLE;

        g_ptr_array_add (relations, orm_table_info_new (name, schema, kind));
    }

    if (!orm_sqlite_inspector_result_ok (result, error))
        return NULL;

    return g_steal_pointer (&relations);
}

/* ------------------------------------------------------------------ */
/* Columns                                                            */
/* ------------------------------------------------------------------ */

/*
 * Whether @table was declared with AUTOINCREMENT.
 *
 * PRAGMA table_info does not report the keyword, so the answer comes from
 * the CREATE TABLE text SQLite keeps in `sqlite_master`.
 *
 * The obvious alternative -- looking for the table in `sqlite_sequence` --
 * is wrong for an inspector, and quietly so: SQLite does not add a row
 * there until the first INSERT, so a freshly created AUTOINCREMENT table
 * reports FALSE and starts reporting TRUE once someone uses it. That
 * describes the data, and this is asking about the schema.
 *
 * Scanning the DDL text can only be fooled by the word appearing
 * somewhere that is not the keyword -- inside a column name or a string
 * default. That costs a false positive on a column flagged as
 * server-generated when it is not; the sqlite_sequence approach costs a
 * false negative on every table that has not been written to yet, which
 * is the common case for a browser.
 */
static gboolean
orm_sqlite_inspector_uses_autoincrement (OrmConnection  *connection,
                                         OrmDialect     *dialect,
                                         const gchar    *table,
                                         const gchar    *schema,
                                         gboolean       *uses,
                                         GError        **error)
{
    g_autofree gchar     *catalog = NULL;
    g_autofree gchar     *sql = NULL;
    g_autofree gchar     *upper = NULL;
    g_autoptr(OrmResult)  result = NULL;
    GList                *params = NULL;
    const gchar          *ddl;
    OrmRow               *row;
    OrmValue             *value;

    *uses = FALSE;

    catalog = orm_sqlite_inspector_qualify (dialect, schema, "sqlite_master");
    sql = g_strdup_printf ("SELECT sql FROM %s WHERE type = 'table' AND name = ?",
                           catalog);

    /* A comparison is an expression, so the table name binds here. */
    params = g_list_append (NULL, orm_value_new_string (table));
    result = orm_connection_query_with_params (connection, sql, params, error);
    g_list_free_full (params, (GDestroyNotify) orm_value_free);

    if (result == NULL)
        return FALSE;

    if (!orm_result_next (result))
        return orm_sqlite_inspector_result_ok (result, error);

    row = orm_result_get_row (result);
    value = orm_row_get_value (row, 0);

    if (value == NULL || orm_value_is_null (value))
        return TRUE;

    ddl = orm_value_get_string (value);
    if (ddl == NULL)
        return TRUE;

    upper = g_ascii_strup (ddl, -1);
    *uses = (strstr (upper, "AUTOINCREMENT") != NULL);

    return TRUE;
}

static GPtrArray *
orm_sqlite_inspector_get_columns (OrmInspector  *inspector,
                                  const gchar   *table,
                                  const gchar   *schema,
                                  GError       **error)
{
    OrmSqliteInspector   *self = ORM_SQLITE_INSPECTOR (inspector);
    OrmConnection        *connection;
    OrmDialect           *dialect;
    g_autofree gchar     *sql = NULL;
    g_autoptr(OrmResult)  result = NULL;
    g_autoptr(GPtrArray)  columns = NULL;
    gboolean              autoincrement_table;

    if (!orm_sqlite_inspector_context (self, &connection, &dialect, error))
        return NULL;

    if (!orm_sqlite_inspector_uses_autoincrement (connection, dialect, table,
                                                  schema, &autoincrement_table,
                                                  error))
        return NULL;

    sql = orm_sqlite_inspector_pragma (dialect, schema, "table_info", table);

    result = orm_connection_query_with_params (connection, sql, NULL, error);
    if (result == NULL)
        return NULL;

    columns = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_column_info_unref);

    while (orm_result_next (result))
    {
        OrmRow           *row = orm_result_get_row (result);
        const gchar      *name = orm_sqlite_inspector_row_string (row, "name");
        const gchar      *decl = orm_sqlite_inspector_row_string (row, "type");
        g_autofree gchar *default_value = NULL;
        gint64            primary_key;
        gboolean          autoincrement;

        if (name == NULL)
            continue;

        default_value = orm_sqlite_inspector_row_sql_text (row, "dflt_value");
        primary_key = orm_sqlite_inspector_row_integer (row, "pk", 0);

        /*
         * AUTOINCREMENT is legal only on a lone INTEGER PRIMARY KEY, and a
         * table earns its sqlite_sequence row only by having one, so the
         * two facts together name the column exactly.  The declared type
         * must be INTEGER and nothing else: "INT" or "BIGINT" is a
         * different column as far as SQLite's rowid alias is concerned.
         */
        autoincrement = autoincrement_table &&
                        primary_key == 1 &&
                        decl != NULL &&
                        g_ascii_strcasecmp (decl, "INTEGER") == 0;

        g_ptr_array_add (columns,
                         orm_column_info_new (name,
                                              (decl != NULL && *decl != '\0') ? decl : NULL,
                                              orm_sqlite_inspector_affinity_of (decl),
                                              orm_sqlite_inspector_row_integer (row, "notnull", 0) == 0,
                                              default_value,
                                              primary_key > 0,
                                              autoincrement,
                                              (gint) orm_sqlite_inspector_row_integer (row, "cid", 0)));
    }

    if (!orm_sqlite_inspector_result_ok (result, error))
        return NULL;

    return g_steal_pointer (&columns);
}

/* ------------------------------------------------------------------ */
/* Indexes                                                            */
/* ------------------------------------------------------------------ */

/*
 * One row of PRAGMA index_list, held while the result that produced it is
 * closed.  Each index needs a second query to name its columns, and running
 * that with the listing still open would assume a connection that can carry
 * two statements at once -- true of a bare sqlite3 handle, not a promise the
 * connection API makes.
 */
typedef struct
{
    gchar    *name;
    gboolean  unique;
} OrmSqliteIndexEntry;

static void
orm_sqlite_index_entry_free (gpointer data)
{
    OrmSqliteIndexEntry *entry = (OrmSqliteIndexEntry *) data;

    g_free (entry->name);
    g_free (entry);
}

static GPtrArray *
orm_sqlite_inspector_get_indexes (OrmInspector  *inspector,
                                  const gchar   *table,
                                  const gchar   *schema,
                                  GError       **error)
{
    OrmSqliteInspector   *self = ORM_SQLITE_INSPECTOR (inspector);
    OrmConnection        *connection;
    OrmDialect           *dialect;
    g_autofree gchar     *list_sql = NULL;
    g_autoptr(OrmResult)  listing = NULL;
    g_autoptr(GPtrArray)  entries = NULL;
    g_autoptr(GPtrArray)  indexes = NULL;
    guint                 i;

    if (!orm_sqlite_inspector_context (self, &connection, &dialect, error))
        return NULL;

    list_sql = orm_sqlite_inspector_pragma (dialect, schema, "index_list", table);

    listing = orm_connection_query_with_params (connection, list_sql, NULL, error);
    if (listing == NULL)
        return NULL;

    entries = g_ptr_array_new_with_free_func (orm_sqlite_index_entry_free);

    while (orm_result_next (listing))
    {
        OrmRow              *row = orm_result_get_row (listing);
        const gchar         *name = orm_sqlite_inspector_row_string (row, "name");
        OrmSqliteIndexEntry *entry;

        if (name == NULL)
            continue;

        /*
         * The `origin` column separates an index the schema asked for ('c')
         * from one SQLite created to enforce UNIQUE ('u') or a PRIMARY KEY
         * ('pk').  All three are indexes that exist and will be used, so all
         * three are reported; only the unique flag distinguishes them here.
         */
        entry = g_new0 (OrmSqliteIndexEntry, 1);
        entry->name = g_strdup (name);
        entry->unique = orm_sqlite_inspector_row_integer (row, "unique", 0) != 0;

        g_ptr_array_add (entries, entry);
    }

    if (!orm_sqlite_inspector_result_ok (listing, error))
        return NULL;

    g_clear_object (&listing);

    indexes = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_index_info_unref);

    for (i = 0; i < entries->len; i++)
    {
        OrmSqliteIndexEntry  *entry = g_ptr_array_index (entries, i);
        g_autofree gchar     *info_sql = NULL;
        g_autoptr(OrmResult)  info = NULL;
        g_autoptr(GPtrArray)  members = NULL;

        info_sql = orm_sqlite_inspector_pragma (dialect, schema, "index_info",
                                                entry->name);

        info = orm_connection_query_with_params (connection, info_sql, NULL, error);
        if (info == NULL)
            return NULL;

        members = g_ptr_array_new_with_free_func (g_free);

        /* index_info emits its rows in seqno order, which is key order. */
        while (orm_result_next (info))
        {
            OrmRow      *row = orm_result_get_row (info);
            const gchar *column = orm_sqlite_inspector_row_string (row, "name");

            /*
             * An index over an expression, or over the rowid, has no column
             * to name and SQLite reports NULL.  There is nothing truthful to
             * put in a list of column names, so the entry is left out.
             */
            if (column == NULL)
                continue;

            g_ptr_array_add (members, g_strdup (column));
        }

        if (!orm_sqlite_inspector_result_ok (info, error))
            return NULL;

        g_ptr_array_add (members, NULL);

        g_ptr_array_add (indexes,
                         orm_index_info_new (entry->name, entry->unique,
                                             (const gchar * const *) members->pdata));
    }

    return g_steal_pointer (&indexes);
}

/* ------------------------------------------------------------------ */
/* Foreign keys                                                       */
/* ------------------------------------------------------------------ */

/*
 * One row of PRAGMA foreign_key_list.
 *
 * The pragma reports one row per referring column, not per constraint: a
 * two-column foreign key arrives as two rows sharing an `id` and numbered by
 * `seq`.  Rebuilding the constraint means grouping on `id` and ordering
 * within the group by `seq`, so the rows are collected first and assembled
 * afterwards.
 */
typedef struct
{
    gint64               id;
    gint64               seq;
    gchar               *ref_table;
    gchar               *from_column;
    gchar               *to_column;   /* NULL: references the target's PK */
    OrmForeignKeyAction  on_update;
    OrmForeignKeyAction  on_delete;
} OrmSqliteForeignKeyEntry;

static void
orm_sqlite_foreign_key_entry_free (gpointer data)
{
    OrmSqliteForeignKeyEntry *entry = (OrmSqliteForeignKeyEntry *) data;

    g_free (entry->ref_table);
    g_free (entry->from_column);
    g_free (entry->to_column);
    g_free (entry);
}

/*
 * Orders the rows into constraints, and each constraint into key order.
 */
static gint
orm_sqlite_foreign_key_entry_compare (gconstpointer a,
                                      gconstpointer b)
{
    const OrmSqliteForeignKeyEntry *first = *(OrmSqliteForeignKeyEntry * const *) a;
    const OrmSqliteForeignKeyEntry *second = *(OrmSqliteForeignKeyEntry * const *) b;

    if (first->id != second->id)
        return (first->id < second->id) ? -1 : 1;

    if (first->seq != second->seq)
        return (first->seq < second->seq) ? -1 : 1;

    return 0;
}

/*
 * SQLite spells the referential actions exactly as the SQL standard does,
 * and reports "NO ACTION" for a clause nobody wrote.
 */
static OrmForeignKeyAction
orm_sqlite_inspector_action_of (const gchar *action)
{
    if (action == NULL)
        return ORM_FK_NO_ACTION;

    if (g_ascii_strcasecmp (action, "CASCADE") == 0)
        return ORM_FK_CASCADE;

    if (g_ascii_strcasecmp (action, "RESTRICT") == 0)
        return ORM_FK_RESTRICT;

    if (g_ascii_strcasecmp (action, "SET NULL") == 0)
        return ORM_FK_SET_NULL;

    if (g_ascii_strcasecmp (action, "SET DEFAULT") == 0)
        return ORM_FK_SET_DEFAULT;

    return ORM_FK_NO_ACTION;
}

static GPtrArray *
orm_sqlite_inspector_get_foreign_keys (OrmInspector  *inspector,
                                       const gchar   *table,
                                       const gchar   *schema,
                                       GError       **error)
{
    OrmSqliteInspector   *self = ORM_SQLITE_INSPECTOR (inspector);
    OrmConnection        *connection;
    OrmDialect           *dialect;
    g_autofree gchar     *sql = NULL;
    g_autoptr(OrmResult)  result = NULL;
    g_autoptr(GPtrArray)  entries = NULL;
    g_autoptr(GPtrArray)  keys = NULL;
    guint                 i;

    if (!orm_sqlite_inspector_context (self, &connection, &dialect, error))
        return NULL;

    sql = orm_sqlite_inspector_pragma (dialect, schema, "foreign_key_list", table);

    result = orm_connection_query_with_params (connection, sql, NULL, error);
    if (result == NULL)
        return NULL;

    entries = g_ptr_array_new_with_free_func (orm_sqlite_foreign_key_entry_free);

    while (orm_result_next (result))
    {
        OrmRow                   *row = orm_result_get_row (result);
        const gchar              *ref_table = orm_sqlite_inspector_row_string (row, "table");
        const gchar              *from_column = orm_sqlite_inspector_row_string (row, "from");
        OrmSqliteForeignKeyEntry *entry;

        if (ref_table == NULL || from_column == NULL)
            continue;

        entry = g_new0 (OrmSqliteForeignKeyEntry, 1);
        entry->id = orm_sqlite_inspector_row_integer (row, "id", 0);
        entry->seq = orm_sqlite_inspector_row_integer (row, "seq", 0);
        entry->ref_table = g_strdup (ref_table);
        entry->from_column = g_strdup (from_column);
        entry->to_column = g_strdup (orm_sqlite_inspector_row_string (row, "to"));
        entry->on_update = orm_sqlite_inspector_action_of (
            orm_sqlite_inspector_row_string (row, "on_update"));
        entry->on_delete = orm_sqlite_inspector_action_of (
            orm_sqlite_inspector_row_string (row, "on_delete"));

        g_ptr_array_add (entries, entry);
    }

    if (!orm_sqlite_inspector_result_ok (result, error))
        return NULL;

    g_clear_object (&result);

    g_ptr_array_sort (entries, orm_sqlite_foreign_key_entry_compare);

    keys = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_foreign_key_info_unref);

    i = 0;
    while (i < entries->len)
    {
        OrmSqliteForeignKeyEntry *first = g_ptr_array_index (entries, i);
        g_autoptr(GPtrArray)      columns = NULL;
        g_autoptr(GPtrArray)      ref_columns = NULL;
        g_auto(GStrv)             referenced_key = NULL;
        gboolean                  implicit;
        guint                     end;
        guint                     j;

        for (end = i;
             end < entries->len &&
             ((OrmSqliteForeignKeyEntry *) g_ptr_array_index (entries, end))->id == first->id;
             end++)
            ;

        columns = g_ptr_array_new_with_free_func (g_free);
        ref_columns = g_ptr_array_new_with_free_func (g_free);

        /*
         * A foreign key written without a column list -- REFERENCES t
         * rather than REFERENCES t (a, b) -- references the target's primary
         * key, and SQLite says so by leaving `to` NULL rather than by naming
         * the columns.  Resolving it here is what keeps a caller from having
         * to know that rule.  It is a property of the constraint, not of the
         * row, so the first row of the group decides for all of them.
         */
        implicit = (first->to_column == NULL);

        for (j = i; j < end; j++)
        {
            OrmSqliteForeignKeyEntry *entry = g_ptr_array_index (entries, j);

            g_ptr_array_add (columns, g_strdup (entry->from_column));

            if (!implicit && entry->to_column != NULL)
                g_ptr_array_add (ref_columns, g_strdup (entry->to_column));
        }

        if (implicit)
        {
            guint k;

            referenced_key = orm_inspector_get_primary_key (inspector,
                                                            first->ref_table,
                                                            schema, error);
            if (referenced_key == NULL)
                return NULL;

            for (k = 0; referenced_key[k] != NULL; k++)
                g_ptr_array_add (ref_columns, g_strdup (referenced_key[k]));
        }

        g_ptr_array_add (columns, NULL);
        g_ptr_array_add (ref_columns, NULL);

        /*
         * SQLite keeps no name for a foreign key: a CONSTRAINT name in the
         * CREATE statement is parsed and discarded, and the pragma offers
         * only the positional id.
         */
        g_ptr_array_add (keys,
                         orm_foreign_key_info_new (NULL,
                                                   (const gchar * const *) columns->pdata,
                                                   first->ref_table,
                                                   schema,
                                                   (const gchar * const *) ref_columns->pdata,
                                                   first->on_delete,
                                                   first->on_update));

        i = end;
    }

    return g_steal_pointer (&keys);
}

/* ------------------------------------------------------------------ */
/* Primary key and row count                                          */
/* ------------------------------------------------------------------ */

/*
 * One primary-key column, held with the position PRAGMA table_info gave it.
 *
 * `pk` is the column's place in the key, counting from one, and it is not
 * the same as `cid`, the column's place in the table: PRIMARY KEY (b, a) on
 * a table declared (a, b) gives a cid 0 / pk 2 and b cid 1 / pk 1.  Key
 * order is the one that matters to a caller building a WHERE clause, so the
 * rows are sorted by `pk` rather than read in the order they arrive.
 */
typedef struct
{
    gint64  position;
    gchar  *name;
} OrmSqlitePrimaryKeyEntry;

static void
orm_sqlite_primary_key_entry_free (gpointer data)
{
    OrmSqlitePrimaryKeyEntry *entry = (OrmSqlitePrimaryKeyEntry *) data;

    g_free (entry->name);
    g_free (entry);
}

static gint
orm_sqlite_primary_key_entry_compare (gconstpointer a,
                                      gconstpointer b)
{
    const OrmSqlitePrimaryKeyEntry *first = *(OrmSqlitePrimaryKeyEntry * const *) a;
    const OrmSqlitePrimaryKeyEntry *second = *(OrmSqlitePrimaryKeyEntry * const *) b;

    if (first->position != second->position)
        return (first->position < second->position) ? -1 : 1;

    return 0;
}

static gchar **
orm_sqlite_inspector_get_primary_key (OrmInspector  *inspector,
                                      const gchar   *table,
                                      const gchar   *schema,
                                      GError       **error)
{
    OrmSqliteInspector   *self = ORM_SQLITE_INSPECTOR (inspector);
    OrmConnection        *connection;
    OrmDialect           *dialect;
    g_autofree gchar     *sql = NULL;
    g_autoptr(OrmResult)  result = NULL;
    g_autoptr(GPtrArray)  entries = NULL;
    GPtrArray            *names;
    guint                 i;

    if (!orm_sqlite_inspector_context (self, &connection, &dialect, error))
        return NULL;

    sql = orm_sqlite_inspector_pragma (dialect, schema, "table_info", table);

    result = orm_connection_query_with_params (connection, sql, NULL, error);
    if (result == NULL)
        return NULL;

    entries = g_ptr_array_new_with_free_func (orm_sqlite_primary_key_entry_free);

    while (orm_result_next (result))
    {
        OrmRow                   *row = orm_result_get_row (result);
        const gchar              *name = orm_sqlite_inspector_row_string (row, "name");
        gint64                    position = orm_sqlite_inspector_row_integer (row, "pk", 0);
        OrmSqlitePrimaryKeyEntry *entry;

        /* pk is zero for every column of a table that has no primary key. */
        if (name == NULL || position <= 0)
            continue;

        entry = g_new0 (OrmSqlitePrimaryKeyEntry, 1);
        entry->position = position;
        entry->name = g_strdup (name);

        g_ptr_array_add (entries, entry);
    }

    if (!orm_sqlite_inspector_result_ok (result, error))
        return NULL;

    g_ptr_array_sort (entries, orm_sqlite_primary_key_entry_compare);

    names = g_ptr_array_new_with_free_func (g_free);

    for (i = 0; i < entries->len; i++)
    {
        OrmSqlitePrimaryKeyEntry *entry = g_ptr_array_index (entries, i);

        g_ptr_array_add (names, g_strdup (entry->name));
    }

    g_ptr_array_add (names, NULL);

    return (gchar **) g_ptr_array_free (names, FALSE);
}

static gint64
orm_sqlite_inspector_estimate_row_count (OrmInspector  *inspector,
                                         const gchar   *table,
                                         const gchar   *schema,
                                         gboolean      *is_estimate,
                                         GError       **error)
{
    OrmSqliteInspector   *self = ORM_SQLITE_INSPECTOR (inspector);
    OrmConnection        *connection;
    OrmDialect           *dialect;
    g_autofree gchar     *qualified = NULL;
    g_autofree gchar     *sql = NULL;
    g_autoptr(OrmResult)  result = NULL;
    OrmRow               *row;
    OrmValue             *value;

    /*
     * SQLite keeps no table statistics unless ANALYZE has been run, and even
     * then sqlite_stat1 holds a string that need not still be true.  There is
     * nothing to estimate from, so the count is the real one -- which is also
     * cheap here in a way it is not on a server, since SQLite walks a local
     * B-tree rather than a heap over the network.
     */
    *is_estimate = FALSE;

    if (!orm_sqlite_inspector_context (self, &connection, &dialect, error))
        return -1;

    qualified = orm_sqlite_inspector_qualify (dialect, schema, table);
    sql = g_strdup_printf ("SELECT COUNT(*) FROM %s", qualified);

    result = orm_connection_query_with_params (connection, sql, NULL, error);
    if (result == NULL)
        return -1;

    if (!orm_result_next (result))
    {
        if (!orm_sqlite_inspector_result_ok (result, error))
            return -1;

        g_set_error (error, ORM_ERROR, ORM_ERROR_QUERY_FAILED,
                     "Counting the rows of \"%s\" returned nothing", table);
        return -1;
    }

    row = orm_result_get_row (result);
    value = (row != NULL) ? orm_row_get_value (row, 0) : NULL;

    if (value == NULL || orm_value_get_value_type (value) != ORM_VALUE_INTEGER)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_TYPE_MISMATCH,
                     "Counting the rows of \"%s\" produced a non-integer", table);
        return -1;
    }

    return orm_value_get_integer (value);
}

/* ------------------------------------------------------------------ */
/* Type                                                               */
/* ------------------------------------------------------------------ */

static void
orm_sqlite_inspector_class_init (OrmSqliteInspectorClass *klass)
{
    OrmInspectorClass *inspector_class = ORM_INSPECTOR_CLASS (klass);

    inspector_class->list_schemas = orm_sqlite_inspector_list_schemas;
    inspector_class->list_relations = orm_sqlite_inspector_list_relations;
    inspector_class->get_columns = orm_sqlite_inspector_get_columns;
    inspector_class->get_indexes = orm_sqlite_inspector_get_indexes;
    inspector_class->get_foreign_keys = orm_sqlite_inspector_get_foreign_keys;
    inspector_class->get_primary_key = orm_sqlite_inspector_get_primary_key;
    inspector_class->estimate_row_count = orm_sqlite_inspector_estimate_row_count;
}

static void
orm_sqlite_inspector_init (OrmSqliteInspector *self)
{
}

/**
 * orm_sqlite_inspector_new:
 * @connection: An open #OrmConnection to a SQLite database
 *
 * Creates an inspector that reads a SQLite schema from `sqlite_master` and
 * the introspection PRAGMAs.
 *
 * Returns: (transfer full): A new #OrmSqliteInspector
 */
OrmSqliteInspector *
orm_sqlite_inspector_new (OrmConnection *connection)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (connection), NULL);

    return g_object_new (ORM_TYPE_SQLITE_INSPECTOR,
                         "connection", connection,
                         NULL);
}
