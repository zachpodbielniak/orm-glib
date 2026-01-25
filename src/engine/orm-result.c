/* orm-result.c
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

#include "orm-result.h"
#include "orm-connection.h"
#include "../core/orm-error.h"

#include <string.h>
#include <stdlib.h>

#ifdef ORM_ENABLE_SQLITE
#include <sqlite3.h>
#endif

#ifdef ORM_ENABLE_POSTGRES
#include <libpq-fe.h>
#endif

#ifdef ORM_ENABLE_MYSQL
#include <mysql/mysql.h>
#endif

/*
 * OrmResult - Query result set.
 *
 * Wraps the underlying database cursor and provides row iteration.
 */

struct _OrmResult
{
    GObject parent_instance;

    OrmConnection *connection;  /* Weak reference */
    GPtrArray     *column_names;
    OrmRow        *current_row;
    gboolean       is_closed;
    gboolean       has_more;
    OrmDialectType dialect_type;

#ifdef ORM_ENABLE_SQLITE
    sqlite3_stmt  *sqlite_stmt;
#endif

#ifdef ORM_ENABLE_POSTGRES
    PGresult      *pg_result;
    int            pg_row_count;
    int            pg_current_row;
#endif

#ifdef ORM_ENABLE_MYSQL
    MYSQL_RES     *mysql_result;
    MYSQL_ROW      mysql_current_row;
#endif
};

G_DEFINE_TYPE (OrmResult, orm_result, G_TYPE_OBJECT)

#ifdef ORM_ENABLE_SQLITE
/* Internal: Get SQLite handle from connection */
extern sqlite3 * orm_connection_get_sqlite_db (OrmConnection *self);

/*
 * Extract column value from SQLite statement.
 */
static OrmValue *
sqlite_column_to_value (sqlite3_stmt *stmt, int col)
{
    int col_type;

    col_type = sqlite3_column_type (stmt, col);

    switch (col_type)
    {
    case SQLITE_NULL:
        return orm_value_new_null ();

    case SQLITE_INTEGER:
        return orm_value_new_integer (sqlite3_column_int64 (stmt, col));

    case SQLITE_FLOAT:
        return orm_value_new_float (sqlite3_column_double (stmt, col));

    case SQLITE_TEXT:
        return orm_value_new_string ((const gchar *) sqlite3_column_text (stmt, col));

    case SQLITE_BLOB:
        {
            const void *data = sqlite3_column_blob (stmt, col);
            int size = sqlite3_column_bytes (stmt, col);
            GBytes *bytes = g_bytes_new (data, size);
            OrmValue *value = orm_value_new_blob (bytes);
            g_bytes_unref (bytes);
            return value;
        }

    default:
        return orm_value_new_null ();
    }
}

/*
 * Build current row from SQLite statement.
 */
static OrmRow *
build_row_from_sqlite (OrmResult *self)
{
    GPtrArray *names;
    GPtrArray *values;
    int i;
    int col_count;

    col_count = sqlite3_column_count (self->sqlite_stmt);

    names = g_ptr_array_new_full (col_count, g_free);
    values = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_value_free);

    for (i = 0; i < col_count; i++)
    {
        const char *name = sqlite3_column_name (self->sqlite_stmt, i);
        g_ptr_array_add (names, g_strdup (name));
        g_ptr_array_add (values, sqlite_column_to_value (self->sqlite_stmt, i));
    }

    return orm_row_new (names, values);
}
#endif

#ifdef ORM_ENABLE_POSTGRES
/* Internal: Get PostgreSQL connection handle from connection */
extern PGconn * orm_connection_get_pg_conn (OrmConnection *self);

/*
 * Build current row from PostgreSQL result.
 */
static OrmRow *
build_row_from_postgres (OrmResult *self)
{
    GPtrArray *names;
    GPtrArray *values;
    int i;
    int col_count;

    col_count = PQnfields (self->pg_result);

    names = g_ptr_array_new_full (col_count, g_free);
    values = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_value_free);

    for (i = 0; i < col_count; i++)
    {
        const char *name = PQfname (self->pg_result, i);
        g_ptr_array_add (names, g_strdup (name));

        if (PQgetisnull (self->pg_result, self->pg_current_row, i))
        {
            g_ptr_array_add (values, orm_value_new_null ());
        }
        else
        {
            char *val = PQgetvalue (self->pg_result, self->pg_current_row, i);
            Oid type_oid = PQftype (self->pg_result, i);

            /*
             * PostgreSQL returns text values for most types.
             * We infer the type from the OID.
             * Common OIDs: 23 = int4, 20 = int8, 25 = text, 16 = bool,
             *              700 = float4, 701 = float8, 1114 = timestamp
             */
            switch (type_oid)
            {
            case 16:  /* bool */
                g_ptr_array_add (values,
                    orm_value_new_boolean (val[0] == 't' || val[0] == 'T'));
                break;

            case 21:  /* int2 */
            case 23:  /* int4 */
            case 20:  /* int8 */
            case 26:  /* oid */
                g_ptr_array_add (values,
                    orm_value_new_integer (g_ascii_strtoll (val, NULL, 10)));
                break;

            case 700: /* float4 */
            case 701: /* float8 */
            case 1700: /* numeric */
                g_ptr_array_add (values,
                    orm_value_new_float (g_ascii_strtod (val, NULL)));
                break;

            case 17:  /* bytea */
                {
                    /*
                     * PostgreSQL bytea in text mode uses escape format.
                     * For simplicity, store as string; proper handling
                     * would decode the bytea format.
                     */
                    g_ptr_array_add (values, orm_value_new_string (val));
                }
                break;

            default:
                /* Default to string for text, varchar, etc. */
                g_ptr_array_add (values, orm_value_new_string (val));
                break;
            }
        }
    }

    return orm_row_new (names, values);
}
#endif

#ifdef ORM_ENABLE_MYSQL
/* Internal: Get MySQL connection handle from connection */
extern MYSQL * orm_connection_get_mysql_conn (OrmConnection *self);

/*
 * Build current row from MySQL result.
 */
static OrmRow *
build_row_from_mysql (OrmResult *self)
{
    GPtrArray *names;
    GPtrArray *values;
    unsigned int i;
    unsigned int col_count;
    MYSQL_FIELD *fields;
    unsigned long *lengths;

    col_count = mysql_num_fields (self->mysql_result);
    fields = mysql_fetch_fields (self->mysql_result);
    lengths = mysql_fetch_lengths (self->mysql_result);

    names = g_ptr_array_new_full (col_count, g_free);
    values = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_value_free);

    for (i = 0; i < col_count; i++)
    {
        g_ptr_array_add (names, g_strdup (fields[i].name));

        if (self->mysql_current_row[i] == NULL)
        {
            g_ptr_array_add (values, orm_value_new_null ());
        }
        else
        {
            char *val = self->mysql_current_row[i];
            enum enum_field_types ftype = fields[i].type;

            switch (ftype)
            {
            case MYSQL_TYPE_TINY:
            case MYSQL_TYPE_SHORT:
            case MYSQL_TYPE_LONG:
            case MYSQL_TYPE_LONGLONG:
            case MYSQL_TYPE_INT24:
                g_ptr_array_add (values,
                    orm_value_new_integer (g_ascii_strtoll (val, NULL, 10)));
                break;

            case MYSQL_TYPE_FLOAT:
            case MYSQL_TYPE_DOUBLE:
            case MYSQL_TYPE_DECIMAL:
            case MYSQL_TYPE_NEWDECIMAL:
                g_ptr_array_add (values,
                    orm_value_new_float (g_ascii_strtod (val, NULL)));
                break;

            case MYSQL_TYPE_BLOB:
            case MYSQL_TYPE_TINY_BLOB:
            case MYSQL_TYPE_MEDIUM_BLOB:
            case MYSQL_TYPE_LONG_BLOB:
                {
                    GBytes *bytes = g_bytes_new (val, lengths[i]);
                    g_ptr_array_add (values, orm_value_new_blob (bytes));
                    g_bytes_unref (bytes);
                }
                break;

            default:
                /* Default to string for VARCHAR, TEXT, etc. */
                g_ptr_array_add (values, orm_value_new_string (val));
                break;
            }
        }
    }

    return orm_row_new (names, values);
}
#endif

static void
orm_result_finalize (GObject *object)
{
    OrmResult *self = ORM_RESULT (object);

    orm_result_close (self);
    g_clear_pointer (&self->column_names, g_ptr_array_unref);
    g_clear_object (&self->current_row);

    G_OBJECT_CLASS (orm_result_parent_class)->finalize (object);
}

static void
orm_result_class_init (OrmResultClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_result_finalize;
}

static void
orm_result_init (OrmResult *self)
{
    self->connection = NULL;
    self->column_names = NULL;
    self->current_row = NULL;
    self->is_closed = FALSE;
    self->has_more = TRUE;
    self->dialect_type = ORM_DIALECT_SQLITE;

#ifdef ORM_ENABLE_SQLITE
    self->sqlite_stmt = NULL;
#endif

#ifdef ORM_ENABLE_POSTGRES
    self->pg_result = NULL;
    self->pg_row_count = 0;
    self->pg_current_row = -1;
#endif

#ifdef ORM_ENABLE_MYSQL
    self->mysql_result = NULL;
    self->mysql_current_row = NULL;
#endif
}

#ifdef ORM_ENABLE_SQLITE
/**
 * orm_result_new_sqlite:
 * @connection: The connection
 * @sql: SQL query
 * @params: (element-type OrmValue) (nullable): Parameters
 * @error: Return location for error
 *
 * Creates a new result from a SQLite query.
 *
 * Returns: (transfer full) (nullable): A new #OrmResult
 */
OrmResult *
orm_result_new_sqlite (OrmConnection  *connection,
                       const gchar    *sql,
                       GList          *params,
                       GError        **error)
{
    OrmResult *self;
    sqlite3 *db;
    int rc;
    int col_count;
    int i;
    int param_index;
    GList *l;

    g_return_val_if_fail (ORM_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (sql != NULL, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    db = orm_connection_get_sqlite_db (connection);
    g_return_val_if_fail (db != NULL, NULL);

    self = g_object_new (ORM_TYPE_RESULT, NULL);
    self->connection = connection;

    /* Prepare statement */
    rc = sqlite3_prepare_v2 (db, sql, -1, &self->sqlite_stmt, NULL);
    if (rc != SQLITE_OK)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_PREPARE,
                     "Failed to prepare statement: %s",
                     sqlite3_errmsg (db));
        g_object_unref (self);
        return NULL;
    }

    /* Bind parameters */
    param_index = 1;
    for (l = params; l != NULL; l = l->next, param_index++)
    {
        OrmValue *value = (OrmValue *) l->data;
        OrmValueType vtype;

        if (value == NULL)
        {
            rc = sqlite3_bind_null (self->sqlite_stmt, param_index);
        }
        else
        {
            vtype = orm_value_get_value_type (value);

            switch (vtype)
            {
            case ORM_VALUE_NULL:
                rc = sqlite3_bind_null (self->sqlite_stmt, param_index);
                break;
            case ORM_VALUE_INTEGER:
                rc = sqlite3_bind_int64 (self->sqlite_stmt, param_index,
                                         orm_value_get_integer (value));
                break;
            case ORM_VALUE_FLOAT:
                rc = sqlite3_bind_double (self->sqlite_stmt, param_index,
                                          orm_value_get_float (value));
                break;
            case ORM_VALUE_STRING:
                rc = sqlite3_bind_text (self->sqlite_stmt, param_index,
                                        orm_value_get_string (value),
                                        -1, SQLITE_TRANSIENT);
                break;
            case ORM_VALUE_BOOLEAN:
                rc = sqlite3_bind_int (self->sqlite_stmt, param_index,
                                       orm_value_get_boolean (value) ? 1 : 0);
                break;
            case ORM_VALUE_DATETIME:
                {
                    GDateTime *dt = orm_value_get_datetime (value);
                    g_autofree gchar *iso = g_date_time_format_iso8601 (dt);
                    rc = sqlite3_bind_text (self->sqlite_stmt, param_index,
                                            iso, -1, SQLITE_TRANSIENT);
                }
                break;
            case ORM_VALUE_BLOB:
                {
                    GBytes *bytes = orm_value_get_blob (value);
                    gsize size;
                    gconstpointer data = g_bytes_get_data (bytes, &size);
                    rc = sqlite3_bind_blob (self->sqlite_stmt, param_index,
                                            data, (int) size, SQLITE_TRANSIENT);
                }
                break;
            default:
                rc = sqlite3_bind_null (self->sqlite_stmt, param_index);
                break;
            }
        }

        if (rc != SQLITE_OK)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_BIND,
                         "Failed to bind parameter %d", param_index);
            sqlite3_finalize (self->sqlite_stmt);
            self->sqlite_stmt = NULL;
            g_object_unref (self);
            return NULL;
        }
    }

    /* Get column names */
    col_count = sqlite3_column_count (self->sqlite_stmt);
    self->column_names = g_ptr_array_new_full (col_count, g_free);

    for (i = 0; i < col_count; i++)
    {
        const char *name = sqlite3_column_name (self->sqlite_stmt, i);
        g_ptr_array_add (self->column_names, g_strdup (name));
    }

    return self;
}
#endif

#ifdef ORM_ENABLE_POSTGRES
/*
 * Convert OrmValue to string for PostgreSQL parameter binding.
 */
static gchar *
orm_value_to_postgres_string (OrmValue *value)
{
    OrmValueType vtype;

    if (value == NULL)
    {
        return NULL;
    }

    vtype = orm_value_get_value_type (value);

    switch (vtype)
    {
    case ORM_VALUE_NULL:
        return NULL;

    case ORM_VALUE_INTEGER:
        return g_strdup_printf ("%" G_GINT64_FORMAT,
                                orm_value_get_integer (value));

    case ORM_VALUE_FLOAT:
        return g_strdup_printf ("%g", orm_value_get_float (value));

    case ORM_VALUE_STRING:
        return g_strdup (orm_value_get_string (value));

    case ORM_VALUE_BOOLEAN:
        return g_strdup (orm_value_get_boolean (value) ? "true" : "false");

    case ORM_VALUE_DATETIME:
        {
            GDateTime *dt = orm_value_get_datetime (value);
            return g_date_time_format_iso8601 (dt);
        }

    case ORM_VALUE_BLOB:
        {
            /* For PostgreSQL BYTEA, we use hex encoding */
            GBytes *bytes = orm_value_get_blob (value);
            gsize size;
            const guchar *data;
            GString *hex;
            gsize i;

            data = g_bytes_get_data (bytes, &size);
            hex = g_string_sized_new (size * 2 + 3);
            g_string_append (hex, "\\x");

            for (i = 0; i < size; i++)
            {
                g_string_append_printf (hex, "%02x", data[i]);
            }

            return g_string_free (hex, FALSE);
        }

    default:
        return NULL;
    }
}

/**
 * orm_result_new_postgres:
 * @connection: The connection
 * @sql: SQL query
 * @params: (element-type OrmValue) (nullable): Parameters
 * @error: Return location for error
 *
 * Creates a new result from a PostgreSQL query.
 *
 * Returns: (transfer full) (nullable): A new #OrmResult
 */
OrmResult *
orm_result_new_postgres (OrmConnection  *connection,
                         const gchar    *sql,
                         GList          *params,
                         GError        **error)
{
    OrmResult *self;
    PGconn *pg_conn;
    int col_count;
    int i;
    int n_params;
    const char **param_values = NULL;
    gchar **param_strings = NULL;
    GList *l;

    g_return_val_if_fail (ORM_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (sql != NULL, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    pg_conn = orm_connection_get_pg_conn (connection);
    g_return_val_if_fail (pg_conn != NULL, NULL);

    self = g_object_new (ORM_TYPE_RESULT, NULL);
    self->connection = connection;
    self->dialect_type = ORM_DIALECT_POSTGRES;

    /* Build parameter arrays */
    n_params = g_list_length (params);

    if (n_params > 0)
    {
        param_values = g_new0 (const char *, n_params);
        param_strings = g_new0 (gchar *, n_params);

        i = 0;
        for (l = params; l != NULL; l = l->next, i++)
        {
            OrmValue *value = (OrmValue *) l->data;
            param_strings[i] = orm_value_to_postgres_string (value);
            param_values[i] = param_strings[i];
        }
    }

    /* Execute parameterized query */
    self->pg_result = PQexecParams (pg_conn,
                                    sql,
                                    n_params,
                                    NULL,        /* paramTypes */
                                    param_values,
                                    NULL,        /* paramLengths */
                                    NULL,        /* paramFormats */
                                    0);          /* resultFormat */

    /* Free parameter strings */
    if (param_strings != NULL)
    {
        for (i = 0; i < n_params; i++)
        {
            g_free (param_strings[i]);
        }
        g_free (param_strings);
        g_free (param_values);
    }

    if (PQresultStatus (self->pg_result) != PGRES_TUPLES_OK &&
        PQresultStatus (self->pg_result) != PGRES_COMMAND_OK)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                     "Query failed: %s",
                     PQerrorMessage (pg_conn));
        PQclear (self->pg_result);
        self->pg_result = NULL;
        g_object_unref (self);
        return NULL;
    }

    /* Get row count and column names */
    self->pg_row_count = PQntuples (self->pg_result);
    self->pg_current_row = -1;

    col_count = PQnfields (self->pg_result);
    self->column_names = g_ptr_array_new_full (col_count, g_free);

    for (i = 0; i < col_count; i++)
    {
        const char *name = PQfname (self->pg_result, i);
        g_ptr_array_add (self->column_names, g_strdup (name));
    }

    return self;
}
#endif

#ifdef ORM_ENABLE_MYSQL
/**
 * orm_result_new_mysql:
 * @connection: The connection
 * @sql: SQL query
 * @params: (element-type OrmValue) (nullable): Parameters
 * @error: Return location for error
 *
 * Creates a new result from a MySQL query.
 *
 * Returns: (transfer full) (nullable): A new #OrmResult
 */
OrmResult *
orm_result_new_mysql (OrmConnection  *connection,
                      const gchar    *sql,
                      GList          *params,
                      GError        **error)
{
    OrmResult *self;
    MYSQL *mysql_conn;
    unsigned int col_count;
    unsigned int i;
    MYSQL_FIELD *fields;

    g_return_val_if_fail (ORM_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (sql != NULL, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    mysql_conn = orm_connection_get_mysql_conn (connection);
    g_return_val_if_fail (mysql_conn != NULL, NULL);

    self = g_object_new (ORM_TYPE_RESULT, NULL);
    self->connection = connection;
    self->dialect_type = ORM_DIALECT_MYSQL;

    /*
     * For simplicity, we use mysql_query for queries without parameters.
     * For parameterized queries, we'd need to use prepared statements.
     * Here we handle the simple case; params should be empty or NULL.
     */
    if (params != NULL && g_list_length (params) > 0)
    {
        /*
         * MySQL requires prepared statements for parameterized queries.
         * For now, we only support non-parameterized queries here.
         */
        MYSQL_STMT *stmt = NULL;
        MYSQL_BIND *binds = NULL;
        gpointer *buffers = NULL;
        int n_params;
        GList *l;
        int idx;
        gboolean success = TRUE;

        stmt = mysql_stmt_init (mysql_conn);
        if (stmt == NULL)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_PREPARE,
                         "Failed to initialize statement");
            g_object_unref (self);
            return NULL;
        }

        if (mysql_stmt_prepare (stmt, sql, strlen (sql)) != 0)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_PREPARE,
                         "Failed to prepare statement: %s",
                         mysql_stmt_error (stmt));
            mysql_stmt_close (stmt);
            g_object_unref (self);
            return NULL;
        }

        n_params = g_list_length (params);
        binds = g_new0 (MYSQL_BIND, n_params);
        buffers = g_new0 (gpointer, n_params);

        idx = 0;
        for (l = params; l != NULL; l = l->next, idx++)
        {
            OrmValue *value = (OrmValue *) l->data;
            OrmValueType vtype;

            memset (&binds[idx], 0, sizeof (MYSQL_BIND));

            if (value == NULL)
            {
                binds[idx].buffer_type = MYSQL_TYPE_NULL;
                buffers[idx] = NULL;
            }
            else
            {
                vtype = orm_value_get_value_type (value);

                switch (vtype)
                {
                case ORM_VALUE_NULL:
                    binds[idx].buffer_type = MYSQL_TYPE_NULL;
                    buffers[idx] = NULL;
                    break;

                case ORM_VALUE_INTEGER:
                    {
                        gint64 *val = g_new (gint64, 1);
                        *val = orm_value_get_integer (value);
                        binds[idx].buffer_type = MYSQL_TYPE_LONGLONG;
                        binds[idx].buffer = val;
                        binds[idx].is_unsigned = FALSE;
                        buffers[idx] = val;
                    }
                    break;

                case ORM_VALUE_FLOAT:
                    {
                        gdouble *val = g_new (gdouble, 1);
                        *val = orm_value_get_float (value);
                        binds[idx].buffer_type = MYSQL_TYPE_DOUBLE;
                        binds[idx].buffer = val;
                        buffers[idx] = val;
                    }
                    break;

                case ORM_VALUE_STRING:
                    {
                        const gchar *str = orm_value_get_string (value);
                        gsize len = strlen (str);
                        gchar *dup = g_strdup (str);
                        binds[idx].buffer_type = MYSQL_TYPE_STRING;
                        binds[idx].buffer = dup;
                        binds[idx].buffer_length = len;
                        buffers[idx] = dup;
                    }
                    break;

                case ORM_VALUE_BOOLEAN:
                    {
                        gint8 *val = g_new (gint8, 1);
                        *val = orm_value_get_boolean (value) ? 1 : 0;
                        binds[idx].buffer_type = MYSQL_TYPE_TINY;
                        binds[idx].buffer = val;
                        buffers[idx] = val;
                    }
                    break;

                default:
                    binds[idx].buffer_type = MYSQL_TYPE_NULL;
                    buffers[idx] = NULL;
                    break;
                }
            }
        }

        if (mysql_stmt_bind_param (stmt, binds) != 0)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_BIND,
                         "Failed to bind parameters: %s",
                         mysql_stmt_error (stmt));
            success = FALSE;
        }

        if (success && mysql_stmt_execute (stmt) != 0)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                         "Failed to execute: %s",
                         mysql_stmt_error (stmt));
            success = FALSE;
        }

        if (success)
        {
            /* Store result for iteration */
            self->mysql_result = mysql_stmt_result_metadata (stmt);
            mysql_stmt_store_result (stmt);
        }

        /* Cleanup binds */
        for (idx = 0; idx < n_params; idx++)
        {
            g_free (buffers[idx]);
        }
        g_free (buffers);
        g_free (binds);

        /*
         * Note: For prepared statement results, we'd need to handle
         * mysql_stmt_fetch instead of mysql_fetch_row. For simplicity,
         * we close the stmt and fall back to simple query for SELECT.
         */
        mysql_stmt_close (stmt);

        if (!success)
        {
            g_object_unref (self);
            return NULL;
        }
    }
    else
    {
        /* Simple query without parameters */
        if (mysql_query (mysql_conn, sql) != 0)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                         "Query failed: %s",
                         mysql_error (mysql_conn));
            g_object_unref (self);
            return NULL;
        }

        self->mysql_result = mysql_store_result (mysql_conn);
    }

    if (self->mysql_result == NULL)
    {
        /*
         * No result set - this is OK for INSERT/UPDATE/DELETE.
         * Create empty column list.
         */
        self->column_names = g_ptr_array_new_full (0, g_free);
        self->has_more = FALSE;
        return self;
    }

    /* Get column names */
    col_count = mysql_num_fields (self->mysql_result);
    fields = mysql_fetch_fields (self->mysql_result);

    self->column_names = g_ptr_array_new_full (col_count, g_free);

    for (i = 0; i < col_count; i++)
    {
        g_ptr_array_add (self->column_names, g_strdup (fields[i].name));
    }

    return self;
}
#endif

/**
 * orm_result_next:
 * @self: An #OrmResult
 *
 * Advances to the next row.
 *
 * Returns: %TRUE if there is another row
 */
gboolean
orm_result_next (OrmResult *self)
{
    g_return_val_if_fail (ORM_IS_RESULT (self), FALSE);
    g_return_val_if_fail (!self->is_closed, FALSE);

    /* Clear previous row */
    g_clear_object (&self->current_row);

    if (!self->has_more)
    {
        return FALSE;
    }

#ifdef ORM_ENABLE_SQLITE
    if (self->dialect_type == ORM_DIALECT_SQLITE && self->sqlite_stmt != NULL)
    {
        int rc = sqlite3_step (self->sqlite_stmt);

        if (rc == SQLITE_ROW)
        {
            self->current_row = build_row_from_sqlite (self);
            return TRUE;
        }
        else if (rc == SQLITE_DONE)
        {
            self->has_more = FALSE;
            return FALSE;
        }
        else
        {
            g_warning ("SQLite step error: %d", rc);
            self->has_more = FALSE;
            return FALSE;
        }
    }
#endif

#ifdef ORM_ENABLE_POSTGRES
    if (self->dialect_type == ORM_DIALECT_POSTGRES && self->pg_result != NULL)
    {
        self->pg_current_row++;

        if (self->pg_current_row < self->pg_row_count)
        {
            self->current_row = build_row_from_postgres (self);
            return TRUE;
        }
        else
        {
            self->has_more = FALSE;
            return FALSE;
        }
    }
#endif

#ifdef ORM_ENABLE_MYSQL
    if (self->dialect_type == ORM_DIALECT_MYSQL && self->mysql_result != NULL)
    {
        self->mysql_current_row = mysql_fetch_row (self->mysql_result);

        if (self->mysql_current_row != NULL)
        {
            self->current_row = build_row_from_mysql (self);
            return TRUE;
        }
        else
        {
            self->has_more = FALSE;
            return FALSE;
        }
    }
#endif

    return FALSE;
}

/**
 * orm_result_get_row:
 * @self: An #OrmResult
 *
 * Gets the current row.
 *
 * Returns: (transfer none) (nullable): The current row
 */
OrmRow *
orm_result_get_row (OrmResult *self)
{
    g_return_val_if_fail (ORM_IS_RESULT (self), NULL);
    return self->current_row;
}

/**
 * orm_result_get_column_count:
 * @self: An #OrmResult
 *
 * Gets the number of columns.
 *
 * Returns: Number of columns
 */
gint
orm_result_get_column_count (OrmResult *self)
{
    g_return_val_if_fail (ORM_IS_RESULT (self), 0);
    return (gint) self->column_names->len;
}

/**
 * orm_result_get_column_name:
 * @self: An #OrmResult
 * @index: Column index
 *
 * Gets a column name.
 *
 * Returns: (transfer none) (nullable): Column name
 */
const gchar *
orm_result_get_column_name (OrmResult *self,
                            gint       index)
{
    g_return_val_if_fail (ORM_IS_RESULT (self), NULL);
    g_return_val_if_fail (index >= 0 && (guint) index < self->column_names->len, NULL);

    return g_ptr_array_index (self->column_names, index);
}

/**
 * orm_result_get_all:
 * @self: An #OrmResult
 *
 * Fetches all remaining rows.
 *
 * Returns: (transfer full) (element-type OrmRow): List of rows
 */
GList *
orm_result_get_all (OrmResult *self)
{
    GList *rows = NULL;

    g_return_val_if_fail (ORM_IS_RESULT (self), NULL);

    while (orm_result_next (self))
    {
        rows = g_list_append (rows, g_object_ref (self->current_row));
    }

    return rows;
}

/**
 * orm_result_get_first:
 * @self: An #OrmResult
 *
 * Gets the first row.
 *
 * Returns: (transfer full) (nullable): The first row
 */
OrmRow *
orm_result_get_first (OrmResult *self)
{
    g_return_val_if_fail (ORM_IS_RESULT (self), NULL);

    if (orm_result_next (self))
    {
        return g_object_ref (self->current_row);
    }

    return NULL;
}

/**
 * orm_result_get_scalar:
 * @self: An #OrmResult
 *
 * Gets the first column of the first row.
 *
 * Returns: (transfer full) (nullable): The scalar value
 */
OrmValue *
orm_result_get_scalar (OrmResult *self)
{
    OrmValue *value;

    g_return_val_if_fail (ORM_IS_RESULT (self), NULL);

    if (!orm_result_next (self))
    {
        return NULL;
    }

    if (self->current_row == NULL)
    {
        return NULL;
    }

    value = orm_row_get_value (self->current_row, 0);
    if (value == NULL)
    {
        return NULL;
    }

    return orm_value_copy (value);
}

/**
 * orm_result_close:
 * @self: An #OrmResult
 *
 * Closes the result set.
 */
void
orm_result_close (OrmResult *self)
{
    g_return_if_fail (ORM_IS_RESULT (self));

    if (self->is_closed)
    {
        return;
    }

#ifdef ORM_ENABLE_SQLITE
    if (self->sqlite_stmt != NULL)
    {
        sqlite3_finalize (self->sqlite_stmt);
        self->sqlite_stmt = NULL;
    }
#endif

#ifdef ORM_ENABLE_POSTGRES
    if (self->pg_result != NULL)
    {
        PQclear (self->pg_result);
        self->pg_result = NULL;
    }
#endif

#ifdef ORM_ENABLE_MYSQL
    if (self->mysql_result != NULL)
    {
        mysql_free_result (self->mysql_result);
        self->mysql_result = NULL;
    }
#endif

    self->is_closed = TRUE;
    self->has_more = FALSE;
}
