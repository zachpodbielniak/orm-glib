/* orm-mysql-driver.c
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

#include "orm-mysql-driver.h"
#include "../../core/orm-error.h"
#include "../../engine/orm-engine.h"
#include "../../dialect/mysql/orm-mysql-dialect.h"

#include <mysql/mysql.h>
#include <string.h>

/*
 * The MySQL/MariaDB backend: driver, connection and result.
 *
 * All three live in one file because they are one implementation -- the
 * result holds a result set belonging to the connection's handle, and
 * splitting them across translation units would mean exporting that
 * relationship rather than keeping it private.
 *
 * MySQL does not stream here: a query is run with mysql_store_result, so
 * the whole result set is already in client memory before the first row
 * is handed out.  ORM_QUERY_FLAGS_STREAMING is therefore accepted and
 * ignored rather than being a second code path; mysql_use_result belongs
 * with the asynchronous work, where the caller can afford to keep the
 * connection tied up for the length of the iteration.
 */

/* Internal: the engine holds the parsed connection parameters. */
extern const gchar * orm_engine_get_host (OrmEngine *engine);
extern gint orm_engine_get_port (OrmEngine *engine);
extern const gchar * orm_engine_get_username (OrmEngine *engine);
extern const gchar * orm_engine_get_password (OrmEngine *engine);
extern const gchar * orm_engine_get_database (OrmEngine *engine);

/* ------------------------------------------------------------------ */
/* Result                                                             */
/* ------------------------------------------------------------------ */

#define ORM_TYPE_MYSQL_DRIVER_RESULT (orm_mysql_driver_result_get_type ())

G_DECLARE_FINAL_TYPE (OrmMysqlDriverResult, orm_mysql_driver_result,
                      ORM, MYSQL_DRIVER_RESULT, OrmDriverResult)

/*
 * A result arrives by one of two routes and this type serves both.
 *
 * A plain query leaves a MYSQL_RES to walk with mysql_fetch_row.  A
 * prepared statement cannot be walked that way -- its rows live in the
 * statement, not the metadata -- so that path reads every row out while
 * the statement is still open and stores them here.  Since MySQL
 * materializes client-side either way, pre-reading costs nothing that
 * was not already being paid.
 *
 * @rows non-%NULL selects the second mode; @result is then metadata only.
 */
struct _OrmMysqlDriverResult
{
    OrmDriverResult  parent_instance;

    MYSQL_RES       *result;
    gboolean         exhausted;

    GPtrArray       *rows;        /* owned OrmRow*, or NULL in MYSQL_RES mode */
    guint            next_row;
};

G_DEFINE_FINAL_TYPE (OrmMysqlDriverResult, orm_mysql_driver_result,
                     ORM_TYPE_DRIVER_RESULT)

/*
 * Turns one column's bytes into an OrmValue, using the field's declared
 * type to decide how to read them.
 *
 * Both result paths funnel through here.  MySQL hands back text either
 * way -- MYSQL_ROW from a plain query, and string-bound output buffers
 * from a prepared statement -- so the conversion is genuinely shared
 * rather than merely similar, and a type mapping fixed in one place
 * cannot drift out of the other.
 */
static OrmValue *
orm_mysql_value_from_field (const MYSQL_FIELD *field,
                            const gchar       *val,
                            unsigned long      length)
{
    if (val == NULL)
        return orm_value_new_null ();

    switch (field->type)
    {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_INT24:
        return orm_value_new_integer (g_ascii_strtoll (val, NULL, 10));

    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
        return orm_value_new_float (g_ascii_strtod (val, NULL));

    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
        {
            /*
             * The length matters here and nowhere else: a blob may contain
             * a NUL byte, which strlen would treat as the end of it.
             */
            GBytes   *bytes = g_bytes_new (val, length);
            OrmValue *value = orm_value_new_blob (bytes);

            g_bytes_unref (bytes);
            return value;
        }

    default:
        /* VARCHAR, TEXT, DATE, ENUM and everything else read as text. */
        return orm_value_new_string (val);
    }
}

static OrmRow *
orm_mysql_build_row (MYSQL_RES *result,
                     MYSQL_ROW  row)
{
    GPtrArray     *names;
    GPtrArray     *values;
    MYSQL_FIELD   *fields;
    unsigned long *lengths;
    guint          col_count;
    guint          i;

    col_count = mysql_num_fields (result);
    fields = mysql_fetch_fields (result);
    lengths = mysql_fetch_lengths (result);

    names = g_ptr_array_new_full (col_count, g_free);
    values = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_value_free);

    for (i = 0; i < col_count; i++)
    {
        g_ptr_array_add (names, g_strdup (fields[i].name));
        g_ptr_array_add (values,
                         orm_mysql_value_from_field (&fields[i], row[i],
                                                     lengths != NULL ? lengths[i] : 0));
    }

    return orm_row_new (names, values);
}

static gint
orm_mysql_driver_result_get_column_count (OrmDriverResult *result)
{
    OrmMysqlDriverResult *self = ORM_MYSQL_DRIVER_RESULT (result);

    if (self->result == NULL)
        return 0;

    return (gint) mysql_num_fields (self->result);
}

static const gchar *
orm_mysql_driver_result_get_column_name (OrmDriverResult *result,
                                         gint             index)
{
    OrmMysqlDriverResult *self = ORM_MYSQL_DRIVER_RESULT (result);
    MYSQL_FIELD          *fields;

    if (self->result == NULL)
        return NULL;

    /* Nothing in the client library bounds-checks the field array. */
    if (index < 0 || (guint) index >= mysql_num_fields (self->result))
        return NULL;

    fields = mysql_fetch_fields (self->result);

    return fields[index].name;
}

/*
 * Maps a MySQL field type onto a value type.  Kept beside the value
 * conversion above so the two cannot disagree about what a column is.
 */
static OrmValueType
orm_mysql_value_type_of (enum enum_field_types type)
{
    switch (type)
    {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_INT24:
        return ORM_VALUE_INTEGER;

    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
        return ORM_VALUE_FLOAT;

    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
        return ORM_VALUE_BLOB;

    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_NEWDATE:
        return ORM_VALUE_DATETIME;

    default:
        return ORM_VALUE_STRING;
    }
}

/*
 * Spells a field type the way the schema would.
 *
 * MySQL reports BOOLEAN as TINYINT(1) and has no way to say otherwise,
 * so this reports what the server reports rather than guessing at intent.
 */
static const gchar *
orm_mysql_type_name_of (const MYSQL_FIELD *field)
{
    switch (field->type)
    {
    case MYSQL_TYPE_TINY:        return "TINYINT";
    case MYSQL_TYPE_SHORT:       return "SMALLINT";
    case MYSQL_TYPE_INT24:       return "MEDIUMINT";
    case MYSQL_TYPE_LONG:        return "INT";
    case MYSQL_TYPE_LONGLONG:    return "BIGINT";
    case MYSQL_TYPE_FLOAT:       return "FLOAT";
    case MYSQL_TYPE_DOUBLE:      return "DOUBLE";
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:  return "DECIMAL";
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE:     return "DATE";
    case MYSQL_TYPE_TIME:        return "TIME";
    case MYSQL_TYPE_DATETIME:    return "DATETIME";
    case MYSQL_TYPE_TIMESTAMP:   return "TIMESTAMP";
    case MYSQL_TYPE_YEAR:        return "YEAR";
    case MYSQL_TYPE_STRING:      return "CHAR";
    case MYSQL_TYPE_VAR_STRING:  return "VARCHAR";
    case MYSQL_TYPE_TINY_BLOB:   return "TINYBLOB";
    case MYSQL_TYPE_MEDIUM_BLOB: return "MEDIUMBLOB";
    case MYSQL_TYPE_LONG_BLOB:   return "LONGBLOB";
    case MYSQL_TYPE_BLOB:        return "BLOB";
    case MYSQL_TYPE_JSON:        return "JSON";
    case MYSQL_TYPE_ENUM:        return "ENUM";
    case MYSQL_TYPE_SET:         return "SET";
    case MYSQL_TYPE_BIT:         return "BIT";
    case MYSQL_TYPE_NULL:        return "NULL";
    default:                     return NULL;
    }
}

/*
 * Both accessors need the field array, which lives in the metadata even
 * when the rows came out of a prepared statement.
 */
static const MYSQL_FIELD *
orm_mysql_driver_result_field (OrmMysqlDriverResult *self,
                               gint                  index)
{
    if (self->result == NULL)
        return NULL;

    if (index < 0 || (guint) index >= mysql_num_fields (self->result))
        return NULL;

    return &mysql_fetch_fields (self->result)[index];
}

static OrmValueType
orm_mysql_driver_result_get_column_value_type (OrmDriverResult *result,
                                               gint             index)
{
    const MYSQL_FIELD *field;

    field = orm_mysql_driver_result_field (ORM_MYSQL_DRIVER_RESULT (result), index);
    if (field == NULL)
        return ORM_VALUE_NULL;

    return orm_mysql_value_type_of (field->type);
}

static const gchar *
orm_mysql_driver_result_get_column_type_name (OrmDriverResult *result,
                                              gint             index)
{
    const MYSQL_FIELD *field;

    field = orm_mysql_driver_result_field (ORM_MYSQL_DRIVER_RESULT (result), index);
    if (field == NULL)
        return NULL;

    return orm_mysql_type_name_of (field);
}

static OrmRow *
orm_mysql_driver_result_fetch_row (OrmDriverResult  *result,
                                   GError          **error)
{
    OrmMysqlDriverResult *self = ORM_MYSQL_DRIVER_RESULT (result);
    MYSQL_ROW             row;

    if (self->exhausted)
        return NULL;

    /* Rows already read out of a prepared statement. */
    if (self->rows != NULL)
    {
        if (self->next_row >= self->rows->len)
        {
            self->exhausted = TRUE;
            return NULL;
        }

        return g_object_ref (g_ptr_array_index (self->rows, self->next_row++));
    }

    if (self->result == NULL)
        return NULL;

    row = mysql_fetch_row (self->result);

    if (row == NULL)
    {
        /*
         * The rows are already in client memory, so there is no server
         * round trip left to fail: a %NULL here is the end of the result
         * set rather than a broken fetch.  A streaming result read with
         * mysql_use_result would have to ask mysql_errno() to tell the
         * two apart.
         */
        self->exhausted = TRUE;
        return NULL;
    }

    return orm_mysql_build_row (self->result, row);
}

static void
orm_mysql_driver_result_close (OrmDriverResult *result)
{
    OrmMysqlDriverResult *self = ORM_MYSQL_DRIVER_RESULT (result);

    if (self->result != NULL)
    {
        mysql_free_result (self->result);
        self->result = NULL;
    }

    g_clear_pointer (&self->rows, g_ptr_array_unref);

    self->exhausted = TRUE;
}

static void
orm_mysql_driver_result_finalize (GObject *object)
{
    orm_mysql_driver_result_close (ORM_DRIVER_RESULT (object));

    G_OBJECT_CLASS (orm_mysql_driver_result_parent_class)->finalize (object);
}

static void
orm_mysql_driver_result_class_init (OrmMysqlDriverResultClass *klass)
{
    GObjectClass         *object_class = G_OBJECT_CLASS (klass);
    OrmDriverResultClass *result_class = ORM_DRIVER_RESULT_CLASS (klass);

    object_class->finalize = orm_mysql_driver_result_finalize;

    result_class->get_column_count = orm_mysql_driver_result_get_column_count;
    result_class->get_column_name = orm_mysql_driver_result_get_column_name;
    result_class->get_column_value_type = orm_mysql_driver_result_get_column_value_type;
    result_class->get_column_type_name = orm_mysql_driver_result_get_column_type_name;
    result_class->fetch_row = orm_mysql_driver_result_fetch_row;
    result_class->close = orm_mysql_driver_result_close;
}

static void
orm_mysql_driver_result_init (OrmMysqlDriverResult *self)
{
    self->result = NULL;
    self->exhausted = FALSE;
}

/* ------------------------------------------------------------------ */
/* Connection                                                         */
/* ------------------------------------------------------------------ */

#define ORM_TYPE_MYSQL_DRIVER_CONNECTION (orm_mysql_driver_connection_get_type ())

G_DECLARE_FINAL_TYPE (OrmMysqlDriverConnection, orm_mysql_driver_connection,
                      ORM, MYSQL_DRIVER_CONNECTION, OrmDriverConnection)

struct _OrmMysqlDriverConnection
{
    OrmDriverConnection  parent_instance;

    MYSQL               *conn;
};

G_DEFINE_FINAL_TYPE (OrmMysqlDriverConnection, orm_mysql_driver_connection,
                     ORM_TYPE_DRIVER_CONNECTION)

/*
 * Describes one OrmValue to @bind.
 *
 * MYSQL_BIND only points at the caller's memory, so every non-NULL value
 * needs a buffer that outlives the execute.  That buffer is returned
 * through @buffer, and the caller frees it once the statement has run.
 */
static gboolean
orm_mysql_bind_value (MYSQL_BIND  *bind,
                      OrmValue    *value,
                      gpointer    *buffer,
                      GError     **error)
{
    memset (bind, 0, sizeof (MYSQL_BIND));

    if (value == NULL)
    {
        bind->buffer_type = MYSQL_TYPE_NULL;
        *buffer = NULL;
        return TRUE;
    }

    switch (orm_value_get_value_type (value))
    {
    case ORM_VALUE_NULL:
        bind->buffer_type = MYSQL_TYPE_NULL;
        *buffer = NULL;
        break;

    case ORM_VALUE_INTEGER:
        {
            gint64 *val = g_new (gint64, 1);

            *val = orm_value_get_integer (value);
            bind->buffer_type = MYSQL_TYPE_LONGLONG;
            bind->buffer = val;
            bind->is_unsigned = FALSE;
            *buffer = val;
        }
        break;

    case ORM_VALUE_FLOAT:
        {
            gdouble *val = g_new (gdouble, 1);

            *val = orm_value_get_float (value);
            bind->buffer_type = MYSQL_TYPE_DOUBLE;
            bind->buffer = val;
            *buffer = val;
        }
        break;

    case ORM_VALUE_STRING:
        {
            gchar *dup = g_strdup (orm_value_get_string (value));

            bind->buffer_type = MYSQL_TYPE_STRING;
            bind->buffer = dup;
            bind->buffer_length = strlen (dup);
            *buffer = dup;
        }
        break;

    case ORM_VALUE_BOOLEAN:
        {
            gint8 *val = g_new (gint8, 1);

            *val = orm_value_get_boolean (value) ? 1 : 0;
            bind->buffer_type = MYSQL_TYPE_TINY;
            bind->buffer = val;
            *buffer = val;
        }
        break;

    case ORM_VALUE_DATETIME:
        {
            GDateTime        *dt = orm_value_get_datetime (value);
            g_autofree gchar *iso = g_date_time_format_iso8601 (dt);
            gchar            *dup = g_strdup (iso);

            bind->buffer_type = MYSQL_TYPE_STRING;
            bind->buffer = dup;
            bind->buffer_length = strlen (dup);
            *buffer = dup;
        }
        break;

    case ORM_VALUE_BLOB:
        {
            GBytes       *bytes = orm_value_get_blob (value);
            gsize         size;
            gconstpointer data = g_bytes_get_data (bytes, &size);
            gchar        *dup = g_memdup2 (data, size);

            bind->buffer_type = MYSQL_TYPE_BLOB;
            bind->buffer = dup;
            bind->buffer_length = size;
            *buffer = dup;
        }
        break;

    default:
        bind->buffer_type = MYSQL_TYPE_NULL;
        *buffer = NULL;
        break;
    }

    return TRUE;
}

/*
 * Prepares @sql, binds @params and runs it.
 *
 * Returns the executed statement, which the caller closes, or %NULL with
 * @error set.  The parameter buffers are freed here because they are only
 * needed until mysql_stmt_execute has read them, and keeping their
 * lifetime inside one function is what stops an early failure from
 * leaking them.
 */
static MYSQL_STMT *
orm_mysql_run_prepared (OrmMysqlDriverConnection  *self,
                        const gchar               *sql,
                        GList                     *params,
                        GError                   **error)
{
    MYSQL_STMT *stmt;
    MYSQL_BIND *binds = NULL;
    gpointer   *buffers = NULL;
    GList      *l;
    gint        n_params;
    gint        i;
    gboolean    success = TRUE;

    stmt = mysql_stmt_init (self->conn);
    if (stmt == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_PREPARE,
                     "Failed to initialize statement");
        return NULL;
    }

    if (mysql_stmt_prepare (stmt, sql, strlen (sql)) != 0)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_PREPARE,
                     "Failed to prepare statement: %s",
                     mysql_stmt_error (stmt));
        mysql_stmt_close (stmt);
        return NULL;
    }

    n_params = (gint) g_list_length (params);

    if (n_params > 0)
    {
        /*
         * The two arrays are parallel: binds points into buffers, and
         * buffers is zeroed up front so a bind that gives up half way
         * still leaves every slot safe to free.
         */
        binds = g_new0 (MYSQL_BIND, n_params);
        buffers = g_new0 (gpointer, n_params);

        i = 0;
        for (l = params; l != NULL; l = l->next, i++)
        {
            if (!orm_mysql_bind_value (&binds[i], (OrmValue *) l->data,
                                       &buffers[i], error))
            {
                success = FALSE;
                break;
            }
        }

        if (success && mysql_stmt_bind_param (stmt, binds) != 0)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_BIND,
                         "Failed to bind parameters: %s",
                         mysql_stmt_error (stmt));
            success = FALSE;
        }
    }

    if (success && mysql_stmt_execute (stmt) != 0)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                     "Failed to execute: %s",
                     mysql_stmt_error (stmt));
        success = FALSE;
    }

    if (buffers != NULL)
    {
        for (i = 0; i < n_params; i++)
            g_free (buffers[i]);

        g_free (buffers);
    }

    g_free (binds);

    if (!success)
    {
        mysql_stmt_close (stmt);
        return NULL;
    }

    return stmt;
}

/*
 * Spells an isolation level the way MySQL spells it.
 */
static const gchar *
orm_mysql_isolation_words (OrmIsolationLevel level)
{
    switch (level)
    {
    case ORM_ISOLATION_READ_UNCOMMITTED:
        return "READ UNCOMMITTED";

    case ORM_ISOLATION_READ_COMMITTED:
        return "READ COMMITTED";

    case ORM_ISOLATION_REPEATABLE_READ:
        return "REPEATABLE READ";

    case ORM_ISOLATION_SERIALIZABLE:
        return "SERIALIZABLE";

    default:
        return NULL;
    }
}

static void
orm_mysql_driver_connection_close (OrmDriverConnection *connection)
{
    OrmMysqlDriverConnection *self = ORM_MYSQL_DRIVER_CONNECTION (connection);

    if (self->conn != NULL)
    {
        mysql_close (self->conn);
        self->conn = NULL;
    }
}

static gboolean
orm_mysql_driver_connection_execute (OrmDriverConnection  *connection,
                                     const gchar          *sql,
                                     GList                *params,
                                     GError              **error)
{
    OrmMysqlDriverConnection *self = ORM_MYSQL_DRIVER_CONNECTION (connection);
    MYSQL_STMT               *stmt;

    g_return_val_if_fail (self->conn != NULL, FALSE);

    if (params == NULL)
    {
        if (mysql_query (self->conn, sql) != 0)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                         "SQL error: %s", mysql_error (self->conn));
            return FALSE;
        }

        return TRUE;
    }

    stmt = orm_mysql_run_prepared (self, sql, params, error);
    if (stmt == NULL)
        return FALSE;

    mysql_stmt_close (stmt);
    return TRUE;
}

/*
 * Reads every row out of an executed prepared statement.
 *
 * Output columns are bound as strings with zero-length buffers, so the
 * first mysql_stmt_fetch reports the real length of each column instead
 * of truncating to a guess; the value is then pulled with
 * mysql_stmt_fetch_column into a buffer of exactly that size.  Binding
 * fixed-size buffers up front is the usual shortcut and it silently
 * truncates any TEXT or BLOB larger than the guess, which is precisely
 * the data you least want quietly shortened.
 *
 * Because every column arrives as text, the values go through the same
 * conversion the plain-query path uses.
 *
 * Returns: (transfer full) (nullable): the rows, or %NULL on error
 */
static GPtrArray *
orm_mysql_fetch_stmt_rows (MYSQL_STMT  *stmt,
                           MYSQL_RES   *metadata,
                           GError     **error)
{
    GPtrArray     *rows;
    MYSQL_BIND    *binds;
    MYSQL_FIELD   *fields;
    unsigned long *lengths;
    my_bool       *is_null;
    my_bool       *is_error;
    guint          col_count;
    guint          i;
    gint           rc;

    col_count = mysql_num_fields (metadata);
    fields = mysql_fetch_fields (metadata);

    if (mysql_stmt_store_result (stmt) != 0)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                     "Failed to buffer result: %s", mysql_stmt_error (stmt));
        return NULL;
    }

    binds = g_new0 (MYSQL_BIND, col_count);
    lengths = g_new0 (unsigned long, col_count);
    is_null = g_new0 (my_bool, col_count);
    is_error = g_new0 (my_bool, col_count);

    for (i = 0; i < col_count; i++)
    {
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = NULL;
        binds[i].buffer_length = 0;
        binds[i].length = &lengths[i];
        binds[i].is_null = &is_null[i];
        binds[i].error = &is_error[i];
    }

    if (mysql_stmt_bind_result (stmt, binds) != 0)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_BIND,
                     "Failed to bind result columns: %s", mysql_stmt_error (stmt));
        g_free (binds);
        g_free (lengths);
        g_free (is_null);
        g_free (is_error);
        return NULL;
    }

    rows = g_ptr_array_new_with_free_func (g_object_unref);

    /*
     * MYSQL_DATA_TRUNCATED is the expected outcome of every fetch here,
     * not a problem: the buffers are deliberately empty, so every
     * non-NULL column "truncates" and reports its length.
     */
    while ((rc = mysql_stmt_fetch (stmt)) == 0 || rc == MYSQL_DATA_TRUNCATED)
    {
        GPtrArray *names;
        GPtrArray *values;

        names = g_ptr_array_new_full (col_count, g_free);
        values = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_value_free);

        for (i = 0; i < col_count; i++)
        {
            g_ptr_array_add (names, g_strdup (fields[i].name));

            if (is_null[i])
            {
                g_ptr_array_add (values, orm_value_new_null ());
            }
            else
            {
                g_autofree gchar *buffer = NULL;
                MYSQL_BIND        column;

                buffer = g_malloc0 (lengths[i] + 1);

                memset (&column, 0, sizeof (MYSQL_BIND));
                column.buffer_type = MYSQL_TYPE_STRING;
                column.buffer = buffer;
                column.buffer_length = lengths[i];
                column.length = &lengths[i];
                column.is_null = &is_null[i];
                column.error = &is_error[i];

                if (mysql_stmt_fetch_column (stmt, &column, i, 0) != 0)
                {
                    g_ptr_array_add (values, orm_value_new_null ());
                }
                else
                {
                    g_ptr_array_add (values,
                                     orm_mysql_value_from_field (&fields[i],
                                                                 buffer,
                                                                 lengths[i]));
                }
            }
        }

        g_ptr_array_add (rows, orm_row_new (names, values));
    }

    g_free (binds);
    g_free (lengths);
    g_free (is_null);
    g_free (is_error);

    if (rc != MYSQL_NO_DATA)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                     "Failed to fetch row: %s", mysql_stmt_error (stmt));
        g_ptr_array_unref (rows);
        return NULL;
    }

    return rows;
}

static OrmDriverResult *
orm_mysql_driver_connection_query (OrmDriverConnection  *connection,
                                   const gchar          *sql,
                                   GList                *params,
                                   OrmQueryFlags         flags,
                                   GError              **error)
{
    OrmMysqlDriverConnection *self = ORM_MYSQL_DRIVER_CONNECTION (connection);
    OrmMysqlDriverResult     *result;
    MYSQL_RES                *res;
    GPtrArray                *rows = NULL;

    g_return_val_if_fail (self->conn != NULL, NULL);

    if (params != NULL)
    {
        MYSQL_STMT *stmt;

        stmt = orm_mysql_run_prepared (self, sql, params, error);
        if (stmt == NULL)
            return NULL;

        /*
         * A prepared statement's rows belong to the statement, not to the
         * metadata, so they have to be read out before it closes.
         */
        res = mysql_stmt_result_metadata (stmt);

        if (res != NULL)
        {
            rows = orm_mysql_fetch_stmt_rows (stmt, res, error);

            if (rows == NULL)
            {
                mysql_free_result (res);
                mysql_stmt_close (stmt);
                return NULL;
            }
        }

        mysql_stmt_close (stmt);
    }
    else
    {
        if (mysql_query (self->conn, sql) != 0)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                         "Query failed: %s", mysql_error (self->conn));
            return NULL;
        }

        res = mysql_store_result (self->conn);
    }

    /*
     * A statement that returns nothing -- an INSERT run through query(),
     * say -- leaves @res %NULL, and that is a result with no columns and
     * no rows rather than a failure.
     */
    result = g_object_new (ORM_TYPE_MYSQL_DRIVER_RESULT, NULL);
    result->result = res;
    result->rows = rows;

    return ORM_DRIVER_RESULT (result);
}

static gint64
orm_mysql_driver_connection_get_last_insert_id (OrmDriverConnection *connection)
{
    OrmMysqlDriverConnection *self = ORM_MYSQL_DRIVER_CONNECTION (connection);

    if (self->conn == NULL)
        return 0;

    return (gint64) mysql_insert_id (self->conn);
}

static gint
orm_mysql_driver_connection_get_changes (OrmDriverConnection *connection)
{
    OrmMysqlDriverConnection *self = ORM_MYSQL_DRIVER_CONNECTION (connection);

    if (self->conn == NULL)
        return 0;

    return (gint) mysql_affected_rows (self->conn);
}

static gboolean
orm_mysql_driver_connection_set_isolation_level (OrmDriverConnection  *connection,
                                                 OrmIsolationLevel     level,
                                                 gboolean              for_next_transaction,
                                                 GError              **error)
{
    const gchar      *words;
    g_autofree gchar *sql = NULL;

    words = orm_mysql_isolation_words (level);
    if (words == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                     "Unknown isolation level: %d", (gint) level);
        return FALSE;
    }

    /*
     * MySQL's SET TRANSACTION configures the transaction about to start,
     * so the per-transaction form has to be issued before BEGIN rather
     * than inside the transaction.
     */
    sql = g_strdup_printf (for_next_transaction
                           ? "SET TRANSACTION ISOLATION LEVEL %s"
                           : "SET SESSION TRANSACTION ISOLATION LEVEL %s",
                           words);

    return orm_driver_connection_execute (connection, sql, NULL, error);
}

static gboolean
orm_mysql_driver_connection_interrupt (OrmDriverConnection  *connection,
                                       GError              **error)
{
    /*
     * There is no same-connection cancel here.  The connection running
     * the statement is blocked waiting on the server, and the only way to
     * stop it is a second connection issuing KILL QUERY against the
     * thread id captured by mysql_thread_id() at connect time.  That
     * means owning a spare connection for the lifetime of every
     * connection, which is deliberately left until the pooling work makes
     * one available.
     */
    g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                 "MySQL cannot interrupt a statement from the connection "
                 "running it; cancelling requires a second connection "
                 "issuing KILL QUERY");
    return FALSE;
}

static void
orm_mysql_driver_connection_finalize (GObject *object)
{
    orm_mysql_driver_connection_close (ORM_DRIVER_CONNECTION (object));

    G_OBJECT_CLASS (orm_mysql_driver_connection_parent_class)->finalize (object);
}

static void
orm_mysql_driver_connection_class_init (OrmMysqlDriverConnectionClass *klass)
{
    GObjectClass             *object_class = G_OBJECT_CLASS (klass);
    OrmDriverConnectionClass *conn_class = ORM_DRIVER_CONNECTION_CLASS (klass);

    object_class->finalize = orm_mysql_driver_connection_finalize;

    conn_class->close = orm_mysql_driver_connection_close;
    conn_class->execute = orm_mysql_driver_connection_execute;
    conn_class->query = orm_mysql_driver_connection_query;
    conn_class->get_last_insert_id = orm_mysql_driver_connection_get_last_insert_id;
    conn_class->get_changes = orm_mysql_driver_connection_get_changes;
    conn_class->set_isolation_level = orm_mysql_driver_connection_set_isolation_level;
    conn_class->interrupt = orm_mysql_driver_connection_interrupt;
}

static void
orm_mysql_driver_connection_init (OrmMysqlDriverConnection *self)
{
    self->conn = NULL;
}

/* ------------------------------------------------------------------ */
/* Driver                                                             */
/* ------------------------------------------------------------------ */

struct _OrmMysqlDriver
{
    OrmDriver parent_instance;
};

G_DEFINE_FINAL_TYPE (OrmMysqlDriver, orm_mysql_driver, ORM_TYPE_DRIVER)

static const gchar *
orm_mysql_driver_get_name (OrmDriver *driver)
{
    return "mysql";
}

static const gchar * const *
orm_mysql_driver_get_schemes (OrmDriver *driver)
{
    static const gchar * const schemes[] = { "mysql", "mariadb", NULL };

    return schemes;
}

static OrmDialectType
orm_mysql_driver_get_dialect_type (OrmDriver *driver)
{
    return ORM_DIALECT_MYSQL;
}

static OrmDialect *
orm_mysql_driver_create_dialect (OrmDriver *driver)
{
    return ORM_DIALECT (orm_mysql_dialect_new ());
}

static OrmDriverConnection *
orm_mysql_driver_open (OrmDriver  *driver,
                       OrmEngine  *engine,
                       GError    **error)
{
    OrmMysqlDriverConnection *self;
    MYSQL                    *conn;
    const gchar              *host;
    const gchar              *user;
    const gchar              *pass;
    const gchar              *dbname;
    gint                      port;

    host = orm_engine_get_host (engine);
    port = orm_engine_get_port (engine);
    user = orm_engine_get_username (engine);
    pass = orm_engine_get_password (engine);
    dbname = orm_engine_get_database (engine);

    conn = mysql_init (NULL);
    if (conn == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                     "Failed to initialize MySQL client");
        return NULL;
    }

    if (mysql_real_connect (conn,
                            host,
                            user,
                            pass,
                            dbname,
                            (unsigned int) port,
                            NULL,   /* Unix socket */
                            0)      /* Client flags */
        == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                     "Failed to connect to MySQL: %s",
                     mysql_error (conn));
        mysql_close (conn);
        return NULL;
    }

    /*
     * utf8mb4 rather than the server default, which on older servers is
     * the three-byte "utf8" that cannot hold anything outside the basic
     * multilingual plane.
     */
    mysql_set_character_set (conn, "utf8mb4");

    self = g_object_new (ORM_TYPE_MYSQL_DRIVER_CONNECTION, NULL);
    self->conn = conn;

    return ORM_DRIVER_CONNECTION (self);
}

static void
orm_mysql_driver_class_init (OrmMysqlDriverClass *klass)
{
    OrmDriverClass *driver_class = ORM_DRIVER_CLASS (klass);

    driver_class->get_name = orm_mysql_driver_get_name;
    driver_class->get_schemes = orm_mysql_driver_get_schemes;
    driver_class->get_dialect_type = orm_mysql_driver_get_dialect_type;
    driver_class->create_dialect = orm_mysql_driver_create_dialect;
    driver_class->open = orm_mysql_driver_open;
}

static void
orm_mysql_driver_init (OrmMysqlDriver *self)
{
}

/**
 * orm_mysql_driver_new:
 *
 * Creates the MySQL/MariaDB driver.
 *
 * Returns: (transfer full): A new #OrmMysqlDriver
 */
OrmMysqlDriver *
orm_mysql_driver_new (void)
{
    return g_object_new (ORM_TYPE_MYSQL_DRIVER, NULL);
}
