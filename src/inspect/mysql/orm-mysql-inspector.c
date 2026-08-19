/* orm-mysql-inspector.c
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

#include "orm-mysql-inspector.h"
#include "../../core/orm-error.h"
#include "../../dialect/orm-dialect.h"
#include "../../engine/orm-engine.h"
#include "../../engine/orm-result.h"
#include "../../engine/orm-row.h"

#include <string.h>

/*
 * The MySQL/MariaDB inspector.
 *
 * Everything here goes through `information_schema`, which is ordinary
 * SQL, so this file runs its queries through the public connection API
 * rather than reaching for the client library.  That keeps the inspector
 * independent of how the driver happens to execute a statement, and it is
 * why the same code serves MySQL and MariaDB.
 *
 * A schema and a database are the same thing on this backend, so a %NULL
 * @schema means "the one this connection is using" and is written
 * `COALESCE(?, DATABASE())`.  Every schema and table name is bound as a
 * parameter: these names arrive from callers who are browsing a database,
 * and interpolating them would be the injection this library exists to
 * avoid.  Identifiers cannot be bound, so the one query that needs one --
 * the exact COUNT(*) fallback -- quotes it through the dialect instead.
 */

struct _OrmMysqlInspector
{
    OrmInspector parent_instance;
};

G_DEFINE_FINAL_TYPE (OrmMysqlInspector, orm_mysql_inspector, ORM_TYPE_INSPECTOR)

/* ------------------------------------------------------------------ */
/* Reading rows                                                       */
/* ------------------------------------------------------------------ */

/*
 * Reads one column of @row as text, or %NULL when it is NULL or absent.
 *
 * The copy is not laziness.  MySQL gives TEXT and BLOB the same type code
 * on the wire, and the data dictionary stores much of what
 * information_schema reports as longtext -- COLUMN_TYPE, COLUMN_DEFAULT
 * and DATA_TYPE among them on MySQL 8 -- so those columns arrive as
 * unterminated bytes rather than as strings.  Asking for text here, once,
 * is what keeps that difference out of every caller.
 *
 * Returns: (transfer full) (nullable): The value as text
 */
static gchar *
orm_mysql_inspector_row_text (OrmRow      *row,
                              const gchar *column)
{
    OrmValue *value;

    if (row == NULL)
        return NULL;

    value = orm_row_get_value_by_name (row, column);
    if (value == NULL || orm_value_is_null (value))
        return NULL;

    switch (orm_value_get_value_type (value))
    {
    case ORM_VALUE_STRING:
        return g_strdup (orm_value_get_string (value));

    case ORM_VALUE_BLOB:
        {
            GBytes       *bytes = orm_value_get_blob (value);
            gconstpointer data;
            gsize         size = 0;

            if (bytes == NULL)
                return NULL;

            data = g_bytes_get_data (bytes, &size);

            return g_strndup ((const gchar *) data, size);
        }

    default:
        /* A numeric column asked for as text: render it, do not refuse. */
        return orm_value_to_string (value);
    }
}

/*
 * Reads one column of @row as a whole number, falling back to @fallback
 * when it is NULL or absent.
 *
 * information_schema's numeric columns have changed type across server
 * versions -- bigint in 5.7, plain int in 8.0, and text for some of them
 * on MariaDB -- so this accepts whatever arrives rather than assuming.
 */
static gint64
orm_mysql_inspector_row_int (OrmRow      *row,
                             const gchar *column,
                             gint64       fallback)
{
    OrmValue *value;

    if (row == NULL)
        return fallback;

    value = orm_row_get_value_by_name (row, column);
    if (value == NULL || orm_value_is_null (value))
        return fallback;

    switch (orm_value_get_value_type (value))
    {
    case ORM_VALUE_INTEGER:
        return orm_value_get_integer (value);

    case ORM_VALUE_FLOAT:
        return (gint64) orm_value_get_float (value);

    default:
        {
            g_autofree gchar *text = orm_mysql_inspector_row_text (row, column);

            if (text == NULL)
                return fallback;

            return g_ascii_strtoll (text, NULL, 10);
        }
    }
}

/*
 * Whether a column is SQL NULL, which for TABLE_ROWS is the difference
 * between "no rows" and "no statistics".
 */
static gboolean
orm_mysql_inspector_row_is_null (OrmRow      *row,
                                 const gchar *column)
{
    OrmValue *value;

    if (row == NULL)
        return TRUE;

    value = orm_row_get_value_by_name (row, column);

    return value == NULL || orm_value_is_null (value);
}

/* ------------------------------------------------------------------ */
/* Running queries                                                    */
/* ------------------------------------------------------------------ */

static OrmConnection *
orm_mysql_inspector_connection (OrmInspector  *self,
                                GError       **error)
{
    OrmConnection *connection;

    connection = orm_inspector_get_connection (self);
    if (connection == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION_CLOSED,
                     "The inspected connection is gone");
        return NULL;
    }

    return connection;
}

/*
 * Runs @sql with @schema, and @table when it is given, bound as
 * parameters in that order.
 *
 * A %NULL @schema is bound as SQL NULL rather than omitted, because the
 * placeholder is inside `COALESCE(?, DATABASE())`: passing no parameter
 * would change the statement, passing NULL is what selects the current
 * database.
 *
 * Returns: (transfer full) (nullable): The result, or %NULL on error
 */
static OrmResult *
orm_mysql_inspector_query (OrmInspector  *self,
                           const gchar   *sql,
                           const gchar   *schema,
                           const gchar   *table,
                           GError       **error)
{
    OrmConnection *connection;
    OrmResult     *result;
    GList         *params = NULL;

    connection = orm_mysql_inspector_connection (self, error);
    if (connection == NULL)
        return NULL;

    params = g_list_append (params,
                            schema != NULL
                            ? orm_value_new_string (schema)
                            : orm_value_new_null ());

    if (table != NULL)
        params = g_list_append (params, orm_value_new_string (table));

    result = orm_connection_query_with_params (connection, sql, params, error);

    g_list_free_full (params, (GDestroyNotify) orm_value_free);

    return result;
}

/*
 * Turns a failed fetch into an error.
 *
 * orm_result_next() answers %FALSE for both the end of the rows and a
 * broken read, so without this check a connection that died half way
 * through would be reported as a schema that happens to be short of a
 * few tables.  Call it after every iteration loop, before the array is
 * handed back.
 */
static gboolean
orm_mysql_inspector_check_result (OrmResult  *result,
                                  GError    **error)
{
    const GError *iter_error;

    iter_error = orm_result_get_error (result);
    if (iter_error == NULL)
        return TRUE;

    g_set_error_literal (error, iter_error->domain, iter_error->code,
                         iter_error->message);

    return FALSE;
}

/* ------------------------------------------------------------------ */
/* Mapping what the server says                                       */
/* ------------------------------------------------------------------ */

/*
 * Maps an information_schema DATA_TYPE onto a value type.
 *
 * DATA_TYPE is the bare type word -- "int" for an `int unsigned` column,
 * "varchar" whatever the length -- which is exactly what this decision
 * wants; the full spelling is reported separately as the type name.
 */
static OrmValueType
orm_mysql_value_type_of_data_type (const gchar *data_type)
{
    static const struct
    {
        const gchar  *data_type;
        OrmValueType  value_type;
    } known[] = {
        { "tinyint",    ORM_VALUE_INTEGER },
        { "smallint",   ORM_VALUE_INTEGER },
        { "mediumint",  ORM_VALUE_INTEGER },
        { "int",        ORM_VALUE_INTEGER },
        { "integer",    ORM_VALUE_INTEGER },
        { "bigint",     ORM_VALUE_INTEGER },
        { "float",      ORM_VALUE_FLOAT },
        { "double",     ORM_VALUE_FLOAT },
        { "decimal",    ORM_VALUE_FLOAT },
        { "numeric",    ORM_VALUE_FLOAT },
        { "blob",       ORM_VALUE_BLOB },
        { "tinyblob",   ORM_VALUE_BLOB },
        { "mediumblob", ORM_VALUE_BLOB },
        { "longblob",   ORM_VALUE_BLOB },
        { "binary",     ORM_VALUE_BLOB },
        { "varbinary",  ORM_VALUE_BLOB },
        { "date",       ORM_VALUE_DATETIME },
        { "datetime",   ORM_VALUE_DATETIME },
        { "timestamp",  ORM_VALUE_DATETIME },
        { NULL,         ORM_VALUE_STRING }
    };
    gsize i;

    if (data_type == NULL)
        return ORM_VALUE_STRING;

    for (i = 0; known[i].data_type != NULL; i++)
    {
        if (g_ascii_strcasecmp (data_type, known[i].data_type) == 0)
            return known[i].value_type;
    }

    /*
     * CHAR, VARCHAR, TEXT, ENUM, SET, JSON, TIME, YEAR and anything a
     * later server adds decode as text, which matches how the driver
     * reads them off the wire.
     */
    return ORM_VALUE_STRING;
}

/*
 * TABLE_TYPE names three things and this cares about two of them.
 * SYSTEM VIEW is what information_schema's own relations report; it is a
 * view for every purpose a caller has.
 */
static OrmRelationKind
orm_mysql_relation_kind_of (const gchar *table_type)
{
    if (table_type == NULL)
        return ORM_RELATION_TABLE;

    if (g_ascii_strcasecmp (table_type, "VIEW") == 0 ||
        g_ascii_strcasecmp (table_type, "SYSTEM VIEW") == 0)
        return ORM_RELATION_VIEW;

    return ORM_RELATION_TABLE;
}

/*
 * Maps a REFERENTIAL_CONSTRAINTS rule word onto an action.
 *
 * InnoDB treats RESTRICT and NO ACTION identically and rejects SET
 * DEFAULT outright, but both are spelled out here: this reports what the
 * constraint declares, and MariaDB's other engines do not share InnoDB's
 * limits.
 */
static OrmForeignKeyAction
orm_mysql_foreign_key_action_of (const gchar *rule)
{
    if (rule == NULL)
        return ORM_FK_NO_ACTION;

    if (g_ascii_strcasecmp (rule, "CASCADE") == 0)
        return ORM_FK_CASCADE;

    if (g_ascii_strcasecmp (rule, "SET NULL") == 0)
        return ORM_FK_SET_NULL;

    if (g_ascii_strcasecmp (rule, "SET DEFAULT") == 0)
        return ORM_FK_SET_DEFAULT;

    if (g_ascii_strcasecmp (rule, "RESTRICT") == 0)
        return ORM_FK_RESTRICT;

    return ORM_FK_NO_ACTION;
}

/*
 * Whether EXTRA says the server generates this column's value.
 *
 * EXTRA is a list of words rather than a single flag -- "DEFAULT_GENERATED
 * on update CURRENT_TIMESTAMP" is a normal value for it -- so this looks
 * for the word instead of comparing the whole field, case-folded because
 * MySQL and MariaDB have not always agreed on the case.
 */
static gboolean
orm_mysql_extra_is_autoincrement (const gchar *extra)
{
    g_autofree gchar *folded = NULL;

    if (extra == NULL)
        return FALSE;

    folded = g_ascii_strdown (extra, -1);

    return strstr (folded, "auto_increment") != NULL;
}

/* ------------------------------------------------------------------ */
/* Schemas and relations                                              */
/* ------------------------------------------------------------------ */

static gchar **
orm_mysql_inspector_list_schemas (OrmInspector  *self,
                                  GError       **error)
{
    OrmConnection        *connection;
    g_autoptr(OrmResult)  result = NULL;
    GPtrArray            *names;

    connection = orm_mysql_inspector_connection (self, error);
    if (connection == NULL)
        return NULL;

    /*
     * The four schemas left out are the server's own: they are always
     * there, nobody stores anything in them, and listing them buries the
     * databases the caller actually has.
     */
    result = orm_connection_query_with_params (connection,
                                               "SELECT SCHEMA_NAME "
                                               "FROM information_schema.SCHEMATA "
                                               "WHERE SCHEMA_NAME NOT IN "
                                               "('information_schema', 'performance_schema', "
                                               "'mysql', 'sys') "
                                               "ORDER BY SCHEMA_NAME",
                                               NULL, error);
    if (result == NULL)
        return NULL;

    names = g_ptr_array_new_with_free_func (g_free);

    while (orm_result_next (result))
    {
        gchar *name = orm_mysql_inspector_row_text (orm_result_get_row (result),
                                                    "SCHEMA_NAME");

        if (name != NULL)
            g_ptr_array_add (names, name);
    }

    if (!orm_mysql_inspector_check_result (result, error))
    {
        g_ptr_array_unref (names);
        return NULL;
    }

    g_ptr_array_add (names, NULL);

    return (gchar **) g_ptr_array_free (names, FALSE);
}

static GPtrArray *
orm_mysql_inspector_list_relations (OrmInspector  *self,
                                    const gchar   *schema,
                                    GError       **error)
{
    g_autoptr(OrmResult)  result = NULL;
    GPtrArray            *relations;

    result = orm_mysql_inspector_query (self,
                                        "SELECT TABLE_NAME, TABLE_TYPE "
                                        "FROM information_schema.TABLES "
                                        "WHERE TABLE_SCHEMA = COALESCE(?, DATABASE()) "
                                        "ORDER BY TABLE_NAME",
                                        schema, NULL, error);
    if (result == NULL)
        return NULL;

    relations = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_table_info_unref);

    while (orm_result_next (result))
    {
        OrmRow           *row = orm_result_get_row (result);
        g_autofree gchar *name = orm_mysql_inspector_row_text (row, "TABLE_NAME");
        g_autofree gchar *table_type = orm_mysql_inspector_row_text (row, "TABLE_TYPE");

        if (name == NULL)
            continue;

        g_ptr_array_add (relations,
                         orm_table_info_new (name, schema,
                                             orm_mysql_relation_kind_of (table_type)));
    }

    if (!orm_mysql_inspector_check_result (result, error))
    {
        g_ptr_array_unref (relations);
        return NULL;
    }

    return relations;
}

/* ------------------------------------------------------------------ */
/* Columns                                                            */
/* ------------------------------------------------------------------ */

static GPtrArray *
orm_mysql_inspector_get_columns (OrmInspector  *self,
                                 const gchar   *table,
                                 const gchar   *schema,
                                 GError       **error)
{
    g_autoptr(OrmResult)  result = NULL;
    GPtrArray            *columns;

    result = orm_mysql_inspector_query (self,
                                        "SELECT COLUMN_NAME, ORDINAL_POSITION, DATA_TYPE, "
                                        "COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "
                                        "COLUMN_KEY, EXTRA "
                                        "FROM information_schema.COLUMNS "
                                        "WHERE TABLE_SCHEMA = COALESCE(?, DATABASE()) "
                                        "AND TABLE_NAME = ? "
                                        "ORDER BY ORDINAL_POSITION",
                                        schema, table, error);
    if (result == NULL)
        return NULL;

    columns = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_column_info_unref);

    while (orm_result_next (result))
    {
        OrmRow           *row = orm_result_get_row (result);
        g_autofree gchar *name = orm_mysql_inspector_row_text (row, "COLUMN_NAME");
        g_autofree gchar *data_type = orm_mysql_inspector_row_text (row, "DATA_TYPE");
        g_autofree gchar *column_type = orm_mysql_inspector_row_text (row, "COLUMN_TYPE");
        g_autofree gchar *is_nullable = orm_mysql_inspector_row_text (row, "IS_NULLABLE");
        g_autofree gchar *default_value = orm_mysql_inspector_row_text (row, "COLUMN_DEFAULT");
        g_autofree gchar *column_key = orm_mysql_inspector_row_text (row, "COLUMN_KEY");
        g_autofree gchar *extra = orm_mysql_inspector_row_text (row, "EXTRA");
        gint64            position;

        if (name == NULL)
            continue;

        /* information_schema counts from one, the record from zero. */
        position = orm_mysql_inspector_row_int (row, "ORDINAL_POSITION", 1);
        if (position < 1)
            position = 1;

        /*
         * COLUMN_TYPE is the declaration as written -- "varchar(80)",
         * "int unsigned", "enum('a','b')" -- where DATA_TYPE is only the
         * type word, so the caller gets the richer of the two and the
         * type word is left to decide how values decode.
         */
        g_ptr_array_add (columns,
                         orm_column_info_new (name,
                                              column_type != NULL ? column_type : data_type,
                                              orm_mysql_value_type_of_data_type (data_type),
                                              is_nullable != NULL &&
                                              g_ascii_strcasecmp (is_nullable, "YES") == 0,
                                              default_value,
                                              column_key != NULL &&
                                              g_ascii_strcasecmp (column_key, "PRI") == 0,
                                              orm_mysql_extra_is_autoincrement (extra),
                                              (gint) (position - 1)));
    }

    if (!orm_mysql_inspector_check_result (result, error))
    {
        g_ptr_array_unref (columns);
        return NULL;
    }

    return columns;
}

/* ------------------------------------------------------------------ */
/* Indexes                                                            */
/* ------------------------------------------------------------------ */

/*
 * Closes off the index built up in @columns and appends it to @indexes,
 * leaving @columns empty for the next one.
 *
 * information_schema.STATISTICS reports one row per indexed column, so an
 * index is only complete once a row carrying a different INDEX_NAME
 * arrives, or the rows run out.  Both of those places call this.
 */
static void
orm_mysql_inspector_emit_index (GPtrArray   *indexes,
                                const gchar *name,
                                gboolean     unique,
                                GPtrArray   *columns)
{
    g_ptr_array_add (columns, NULL);

    g_ptr_array_add (indexes,
                     orm_index_info_new (name, unique,
                                         (const gchar * const *) columns->pdata));

    /* Shrinking frees the names; the record copied what it needed. */
    g_ptr_array_set_size (columns, 0);
}

static GPtrArray *
orm_mysql_inspector_get_indexes (OrmInspector  *self,
                                 const gchar   *table,
                                 const gchar   *schema,
                                 GError       **error)
{
    g_autoptr(OrmResult)  result = NULL;
    GPtrArray            *indexes;
    GPtrArray            *columns;
    gchar                *current_name = NULL;
    gboolean              current_unique = FALSE;

    /*
     * The ordering is the grouping: rows of one index arrive together and
     * in key order, so a single pass builds each index without having to
     * hold the whole result in a hash table first.  PRIMARY comes back
     * here like any other index, which is what the caller wants -- it is
     * a real index and it is the one most lookups use.
     */
    result = orm_mysql_inspector_query (self,
                                        "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, "
                                        "COLUMN_NAME "
                                        "FROM information_schema.STATISTICS "
                                        "WHERE TABLE_SCHEMA = COALESCE(?, DATABASE()) "
                                        "AND TABLE_NAME = ? "
                                        "ORDER BY INDEX_NAME, SEQ_IN_INDEX",
                                        schema, table, error);
    if (result == NULL)
        return NULL;

    indexes = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_index_info_unref);
    columns = g_ptr_array_new_with_free_func (g_free);

    while (orm_result_next (result))
    {
        OrmRow           *row = orm_result_get_row (result);
        g_autofree gchar *name = orm_mysql_inspector_row_text (row, "INDEX_NAME");
        gchar            *column;

        if (name == NULL)
            continue;

        if (current_name != NULL && strcmp (name, current_name) != 0)
        {
            orm_mysql_inspector_emit_index (indexes, current_name,
                                            current_unique, columns);
            g_clear_pointer (&current_name, g_free);
        }

        if (current_name == NULL)
        {
            /* NON_UNIQUE is 1 for an index that permits duplicates. */
            current_name = g_strdup (name);
            current_unique = orm_mysql_inspector_row_int (row, "NON_UNIQUE", 1) == 0;
        }

        /*
         * A functional index reports a NULL COLUMN_NAME and keeps the
         * expression in a column this query does not read, so that part
         * is left out rather than recorded as a column with no name.
         */
        column = orm_mysql_inspector_row_text (row, "COLUMN_NAME");
        if (column != NULL)
            g_ptr_array_add (columns, column);
    }

    if (current_name != NULL)
    {
        orm_mysql_inspector_emit_index (indexes, current_name,
                                        current_unique, columns);
        g_clear_pointer (&current_name, g_free);
    }

    g_ptr_array_unref (columns);

    if (!orm_mysql_inspector_check_result (result, error))
    {
        g_ptr_array_unref (indexes);
        return NULL;
    }

    return indexes;
}

/* ------------------------------------------------------------------ */
/* Foreign keys                                                       */
/* ------------------------------------------------------------------ */

/*
 * One foreign key under construction.
 *
 * KEY_COLUMN_USAGE reports a composite key as one row per column, and the
 * record wants the whole key at once, so the rows sharing a
 * CONSTRAINT_NAME are accumulated here first.  Everything but the two
 * column arrays is taken from the first row of the group, since those
 * fields describe the constraint rather than the column.
 */
typedef struct
{
    gchar               *name;
    gchar               *ref_table;
    gchar               *ref_schema;
    OrmForeignKeyAction  on_delete;
    OrmForeignKeyAction  on_update;
    GPtrArray           *columns;       /* Owned gchar *: the referring side */
    GPtrArray           *ref_columns;   /* Owned gchar *: the referenced side */
} OrmMysqlForeignKeyGroup;

/*
 * Appends the accumulated key to @keys and resets @group for the next one.
 */
static void
orm_mysql_inspector_emit_foreign_key (OrmMysqlForeignKeyGroup *group,
                                      GPtrArray               *keys)
{
    if (group->name == NULL)
        return;

    /*
     * The query excludes constraints with no referenced table, so this is
     * a guard rather than a case: the record refuses one, and refusing it
     * loudly on a server that reports something unexpected is worse than
     * quietly having one fewer key.
     */
    if (group->ref_table != NULL)
    {
        g_ptr_array_add (group->columns, NULL);
        g_ptr_array_add (group->ref_columns, NULL);

        g_ptr_array_add (keys,
                         orm_foreign_key_info_new (group->name,
                                                   (const gchar * const *) group->columns->pdata,
                                                   group->ref_table,
                                                   group->ref_schema,
                                                   (const gchar * const *) group->ref_columns->pdata,
                                                   group->on_delete,
                                                   group->on_update));
    }

    g_clear_pointer (&group->name, g_free);
    g_clear_pointer (&group->ref_table, g_free);
    g_clear_pointer (&group->ref_schema, g_free);
    g_ptr_array_set_size (group->columns, 0);
    g_ptr_array_set_size (group->ref_columns, 0);
}

static GPtrArray *
orm_mysql_inspector_get_foreign_keys (OrmInspector  *self,
                                      const gchar   *table,
                                      const gchar   *schema,
                                      GError       **error)
{
    g_autoptr(OrmResult)     result = NULL;
    OrmMysqlForeignKeyGroup  group;
    GPtrArray               *keys;

    /*
     * KEY_COLUMN_USAGE knows which columns point where; only
     * REFERENTIAL_CONSTRAINTS knows what happens on delete and update, so
     * the two have to be joined.  A constraint name is unique within its
     * schema, which is what makes CONSTRAINT_SCHEMA plus CONSTRAINT_NAME
     * the right join key.  Rows for plain unique and primary keys also
     * live in KEY_COLUMN_USAGE and are excluded by requiring a referenced
     * table.
     */
    result = orm_mysql_inspector_query (self,
                                        "SELECT kcu.CONSTRAINT_NAME, kcu.COLUMN_NAME, "
                                        "kcu.ORDINAL_POSITION, "
                                        "kcu.REFERENCED_TABLE_SCHEMA, "
                                        "kcu.REFERENCED_TABLE_NAME, "
                                        "kcu.REFERENCED_COLUMN_NAME, "
                                        "rc.UPDATE_RULE, rc.DELETE_RULE "
                                        "FROM information_schema.KEY_COLUMN_USAGE AS kcu "
                                        "JOIN information_schema.REFERENTIAL_CONSTRAINTS AS rc "
                                        "ON rc.CONSTRAINT_SCHEMA = kcu.CONSTRAINT_SCHEMA "
                                        "AND rc.CONSTRAINT_NAME = kcu.CONSTRAINT_NAME "
                                        "WHERE kcu.TABLE_SCHEMA = COALESCE(?, DATABASE()) "
                                        "AND kcu.TABLE_NAME = ? "
                                        "AND kcu.REFERENCED_TABLE_NAME IS NOT NULL "
                                        "ORDER BY kcu.CONSTRAINT_NAME, kcu.ORDINAL_POSITION",
                                        schema, table, error);
    if (result == NULL)
        return NULL;

    keys = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_foreign_key_info_unref);

    memset (&group, 0, sizeof (group));
    group.columns = g_ptr_array_new_with_free_func (g_free);
    group.ref_columns = g_ptr_array_new_with_free_func (g_free);

    while (orm_result_next (result))
    {
        OrmRow           *row = orm_result_get_row (result);
        g_autofree gchar *name = orm_mysql_inspector_row_text (row, "CONSTRAINT_NAME");
        gchar            *column;
        gchar            *ref_column;

        if (name == NULL)
            continue;

        if (group.name != NULL && strcmp (name, group.name) != 0)
            orm_mysql_inspector_emit_foreign_key (&group, keys);

        if (group.name == NULL)
        {
            g_autofree gchar *delete_rule =
                orm_mysql_inspector_row_text (row, "DELETE_RULE");
            g_autofree gchar *update_rule =
                orm_mysql_inspector_row_text (row, "UPDATE_RULE");

            group.name = g_strdup (name);
            group.ref_table = orm_mysql_inspector_row_text (row, "REFERENCED_TABLE_NAME");
            group.ref_schema = orm_mysql_inspector_row_text (row, "REFERENCED_TABLE_SCHEMA");
            group.on_delete = orm_mysql_foreign_key_action_of (delete_rule);
            group.on_update = orm_mysql_foreign_key_action_of (update_rule);
        }

        column = orm_mysql_inspector_row_text (row, "COLUMN_NAME");
        ref_column = orm_mysql_inspector_row_text (row, "REFERENCED_COLUMN_NAME");

        /*
         * The two arrays are read positionally -- the nth referring column
         * points at the nth referenced one -- so a row missing either half
         * contributes neither.  A key one column short is recoverable; a
         * key whose columns point at the wrong ones is not.
         */
        if (column != NULL && ref_column != NULL)
        {
            g_ptr_array_add (group.columns, column);
            g_ptr_array_add (group.ref_columns, ref_column);
        }
        else
        {
            g_free (column);
            g_free (ref_column);
        }
    }

    orm_mysql_inspector_emit_foreign_key (&group, keys);

    g_ptr_array_unref (group.columns);
    g_ptr_array_unref (group.ref_columns);

    if (!orm_mysql_inspector_check_result (result, error))
    {
        g_ptr_array_unref (keys);
        return NULL;
    }

    return keys;
}

/* ------------------------------------------------------------------ */
/* Primary key                                                        */
/* ------------------------------------------------------------------ */

static gchar **
orm_mysql_inspector_get_primary_key (OrmInspector  *self,
                                     const gchar   *table,
                                     const gchar   *schema,
                                     GError       **error)
{
    g_autoptr(OrmResult)  result = NULL;
    GPtrArray            *names;

    /*
     * MySQL names every primary key PRIMARY and lets nothing else use the
     * name, so no join is needed to tell it from the other constraints.
     * ORDINAL_POSITION counts within the key rather than within the
     * table, which is exactly the order asked for: a key on (b, a) must
     * not be reported as (a, b).
     */
    result = orm_mysql_inspector_query (self,
                                        "SELECT COLUMN_NAME "
                                        "FROM information_schema.KEY_COLUMN_USAGE "
                                        "WHERE TABLE_SCHEMA = COALESCE(?, DATABASE()) "
                                        "AND TABLE_NAME = ? "
                                        "AND CONSTRAINT_NAME = 'PRIMARY' "
                                        "ORDER BY ORDINAL_POSITION",
                                        schema, table, error);
    if (result == NULL)
        return NULL;

    names = g_ptr_array_new_with_free_func (g_free);

    while (orm_result_next (result))
    {
        gchar *name = orm_mysql_inspector_row_text (orm_result_get_row (result),
                                                    "COLUMN_NAME");

        if (name != NULL)
            g_ptr_array_add (names, name);
    }

    if (!orm_mysql_inspector_check_result (result, error))
    {
        g_ptr_array_unref (names);
        return NULL;
    }

    /* No rows is a table without a primary key, not a failure. */
    g_ptr_array_add (names, NULL);

    return (gchar **) g_ptr_array_free (names, FALSE);
}

/* ------------------------------------------------------------------ */
/* Row count                                                          */
/* ------------------------------------------------------------------ */

/*
 * Counts the rows of @table exactly.
 *
 * The relation name cannot be a parameter, so it is quoted through the
 * dialect -- the same quoting the rest of the library emits DDL with --
 * rather than pasted in.
 */
static gint64
orm_mysql_inspector_count_rows (OrmInspector  *self,
                                const gchar   *table,
                                const gchar   *schema,
                                GError       **error)
{
    OrmConnection        *connection;
    OrmDialect           *dialect;
    OrmEngine            *engine;
    g_autofree gchar     *quoted_table = NULL;
    g_autofree gchar     *quoted_schema = NULL;
    g_autofree gchar     *sql = NULL;
    g_autoptr(OrmResult)  result = NULL;

    connection = orm_mysql_inspector_connection (self, error);
    if (connection == NULL)
        return -1;

    engine = orm_connection_get_engine (connection);
    dialect = engine != NULL ? orm_engine_get_dialect (engine) : NULL;

    if (dialect == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                     "Cannot count rows without a dialect to quote \"%s\" with",
                     table);
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

    if (!orm_result_next (result))
    {
        if (!orm_mysql_inspector_check_result (result, error))
            return -1;

        g_set_error (error, ORM_ERROR, ORM_ERROR_QUERY_FAILED,
                     "COUNT(*) on \"%s\" returned no rows", table);
        return -1;
    }

    return orm_mysql_inspector_row_int (orm_result_get_row (result), "row_count", 0);
}

static gint64
orm_mysql_inspector_estimate_row_count (OrmInspector  *self,
                                        const gchar   *table,
                                        const gchar   *schema,
                                        gboolean      *is_estimate,
                                        GError       **error)
{
    g_autoptr(OrmResult)  result = NULL;
    OrmRow               *row;

    result = orm_mysql_inspector_query (self,
                                        "SELECT TABLE_ROWS "
                                        "FROM information_schema.TABLES "
                                        "WHERE TABLE_SCHEMA = COALESCE(?, DATABASE()) "
                                        "AND TABLE_NAME = ?",
                                        schema, table, error);
    if (result == NULL)
        return -1;

    if (!orm_result_next (result))
    {
        if (!orm_mysql_inspector_check_result (result, error))
            return -1;

        g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_FOUND,
                     "No relation named \"%s\" in this schema", table);
        return -1;
    }

    row = orm_result_get_row (result);

    /*
     * TABLE_ROWS is NULL for a view, which has no rows of its own, and
     * for any engine that keeps no statistics; those are the cases worth
     * paying for an exact count.
     */
    if (orm_mysql_inspector_row_is_null (row, "TABLE_ROWS"))
    {
        gint64 count = orm_mysql_inspector_count_rows (self, table, schema, error);

        if (count >= 0 && is_estimate != NULL)
            *is_estimate = FALSE;

        return count;
    }

    /*
     * InnoDB derives TABLE_ROWS from a sample of the index pages, so it
     * can be off by tens of percent and is not usable as an answer to
     * "how many rows are there" -- only as an answer to "roughly how big
     * is this", which is what a table listing is asking.
     */
    if (is_estimate != NULL)
        *is_estimate = TRUE;

    return orm_mysql_inspector_row_int (row, "TABLE_ROWS", 0);
}

/* ------------------------------------------------------------------ */
/* Type                                                               */
/* ------------------------------------------------------------------ */

static void
orm_mysql_inspector_class_init (OrmMysqlInspectorClass *klass)
{
    OrmInspectorClass *inspector_class = ORM_INSPECTOR_CLASS (klass);

    inspector_class->list_schemas = orm_mysql_inspector_list_schemas;
    inspector_class->list_relations = orm_mysql_inspector_list_relations;
    inspector_class->get_columns = orm_mysql_inspector_get_columns;
    inspector_class->get_indexes = orm_mysql_inspector_get_indexes;
    inspector_class->get_foreign_keys = orm_mysql_inspector_get_foreign_keys;
    inspector_class->get_primary_key = orm_mysql_inspector_get_primary_key;
    inspector_class->estimate_row_count = orm_mysql_inspector_estimate_row_count;
}

static void
orm_mysql_inspector_init (OrmMysqlInspector *self)
{
}

/**
 * orm_mysql_inspector_new:
 * @connection: An open #OrmConnection to a MySQL or MariaDB server
 *
 * Creates an inspector that reads the shape of the database through
 * `information_schema`.
 *
 * Returns: (transfer full): A new #OrmMysqlInspector
 */
OrmMysqlInspector *
orm_mysql_inspector_new (OrmConnection *connection)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (connection), NULL);

    return g_object_new (ORM_TYPE_MYSQL_INSPECTOR,
                         "connection", connection,
                         NULL);
}
