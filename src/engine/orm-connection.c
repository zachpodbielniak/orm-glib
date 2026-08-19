/* orm-connection.c
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

#include "orm-connection.h"
#include "orm-engine.h"
#include "orm-result.h"
#include "orm-transaction.h"
#include "../core/orm-error.h"

#include <string.h>

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
 * OrmConnection - Database connection wrapper.
 *
 * Wraps the underlying database connection and provides
 * a unified interface for executing SQL.
 */

struct _OrmConnection
{
    GObject parent_instance;

    OrmEngine         *engine;       /* Weak reference */
    OrmDialectType     dialect_type;
    gboolean           is_open;
    gboolean           in_transaction;
    OrmIsolationLevel  isolation_level;  /* Cached; see orm_connection_set_isolation_level */

#ifdef ORM_ENABLE_SQLITE
    sqlite3        *sqlite_db;
#endif

#ifdef ORM_ENABLE_POSTGRES
    PGconn         *pg_conn;
#endif

#ifdef ORM_ENABLE_MYSQL
    MYSQL          *mysql_conn;
#endif
};

G_DEFINE_TYPE (OrmConnection, orm_connection, G_TYPE_OBJECT)

/* Internal functions to get connection parameters from engine */
extern const gchar * orm_engine_get_database_path (OrmEngine *engine);
extern const gchar * orm_engine_get_host (OrmEngine *engine);
extern gint orm_engine_get_port (OrmEngine *engine);
extern const gchar * orm_engine_get_username (OrmEngine *engine);
extern const gchar * orm_engine_get_password (OrmEngine *engine);
extern const gchar * orm_engine_get_database (OrmEngine *engine);

#ifdef ORM_ENABLE_SQLITE
/*
 * Bind OrmValue to SQLite statement parameter.
 */
static gboolean
bind_value_to_sqlite (sqlite3_stmt *stmt,
                      int           index,
                      OrmValue     *value,
                      GError      **error)
{
    int rc;
    OrmValueType vtype;

    if (value == NULL)
    {
        rc = sqlite3_bind_null (stmt, index);
    }
    else
    {
        vtype = orm_value_get_value_type (value);

        switch (vtype)
        {
        case ORM_VALUE_NULL:
            rc = sqlite3_bind_null (stmt, index);
            break;

        case ORM_VALUE_INTEGER:
            rc = sqlite3_bind_int64 (stmt, index,
                                     orm_value_get_integer (value));
            break;

        case ORM_VALUE_FLOAT:
            rc = sqlite3_bind_double (stmt, index,
                                      orm_value_get_float (value));
            break;

        case ORM_VALUE_STRING:
            rc = sqlite3_bind_text (stmt, index,
                                    orm_value_get_string (value),
                                    -1, SQLITE_TRANSIENT);
            break;

        case ORM_VALUE_BOOLEAN:
            rc = sqlite3_bind_int (stmt, index,
                                   orm_value_get_boolean (value) ? 1 : 0);
            break;

        case ORM_VALUE_DATETIME:
            {
                GDateTime *dt = orm_value_get_datetime (value);
                g_autofree gchar *iso = g_date_time_format_iso8601 (dt);
                rc = sqlite3_bind_text (stmt, index, iso, -1, SQLITE_TRANSIENT);
            }
            break;

        case ORM_VALUE_BLOB:
            {
                GBytes *bytes = orm_value_get_blob (value);
                gsize size;
                gconstpointer data = g_bytes_get_data (bytes, &size);
                rc = sqlite3_bind_blob (stmt, index, data, (int) size,
                                        SQLITE_TRANSIENT);
            }
            break;

        default:
            rc = sqlite3_bind_null (stmt, index);
            break;
        }
    }

    if (rc != SQLITE_OK)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_BIND,
                     "Failed to bind parameter %d", index);
        return FALSE;
    }

    return TRUE;
}
#endif

#ifdef ORM_ENABLE_POSTGRES
/*
 * Convert OrmValue to string for PostgreSQL parameter binding.
 * PostgreSQL uses text-based parameter binding in simple queries.
 *
 * Returns: (transfer full): A string representation, or NULL for NULL value
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
#endif

#ifdef ORM_ENABLE_MYSQL
/*
 * Bind OrmValue to MySQL prepared statement parameter.
 */
static gboolean
bind_value_to_mysql (MYSQL_BIND  *bind,
                     OrmValue    *value,
                     gpointer    *buffer,
                     GError     **error)
{
    OrmValueType vtype;

    memset (bind, 0, sizeof (MYSQL_BIND));

    if (value == NULL)
    {
        bind->buffer_type = MYSQL_TYPE_NULL;
        *buffer = NULL;
        return TRUE;
    }

    vtype = orm_value_get_value_type (value);

    switch (vtype)
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
            const gchar *str = orm_value_get_string (value);
            gsize len = strlen (str);
            gchar *dup = g_strdup (str);
            bind->buffer_type = MYSQL_TYPE_STRING;
            bind->buffer = dup;
            bind->buffer_length = len;
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
            GDateTime *dt = orm_value_get_datetime (value);
            g_autofree gchar *iso = g_date_time_format_iso8601 (dt);
            gchar *dup = g_strdup (iso);
            bind->buffer_type = MYSQL_TYPE_STRING;
            bind->buffer = dup;
            bind->buffer_length = strlen (dup);
            *buffer = dup;
        }
        break;

    case ORM_VALUE_BLOB:
        {
            GBytes *bytes = orm_value_get_blob (value);
            gsize size;
            gconstpointer data = g_bytes_get_data (bytes, &size);
            gchar *dup = g_memdup2 (data, size);
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
#endif

static void
orm_connection_finalize (GObject *object)
{
    OrmConnection *self = ORM_CONNECTION (object);

    orm_connection_close (self);

    G_OBJECT_CLASS (orm_connection_parent_class)->finalize (object);
}

static void
orm_connection_class_init (OrmConnectionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orm_connection_finalize;
}

static void
orm_connection_init (OrmConnection *self)
{
    self->engine = NULL;
    self->is_open = FALSE;
    self->in_transaction = FALSE;
    self->isolation_level = ORM_ISOLATION_SERIALIZABLE;

#ifdef ORM_ENABLE_SQLITE
    self->sqlite_db = NULL;
#endif

#ifdef ORM_ENABLE_POSTGRES
    self->pg_conn = NULL;
#endif

#ifdef ORM_ENABLE_MYSQL
    self->mysql_conn = NULL;
#endif
}

/**
 * orm_connection_new:
 * @engine: The engine that owns this connection
 * @error: Return location for error
 *
 * Creates a new database connection.
 *
 * Returns: (transfer full) (nullable): A new #OrmConnection, or %NULL on error
 */

#ifdef ORM_ENABLE_POSTGRES

/*
 * Quotes a value for a libpq keyword/value connection string.
 *
 * libpq splits that string on whitespace, so a password containing a space
 * silently becomes a truncated password plus a garbage keyword. Wrapping in
 * single quotes and backslash-escaping quotes and backslashes is the
 * encoding libpq documents for exactly this.
 *
 * Returns: (transfer full): the quoted value
 */
static gchar *
orm_conninfo_quote (const gchar *value)
{
    GString     *quoted;
    const gchar *c;

    quoted = g_string_new ("'");

    for (c = value; c != NULL && *c != '\0'; c++)
    {
        if (*c == '\'' || *c == '\\')
            g_string_append_c (quoted, '\\');

        g_string_append_c (quoted, *c);
    }

    g_string_append_c (quoted, '\'');

    return g_string_free (quoted, FALSE);
}

#endif /* ORM_ENABLE_POSTGRES */

OrmConnection *
orm_connection_new (OrmEngine  *engine,
                    GError    **error)
{
    OrmConnection *self;
    OrmDialectType dtype;

    g_return_val_if_fail (ORM_IS_ENGINE (engine), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    self = g_object_new (ORM_TYPE_CONNECTION, NULL);
    self->engine = engine;  /* Weak reference */
    self->dialect_type = orm_engine_get_dialect_type (engine);

    /* Each backend opens at its own documented default, not a common one. */
    switch (self->dialect_type)
    {
    case ORM_DIALECT_POSTGRES:
        self->isolation_level = ORM_ISOLATION_READ_COMMITTED;
        break;
    case ORM_DIALECT_MYSQL:
        self->isolation_level = ORM_ISOLATION_REPEATABLE_READ;
        break;
    default:
        self->isolation_level = ORM_ISOLATION_SERIALIZABLE;
        break;
    }

    dtype = self->dialect_type;

    switch (dtype)
    {
#ifdef ORM_ENABLE_SQLITE
    case ORM_DIALECT_SQLITE:
        {
            const gchar *path;
            int rc;

            path = orm_engine_get_database_path (engine);
            rc = sqlite3_open (path, &self->sqlite_db);

            if (rc != SQLITE_OK)
            {
                g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                             "Failed to open SQLite database: %s",
                             sqlite3_errmsg (self->sqlite_db));
                sqlite3_close (self->sqlite_db);
                self->sqlite_db = NULL;
                g_object_unref (self);
                return NULL;
            }

            /* Enable foreign keys */
            sqlite3_exec (self->sqlite_db, "PRAGMA foreign_keys = ON",
                          NULL, NULL, NULL);

            self->is_open = TRUE;
        }
        break;
#endif

#ifdef ORM_ENABLE_POSTGRES
    case ORM_DIALECT_POSTGRES:
        {
            const gchar *host;
            gint port;
            const gchar *user;
            const gchar *pass;
            const gchar *dbname;
            g_autofree gchar *conninfo = NULL;

            host = orm_engine_get_host (engine);
            port = orm_engine_get_port (engine);
            user = orm_engine_get_username (engine);
            pass = orm_engine_get_password (engine);
            dbname = orm_engine_get_database (engine);

            /*
             * Build the connection string. Every value is quoted: libpq
             * splits this on whitespace, so an unquoted password with a
             * space in it becomes a truncated password and a stray
             * keyword, and the resulting error says nothing useful.
             */
            {
                g_autofree gchar *q_host = orm_conninfo_quote (host);
                g_autofree gchar *q_dbname = orm_conninfo_quote (dbname);
                g_autofree gchar *q_user = NULL;
                g_autofree gchar *q_pass = NULL;

                if (pass != NULL)
                {
                    q_user = orm_conninfo_quote (user);
                    q_pass = orm_conninfo_quote (pass);
                    conninfo = g_strdup_printf (
                        "host=%s port=%d dbname=%s user=%s password=%s",
                        q_host, port, q_dbname, q_user, q_pass);
                }
                else if (user != NULL)
                {
                    q_user = orm_conninfo_quote (user);
                    conninfo = g_strdup_printf (
                        "host=%s port=%d dbname=%s user=%s",
                        q_host, port, q_dbname, q_user);
                }
                else
                {
                    conninfo = g_strdup_printf (
                        "host=%s port=%d dbname=%s",
                        q_host, port, q_dbname);
                }
            }

            self->pg_conn = PQconnectdb (conninfo);

            if (PQstatus (self->pg_conn) != CONNECTION_OK)
            {
                g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                             "Failed to connect to PostgreSQL: %s",
                             PQerrorMessage (self->pg_conn));
                PQfinish (self->pg_conn);
                self->pg_conn = NULL;
                g_object_unref (self);
                return NULL;
            }

            self->is_open = TRUE;
        }
        break;
#endif

#ifdef ORM_ENABLE_MYSQL
    case ORM_DIALECT_MYSQL:
        {
            const gchar *host;
            gint port;
            const gchar *user;
            const gchar *pass;
            const gchar *dbname;

            host = orm_engine_get_host (engine);
            port = orm_engine_get_port (engine);
            user = orm_engine_get_username (engine);
            pass = orm_engine_get_password (engine);
            dbname = orm_engine_get_database (engine);

            self->mysql_conn = mysql_init (NULL);
            if (self->mysql_conn == NULL)
            {
                g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                             "Failed to initialize MySQL client");
                g_object_unref (self);
                return NULL;
            }

            if (mysql_real_connect (self->mysql_conn,
                                    host,
                                    user,
                                    pass,
                                    dbname,
                                    (unsigned int) port,
                                    NULL,  /* Unix socket */
                                    0)     /* Client flags */
                == NULL)
            {
                g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                             "Failed to connect to MySQL: %s",
                             mysql_error (self->mysql_conn));
                mysql_close (self->mysql_conn);
                self->mysql_conn = NULL;
                g_object_unref (self);
                return NULL;
            }

            /* Set character set to UTF-8 */
            mysql_set_character_set (self->mysql_conn, "utf8mb4");

            self->is_open = TRUE;
        }
        break;
#endif

    default:
        g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_IMPLEMENTED,
                     "Dialect %d not yet implemented", dtype);
        g_object_unref (self);
        return NULL;
    }

    return self;
}

/**
 * orm_connection_close:
 * @self: An #OrmConnection
 *
 * Closes the connection.
 */
void
orm_connection_close (OrmConnection *self)
{
    g_return_if_fail (ORM_IS_CONNECTION (self));

    if (!self->is_open)
    {
        return;
    }

#ifdef ORM_ENABLE_SQLITE
    if (self->sqlite_db != NULL)
    {
        sqlite3_close (self->sqlite_db);
        self->sqlite_db = NULL;
    }
#endif

#ifdef ORM_ENABLE_POSTGRES
    if (self->pg_conn != NULL)
    {
        PQfinish (self->pg_conn);
        self->pg_conn = NULL;
    }
#endif

#ifdef ORM_ENABLE_MYSQL
    if (self->mysql_conn != NULL)
    {
        mysql_close (self->mysql_conn);
        self->mysql_conn = NULL;
    }
#endif

    self->is_open = FALSE;
}

/**
 * orm_connection_is_open:
 * @self: An #OrmConnection
 *
 * Checks if the connection is open.
 *
 * Returns: %TRUE if open
 */
gboolean
orm_connection_is_open (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    return self->is_open;
}

/**
 * orm_connection_execute:
 * @self: An #OrmConnection
 * @sql: SQL statement to execute
 * @error: Return location for error
 *
 * Executes a SQL statement that doesn't return results.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_connection_execute (OrmConnection  *self,
                        const gchar    *sql,
                        GError        **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    g_return_val_if_fail (sql != NULL, FALSE);
    g_return_val_if_fail (self->is_open, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

#ifdef ORM_ENABLE_SQLITE
    if (self->dialect_type == ORM_DIALECT_SQLITE)
    {
        char *errmsg = NULL;
        int rc;

        rc = sqlite3_exec (self->sqlite_db, sql, NULL, NULL, &errmsg);

        if (rc != SQLITE_OK)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                         "SQL error: %s", errmsg ? errmsg : "unknown");
            sqlite3_free (errmsg);
            return FALSE;
        }

        return TRUE;
    }
#endif

#ifdef ORM_ENABLE_POSTGRES
    if (self->dialect_type == ORM_DIALECT_POSTGRES)
    {
        PGresult *res;

        res = PQexec (self->pg_conn, sql);

        if (PQresultStatus (res) != PGRES_COMMAND_OK &&
            PQresultStatus (res) != PGRES_TUPLES_OK)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                         "SQL error: %s", PQerrorMessage (self->pg_conn));
            PQclear (res);
            return FALSE;
        }

        PQclear (res);
        return TRUE;
    }
#endif

#ifdef ORM_ENABLE_MYSQL
    if (self->dialect_type == ORM_DIALECT_MYSQL)
    {
        if (mysql_query (self->mysql_conn, sql) != 0)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                         "SQL error: %s", mysql_error (self->mysql_conn));
            return FALSE;
        }

        return TRUE;
    }
#endif

    g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_IMPLEMENTED,
                 "Execute not implemented for dialect");
    return FALSE;
}

/**
 * orm_connection_execute_with_params:
 * @self: An #OrmConnection
 * @sql: SQL statement with ? placeholders
 * @params: (element-type OrmValue): Parameter values
 * @error: Return location for error
 *
 * Executes a parameterized SQL statement.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_connection_execute_with_params (OrmConnection  *self,
                                    const gchar    *sql,
                                    GList          *params,
                                    GError        **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    g_return_val_if_fail (sql != NULL, FALSE);
    g_return_val_if_fail (self->is_open, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

#ifdef ORM_ENABLE_SQLITE
    if (self->dialect_type == ORM_DIALECT_SQLITE)
    {
        sqlite3_stmt *stmt = NULL;
        int rc;
        int index;
        GList *l;

        rc = sqlite3_prepare_v2 (self->sqlite_db, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_PREPARE,
                         "Failed to prepare statement: %s",
                         sqlite3_errmsg (self->sqlite_db));
            return FALSE;
        }

        /* Bind parameters */
        index = 1;
        for (l = params; l != NULL; l = l->next, index++)
        {
            OrmValue *value = (OrmValue *) l->data;
            if (!bind_value_to_sqlite (stmt, index, value, error))
            {
                sqlite3_finalize (stmt);
                return FALSE;
            }
        }

        /* Execute */
        rc = sqlite3_step (stmt);
        sqlite3_finalize (stmt);

        if (rc != SQLITE_DONE && rc != SQLITE_ROW)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                         "Failed to execute: %s",
                         sqlite3_errmsg (self->sqlite_db));
            return FALSE;
        }

        return TRUE;
    }
#endif

#ifdef ORM_ENABLE_POSTGRES
    if (self->dialect_type == ORM_DIALECT_POSTGRES)
    {
        PGresult *res;
        int n_params;
        const char **param_values = NULL;
        gchar **param_strings = NULL;
        GList *l;
        int i;

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

        res = PQexecParams (self->pg_conn,
                            sql,
                            n_params,
                            NULL,        /* paramTypes - let server infer */
                            param_values,
                            NULL,        /* paramLengths */
                            NULL,        /* paramFormats */
                            0);          /* resultFormat - text */

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

        if (PQresultStatus (res) != PGRES_COMMAND_OK &&
            PQresultStatus (res) != PGRES_TUPLES_OK)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                         "SQL error: %s", PQerrorMessage (self->pg_conn));
            PQclear (res);
            return FALSE;
        }

        PQclear (res);
        return TRUE;
    }
#endif

#ifdef ORM_ENABLE_MYSQL
    if (self->dialect_type == ORM_DIALECT_MYSQL)
    {
        MYSQL_STMT *stmt = NULL;
        MYSQL_BIND *binds = NULL;
        gpointer *buffers = NULL;
        int n_params;
        GList *l;
        int i;
        gboolean success = TRUE;

        stmt = mysql_stmt_init (self->mysql_conn);
        if (stmt == NULL)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_PREPARE,
                         "Failed to initialize statement");
            return FALSE;
        }

        if (mysql_stmt_prepare (stmt, sql, strlen (sql)) != 0)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_PREPARE,
                         "Failed to prepare statement: %s",
                         mysql_stmt_error (stmt));
            mysql_stmt_close (stmt);
            return FALSE;
        }

        n_params = g_list_length (params);

        if (n_params > 0)
        {
            binds = g_new0 (MYSQL_BIND, n_params);
            buffers = g_new0 (gpointer, n_params);

            i = 0;
            for (l = params; l != NULL; l = l->next, i++)
            {
                OrmValue *value = (OrmValue *) l->data;
                if (!bind_value_to_mysql (&binds[i], value, &buffers[i], error))
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

        /* Cleanup */
        if (buffers != NULL)
        {
            for (i = 0; i < n_params; i++)
            {
                g_free (buffers[i]);
            }
            g_free (buffers);
        }
        g_free (binds);
        mysql_stmt_close (stmt);

        return success;
    }
#endif

    g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_IMPLEMENTED,
                 "Execute with params not implemented for dialect");
    return FALSE;
}

/**
 * orm_connection_query:
 * @self: An #OrmConnection
 * @sql: SQL query
 * @error: Return location for error
 *
 * Executes a SQL query and returns results.
 *
 * Returns: (transfer full) (nullable): Query results, or %NULL on error
 */
OrmResult *
orm_connection_query (OrmConnection  *self,
                      const gchar    *sql,
                      GError        **error)
{
    return orm_connection_query_with_params (self, sql, NULL, error);
}

/**
 * orm_connection_query_with_params:
 * @self: An #OrmConnection
 * @sql: SQL query with ? placeholders
 * @params: (element-type OrmValue) (nullable): Parameter values
 * @error: Return location for error
 *
 * Executes a parameterized SQL query.
 *
 * Returns: (transfer full) (nullable): Query results, or %NULL on error
 */
OrmResult *
orm_connection_query_with_params (OrmConnection  *self,
                                  const gchar    *sql,
                                  GList          *params,
                                  GError        **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    g_return_val_if_fail (sql != NULL, NULL);
    g_return_val_if_fail (self->is_open, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

#ifdef ORM_ENABLE_SQLITE
    if (self->dialect_type == ORM_DIALECT_SQLITE)
    {
        return orm_result_new_sqlite (self, sql, params, error);
    }
#endif

#ifdef ORM_ENABLE_POSTGRES
    if (self->dialect_type == ORM_DIALECT_POSTGRES)
    {
        return orm_result_new_postgres (self, sql, params, error);
    }
#endif

#ifdef ORM_ENABLE_MYSQL
    if (self->dialect_type == ORM_DIALECT_MYSQL)
    {
        return orm_result_new_mysql (self, sql, params, error);
    }
#endif

    g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_IMPLEMENTED,
                 "Query not implemented for dialect");
    return NULL;
}

/**
 * orm_connection_begin_transaction:
 * @self: An #OrmConnection
 * @error: Return location for error
 *
 * Begins a new transaction.
 *
 * Returns: (transfer full) (nullable): A new #OrmTransaction, or %NULL on error
 */
OrmTransaction *
orm_connection_begin_transaction (OrmConnection  *self,
                                  GError        **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    g_return_val_if_fail (self->is_open, NULL);
    g_return_val_if_fail (!self->in_transaction, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    return orm_transaction_new (self, error);
}

/*
 * Spells an isolation level the way every SQL backend spells it.  The
 * words are identical across PostgreSQL and MySQL, so one table serves
 * both; SQLite does not accept them at all and is handled separately.
 */
static const gchar *
orm_isolation_level_sql (OrmIsolationLevel level)
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

/*
 * Applies @level to @self.
 *
 * @for_next_transaction selects between the session-wide form and the form
 * that affects only the transaction about to start.  The two backends
 * disagree about ordering, which is why the caller cannot simply always
 * emit the same statement: MySQL's SET TRANSACTION configures the *next*
 * transaction and so must precede BEGIN, while PostgreSQL's must be the
 * first statement *inside* the transaction.  See
 * orm_connection_begin_transaction_with_isolation.
 */
static gboolean
orm_connection_apply_isolation (OrmConnection      *self,
                                OrmIsolationLevel   level,
                                gboolean            for_next_transaction,
                                GError            **error)
{
    const gchar      *words;
    g_autofree gchar *sql = NULL;

    words = orm_isolation_level_sql (level);
    if (words == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                     "Unknown isolation level: %d", (gint) level);
        return FALSE;
    }

    switch (self->dialect_type)
    {
    case ORM_DIALECT_SQLITE:
        /*
         * SQLite has no isolation-level statement.  Its own behaviour is
         * SERIALIZABLE, so asking for that is a no-op success rather than
         * an error; READ UNCOMMITTED is reachable through a pragma, and
         * the two levels in between simply do not exist here.
         */
        if (level == ORM_ISOLATION_SERIALIZABLE)
            return TRUE;

        if (level == ORM_ISOLATION_READ_UNCOMMITTED)
            return orm_connection_execute (self, "PRAGMA read_uncommitted = 1", error);

        g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                     "SQLite supports only SERIALIZABLE and READ UNCOMMITTED "
                     "isolation, not %s", words);
        return FALSE;

    case ORM_DIALECT_POSTGRES:
        sql = g_strdup_printf (for_next_transaction
                               ? "SET TRANSACTION ISOLATION LEVEL %s"
                               : "SET SESSION CHARACTERISTICS AS TRANSACTION "
                                 "ISOLATION LEVEL %s",
                               words);
        return orm_connection_execute (self, sql, error);

    case ORM_DIALECT_MYSQL:
        sql = g_strdup_printf (for_next_transaction
                               ? "SET TRANSACTION ISOLATION LEVEL %s"
                               : "SET SESSION TRANSACTION ISOLATION LEVEL %s",
                               words);
        return orm_connection_execute (self, sql, error);

    default:
        g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                     "Isolation levels are not supported by this dialect");
        return FALSE;
    }
}

/**
 * orm_connection_set_isolation_level:
 * @self: An #OrmConnection
 * @level: The isolation level to apply
 * @error: Return location for error
 *
 * Sets the transaction isolation level for the whole session, so it
 * governs every transaction started afterwards on this connection.
 *
 * SQLite accepts only %ORM_ISOLATION_SERIALIZABLE (its native behaviour,
 * applied as a no-op) and %ORM_ISOLATION_READ_UNCOMMITTED; any other level
 * fails with %ORM_ERROR_NOT_SUPPORTED.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_connection_set_isolation_level (OrmConnection      *self,
                                    OrmIsolationLevel   level,
                                    GError            **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    g_return_val_if_fail (self->is_open, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    if (!orm_connection_apply_isolation (self, level, FALSE, error))
        return FALSE;

    self->isolation_level = level;
    return TRUE;
}

/**
 * orm_connection_get_isolation_level:
 * @self: An #OrmConnection
 *
 * Gets the isolation level this connection is known to be using.
 *
 * The value is tracked rather than queried: it starts at the backend's
 * documented default and follows every successful
 * orm_connection_set_isolation_level(). A level changed behind the
 * library's back -- by raw SQL, say -- is not reflected here.
 *
 * Returns: The current #OrmIsolationLevel
 */
OrmIsolationLevel
orm_connection_get_isolation_level (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), ORM_ISOLATION_SERIALIZABLE);
    return self->isolation_level;
}

/**
 * orm_connection_begin_transaction_with_isolation:
 * @self: An #OrmConnection
 * @level: The isolation level for this transaction only
 * @error: Return location for error
 *
 * Begins a transaction that runs at @level, leaving the session default
 * untouched.
 *
 * Returns: (transfer full) (nullable): A new #OrmTransaction, or %NULL on error
 */
OrmTransaction *
orm_connection_begin_transaction_with_isolation (OrmConnection      *self,
                                                 OrmIsolationLevel   level,
                                                 GError            **error)
{
    OrmTransaction *transaction;

    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    g_return_val_if_fail (self->is_open, NULL);
    g_return_val_if_fail (!self->in_transaction, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    /*
     * MySQL wants the level set before the transaction opens; PostgreSQL
     * wants it as the transaction's first statement.  SQLite's pragma is
     * connection-scoped and so belongs before BEGIN as well.
     */
    if (self->dialect_type != ORM_DIALECT_POSTGRES)
    {
        if (!orm_connection_apply_isolation (self, level, TRUE, error))
            return NULL;

        return orm_transaction_new (self, error);
    }

    transaction = orm_transaction_new (self, error);
    if (transaction == NULL)
        return NULL;

    if (!orm_connection_apply_isolation (self, level, TRUE, error))
    {
        /*
         * Roll back rather than hand back a transaction running at the
         * wrong isolation level -- a caller that asked for SERIALIZABLE
         * and silently got READ COMMITTED is the worst outcome here.
         */
        orm_transaction_rollback (transaction, NULL);
        g_object_unref (transaction);
        return NULL;
    }

    return transaction;
}

/**
 * orm_connection_in_transaction:
 * @self: An #OrmConnection
 *
 * Checks if a transaction is active.
 *
 * Returns: %TRUE if in a transaction
 */
gboolean
orm_connection_in_transaction (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    return self->in_transaction;
}

/**
 * orm_connection_get_last_insert_rowid:
 * @self: An #OrmConnection
 *
 * Gets the rowid of the last inserted row.
 *
 * Returns: The last insert rowid
 */
gint64
orm_connection_get_last_insert_rowid (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), 0);
    g_return_val_if_fail (self->is_open, 0);

#ifdef ORM_ENABLE_SQLITE
    if (self->dialect_type == ORM_DIALECT_SQLITE)
    {
        return sqlite3_last_insert_rowid (self->sqlite_db);
    }
#endif

#ifdef ORM_ENABLE_POSTGRES
    if (self->dialect_type == ORM_DIALECT_POSTGRES)
    {
        /*
         * PostgreSQL doesn't have a simple last_insert_id function.
         * You need to use RETURNING clause or query the sequence directly.
         * This returns 0 as a placeholder - use RETURNING in queries instead.
         */
        return 0;
    }
#endif

#ifdef ORM_ENABLE_MYSQL
    if (self->dialect_type == ORM_DIALECT_MYSQL)
    {
        return (gint64) mysql_insert_id (self->mysql_conn);
    }
#endif

    return 0;
}

/**
 * orm_connection_get_changes:
 * @self: An #OrmConnection
 *
 * Gets the number of rows changed by the last statement.
 *
 * Returns: Number of changed rows
 */
gint
orm_connection_get_changes (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), 0);
    g_return_val_if_fail (self->is_open, 0);

#ifdef ORM_ENABLE_SQLITE
    if (self->dialect_type == ORM_DIALECT_SQLITE)
    {
        return sqlite3_changes (self->sqlite_db);
    }
#endif

#ifdef ORM_ENABLE_POSTGRES
    if (self->dialect_type == ORM_DIALECT_POSTGRES)
    {
        /*
         * PostgreSQL tracks affected rows per statement result.
         * This needs to be tracked from the last PGresult.
         * Returns 0 as placeholder - use PQcmdTuples on result instead.
         */
        return 0;
    }
#endif

#ifdef ORM_ENABLE_MYSQL
    if (self->dialect_type == ORM_DIALECT_MYSQL)
    {
        return (gint) mysql_affected_rows (self->mysql_conn);
    }
#endif

    return 0;
}

/**
 * orm_connection_get_engine:
 * @self: An #OrmConnection
 *
 * Gets the engine that created this connection.
 *
 * Returns: (transfer none): The engine
 */
OrmEngine *
orm_connection_get_engine (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    return self->engine;
}

/*
 * Internal: Set transaction state.
 */
void
orm_connection_set_in_transaction (OrmConnection *self,
                                   gboolean       in_transaction)
{
    g_return_if_fail (ORM_IS_CONNECTION (self));
    self->in_transaction = in_transaction;
}

/*
 * Internal: Get SQLite handle.
 */
#ifdef ORM_ENABLE_SQLITE
sqlite3 *
orm_connection_get_sqlite_db (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    return self->sqlite_db;
}
#endif

/*
 * Internal: Get PostgreSQL connection handle.
 */
#ifdef ORM_ENABLE_POSTGRES
PGconn *
orm_connection_get_pg_conn (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    return self->pg_conn;
}
#endif

/*
 * Internal: Get MySQL connection handle.
 */
#ifdef ORM_ENABLE_MYSQL
MYSQL *
orm_connection_get_mysql_conn (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    return self->mysql_conn;
}
#endif
