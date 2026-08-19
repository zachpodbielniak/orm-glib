/* orm-sqlite-driver.c
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

#include "orm-sqlite-driver.h"
#include "../../core/orm-error.h"
#include "../../engine/orm-engine.h"
#include "../../dialect/sqlite/orm-sqlite-dialect.h"

#include <sqlite3.h>
#include <string.h>

/*
 * The SQLite backend: driver, connection and result.
 *
 * All three live in one file because they are one implementation -- the
 * result holds a statement belonging to the connection's database handle,
 * and splitting them across translation units would mean exporting that
 * relationship rather than keeping it private.
 *
 * SQLite is the one backend that genuinely streams: a result is a
 * prepared statement being stepped, so rows arrive one at a time whether
 * or not the caller asked for streaming, and ORM_QUERY_FLAGS_STREAMING is
 * therefore a no-op here rather than a second code path.
 */

/* Internal: the engine holds the parsed connection parameters. */
extern const gchar * orm_engine_get_database_path (OrmEngine *engine);

/* ------------------------------------------------------------------ */
/* Result                                                             */
/* ------------------------------------------------------------------ */

#define ORM_TYPE_SQLITE_DRIVER_RESULT (orm_sqlite_driver_result_get_type ())

G_DECLARE_FINAL_TYPE (OrmSqliteDriverResult, orm_sqlite_driver_result,
                      ORM, SQLITE_DRIVER_RESULT, OrmDriverResult)

struct _OrmSqliteDriverResult
{
    OrmDriverResult  parent_instance;

    sqlite3_stmt    *stmt;
    gboolean         exhausted;
};

G_DEFINE_FINAL_TYPE (OrmSqliteDriverResult, orm_sqlite_driver_result,
                     ORM_TYPE_DRIVER_RESULT)

/*
 * Reads one column of the current row into an OrmValue.
 */
static OrmValue *
orm_sqlite_column_to_value (sqlite3_stmt *stmt,
                            gint          col)
{
    switch (sqlite3_column_type (stmt, col))
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
            gint        size = sqlite3_column_bytes (stmt, col);
            GBytes     *bytes = g_bytes_new (data, size);
            OrmValue   *value = orm_value_new_blob (bytes);

            g_bytes_unref (bytes);
            return value;
        }

    default:
        return orm_value_new_null ();
    }
}

static gint
orm_sqlite_driver_result_get_column_count (OrmDriverResult *result)
{
    OrmSqliteDriverResult *self = ORM_SQLITE_DRIVER_RESULT (result);

    if (self->stmt == NULL)
        return 0;

    return sqlite3_column_count (self->stmt);
}

static const gchar *
orm_sqlite_driver_result_get_column_name (OrmDriverResult *result,
                                          gint             index)
{
    OrmSqliteDriverResult *self = ORM_SQLITE_DRIVER_RESULT (result);

    if (self->stmt == NULL)
        return NULL;

    return sqlite3_column_name (self->stmt, index);
}

/*
 * Maps a declared type name onto a value type using SQLite's own type
 * affinity rules (the substring tests from its documentation, in the
 * order it applies them).
 *
 * SQLite stores types per value, not per column, so the declared type is
 * the only thing that describes a column as a whole -- and it is a free
 * text string: "VARCHAR(80)", "BIG INT" and "nvarchar" are all things a
 * schema can legitimately say.
 */
static OrmValueType
orm_sqlite_affinity_of (const gchar *decl)
{
    g_autofree gchar *upper = NULL;

    /*
     * A column declared with no type at all has BLOB affinity.  This is
     * not the same as having no declared type to read, which the caller
     * handles before getting here.
     */
    if (*decl == '\0')
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
     * Past this point SQLite would say NUMERIC.  These two are checked
     * after the affinity rules rather than before because a column
     * declared BOOLEAN or DATETIME has numeric affinity as far as SQLite
     * is concerned, while orm-glib has real types for both.
     */
    if (strstr (upper, "BOOL") != NULL)
        return ORM_VALUE_BOOLEAN;

    if (strstr (upper, "DATE") != NULL || strstr (upper, "TIME") != NULL)
        return ORM_VALUE_DATETIME;

    return ORM_VALUE_FLOAT;
}

static OrmValueType
orm_sqlite_driver_result_get_column_value_type (OrmDriverResult *result,
                                                gint             index)
{
    OrmSqliteDriverResult *self = ORM_SQLITE_DRIVER_RESULT (result);
    const gchar           *decl;

    if (self->stmt == NULL)
        return ORM_VALUE_NULL;

    decl = sqlite3_column_decltype (self->stmt, index);

    /*
     * A computed column -- COUNT(*), an expression, a literal -- has no
     * declared type at all, and sqlite3_column_decltype returns NULL.
     * Saying "unknown" is the truthful answer; guessing from whatever the
     * current row happens to hold would be wrong on the next row.
     */
    if (decl == NULL)
        return ORM_VALUE_NULL;

    return orm_sqlite_affinity_of (decl);
}

static const gchar *
orm_sqlite_driver_result_get_column_type_name (OrmDriverResult *result,
                                               gint             index)
{
    OrmSqliteDriverResult *self = ORM_SQLITE_DRIVER_RESULT (result);

    if (self->stmt == NULL)
        return NULL;

    return sqlite3_column_decltype (self->stmt, index);
}

static OrmRow *
orm_sqlite_driver_result_fetch_row (OrmDriverResult  *result,
                                    GError          **error)
{
    OrmSqliteDriverResult *self = ORM_SQLITE_DRIVER_RESULT (result);
    GPtrArray             *names;
    GPtrArray             *values;
    gint                   col_count;
    gint                   i;
    gint                   rc;

    if (self->stmt == NULL || self->exhausted)
        return NULL;

    rc = sqlite3_step (self->stmt);

    if (rc == SQLITE_DONE)
    {
        self->exhausted = TRUE;
        return NULL;
    }

    if (rc != SQLITE_ROW)
    {
        /*
         * A step failure used to be a g_warning and an end-of-results,
         * which made a broken query indistinguishable from an empty one.
         * It is an error now, and the caller gets to see it.
         */
        self->exhausted = TRUE;
        g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                     "SQLite step failed: %s",
                     sqlite3_errstr (rc));
        return NULL;
    }

    col_count = sqlite3_column_count (self->stmt);
    names = g_ptr_array_new_full (col_count, g_free);
    values = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_value_free);

    for (i = 0; i < col_count; i++)
    {
        g_ptr_array_add (names, g_strdup (sqlite3_column_name (self->stmt, i)));
        g_ptr_array_add (values, orm_sqlite_column_to_value (self->stmt, i));
    }

    return orm_row_new (names, values);
}

static void
orm_sqlite_driver_result_close (OrmDriverResult *result)
{
    OrmSqliteDriverResult *self = ORM_SQLITE_DRIVER_RESULT (result);

    if (self->stmt != NULL)
    {
        sqlite3_finalize (self->stmt);
        self->stmt = NULL;
    }

    self->exhausted = TRUE;
}

static void
orm_sqlite_driver_result_finalize (GObject *object)
{
    orm_sqlite_driver_result_close (ORM_DRIVER_RESULT (object));

    G_OBJECT_CLASS (orm_sqlite_driver_result_parent_class)->finalize (object);
}

static void
orm_sqlite_driver_result_class_init (OrmSqliteDriverResultClass *klass)
{
    GObjectClass         *object_class = G_OBJECT_CLASS (klass);
    OrmDriverResultClass *result_class = ORM_DRIVER_RESULT_CLASS (klass);

    object_class->finalize = orm_sqlite_driver_result_finalize;

    result_class->get_column_count = orm_sqlite_driver_result_get_column_count;
    result_class->get_column_name = orm_sqlite_driver_result_get_column_name;
    result_class->get_column_value_type = orm_sqlite_driver_result_get_column_value_type;
    result_class->get_column_type_name = orm_sqlite_driver_result_get_column_type_name;
    result_class->fetch_row = orm_sqlite_driver_result_fetch_row;
    result_class->close = orm_sqlite_driver_result_close;
}

static void
orm_sqlite_driver_result_init (OrmSqliteDriverResult *self)
{
    self->stmt = NULL;
    self->exhausted = FALSE;
}

/* ------------------------------------------------------------------ */
/* Connection                                                         */
/* ------------------------------------------------------------------ */

#define ORM_TYPE_SQLITE_DRIVER_CONNECTION (orm_sqlite_driver_connection_get_type ())

G_DECLARE_FINAL_TYPE (OrmSqliteDriverConnection, orm_sqlite_driver_connection,
                      ORM, SQLITE_DRIVER_CONNECTION, OrmDriverConnection)

struct _OrmSqliteDriverConnection
{
    OrmDriverConnection  parent_instance;

    sqlite3             *db;
};

G_DEFINE_FINAL_TYPE (OrmSqliteDriverConnection, orm_sqlite_driver_connection,
                     ORM_TYPE_DRIVER_CONNECTION)

/*
 * Binds one OrmValue to a prepared statement parameter.
 */
static gboolean
orm_sqlite_bind_value (sqlite3_stmt  *stmt,
                       gint           index,
                       OrmValue      *value,
                       GError       **error)
{
    gint rc;

    if (value == NULL)
    {
        rc = sqlite3_bind_null (stmt, index);
    }
    else
    {
        switch (orm_value_get_value_type (value))
        {
        case ORM_VALUE_NULL:
            rc = sqlite3_bind_null (stmt, index);
            break;

        case ORM_VALUE_INTEGER:
            rc = sqlite3_bind_int64 (stmt, index, orm_value_get_integer (value));
            break;

        case ORM_VALUE_FLOAT:
            rc = sqlite3_bind_double (stmt, index, orm_value_get_float (value));
            break;

        case ORM_VALUE_STRING:
            rc = sqlite3_bind_text (stmt, index, orm_value_get_string (value),
                                    -1, SQLITE_TRANSIENT);
            break;

        case ORM_VALUE_BOOLEAN:
            rc = sqlite3_bind_int (stmt, index,
                                   orm_value_get_boolean (value) ? 1 : 0);
            break;

        case ORM_VALUE_DATETIME:
            {
                GDateTime        *dt = orm_value_get_datetime (value);
                g_autofree gchar *iso = g_date_time_format_iso8601 (dt);

                rc = sqlite3_bind_text (stmt, index, iso, -1, SQLITE_TRANSIENT);
            }
            break;

        case ORM_VALUE_BLOB:
            {
                GBytes       *bytes = orm_value_get_blob (value);
                gsize         size;
                gconstpointer data = g_bytes_get_data (bytes, &size);

                rc = sqlite3_bind_blob (stmt, index, data, (gint) size,
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

/*
 * Prepares @sql and binds @params, leaving the statement ready to step.
 */
static sqlite3_stmt *
orm_sqlite_prepare (OrmSqliteDriverConnection  *self,
                    const gchar                *sql,
                    GList                      *params,
                    GError                    **error)
{
    sqlite3_stmt *stmt = NULL;
    GList        *l;
    gint          index;
    gint          rc;

    rc = sqlite3_prepare_v2 (self->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_PREPARE,
                     "Failed to prepare statement: %s",
                     sqlite3_errmsg (self->db));
        return NULL;
    }

    index = 1;
    for (l = params; l != NULL; l = l->next, index++)
    {
        if (!orm_sqlite_bind_value (stmt, index, (OrmValue *) l->data, error))
        {
            sqlite3_finalize (stmt);
            return NULL;
        }
    }

    return stmt;
}

static void
orm_sqlite_driver_connection_close (OrmDriverConnection *connection)
{
    OrmSqliteDriverConnection *self = ORM_SQLITE_DRIVER_CONNECTION (connection);

    if (self->db != NULL)
    {
        sqlite3_close (self->db);
        self->db = NULL;
    }
}

static gboolean
orm_sqlite_driver_connection_execute (OrmDriverConnection  *connection,
                                      const gchar          *sql,
                                      GList                *params,
                                      GError              **error)
{
    OrmSqliteDriverConnection *self = ORM_SQLITE_DRIVER_CONNECTION (connection);
    sqlite3_stmt              *stmt;
    gint                       rc;

    g_return_val_if_fail (self->db != NULL, FALSE);

    /*
     * No parameters means sqlite3_exec, which handles several statements
     * separated by semicolons; the prepared path handles exactly one.
     * Callers rely on the multi-statement behaviour for schema setup.
     */
    if (params == NULL)
    {
        gchar *errmsg = NULL;

        rc = sqlite3_exec (self->db, sql, NULL, NULL, &errmsg);

        if (rc != SQLITE_OK)
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                         "SQL error: %s", errmsg ? errmsg : "unknown");
            sqlite3_free (errmsg);
            return FALSE;
        }

        return TRUE;
    }

    stmt = orm_sqlite_prepare (self, sql, params, error);
    if (stmt == NULL)
        return FALSE;

    rc = sqlite3_step (stmt);

    if (rc != SQLITE_DONE && rc != SQLITE_ROW)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                     "SQL error: %s", sqlite3_errmsg (self->db));
        sqlite3_finalize (stmt);
        return FALSE;
    }

    sqlite3_finalize (stmt);
    return TRUE;
}

static OrmDriverResult *
orm_sqlite_driver_connection_query (OrmDriverConnection  *connection,
                                    const gchar          *sql,
                                    GList                *params,
                                    OrmQueryFlags         flags,
                                    GError              **error)
{
    OrmSqliteDriverConnection *self = ORM_SQLITE_DRIVER_CONNECTION (connection);
    OrmSqliteDriverResult     *result;
    sqlite3_stmt              *stmt;

    g_return_val_if_fail (self->db != NULL, NULL);

    stmt = orm_sqlite_prepare (self, sql, params, error);
    if (stmt == NULL)
        return NULL;

    result = g_object_new (ORM_TYPE_SQLITE_DRIVER_RESULT, NULL);
    result->stmt = stmt;

    return ORM_DRIVER_RESULT (result);
}

static gint64
orm_sqlite_driver_connection_get_last_insert_id (OrmDriverConnection *connection)
{
    OrmSqliteDriverConnection *self = ORM_SQLITE_DRIVER_CONNECTION (connection);

    if (self->db == NULL)
        return 0;

    return sqlite3_last_insert_rowid (self->db);
}

static gint
orm_sqlite_driver_connection_get_changes (OrmDriverConnection *connection)
{
    OrmSqliteDriverConnection *self = ORM_SQLITE_DRIVER_CONNECTION (connection);

    if (self->db == NULL)
        return 0;

    return sqlite3_changes (self->db);
}

static gboolean
orm_sqlite_driver_connection_set_isolation_level (OrmDriverConnection  *connection,
                                                  OrmIsolationLevel     level,
                                                  gboolean              for_next_transaction,
                                                  GError              **error)
{
    /*
     * SQLite has no isolation-level statement.  It is serializable
     * natively, so that request is a no-op success; READ UNCOMMITTED is
     * reachable through a pragma.  The two levels in between do not exist
     * here, and saying so beats quietly giving the caller weaker
     * guarantees than it asked for.
     */
    if (level == ORM_ISOLATION_SERIALIZABLE)
        return TRUE;

    if (level == ORM_ISOLATION_READ_UNCOMMITTED)
        return orm_driver_connection_execute (connection,
                                              "PRAGMA read_uncommitted = 1",
                                              NULL, error);

    g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                 "SQLite supports only SERIALIZABLE and READ UNCOMMITTED isolation");
    return FALSE;
}

static gboolean
orm_sqlite_driver_connection_interrupt (OrmDriverConnection  *connection,
                                        GError              **error)
{
    OrmSqliteDriverConnection *self = ORM_SQLITE_DRIVER_CONNECTION (connection);

    if (self->db == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                     "Connection is closed");
        return FALSE;
    }

    /* Documented safe to call from another thread while a step runs. */
    sqlite3_interrupt (self->db);
    return TRUE;
}

static void
orm_sqlite_driver_connection_finalize (GObject *object)
{
    orm_sqlite_driver_connection_close (ORM_DRIVER_CONNECTION (object));

    G_OBJECT_CLASS (orm_sqlite_driver_connection_parent_class)->finalize (object);
}

static void
orm_sqlite_driver_connection_class_init (OrmSqliteDriverConnectionClass *klass)
{
    GObjectClass             *object_class = G_OBJECT_CLASS (klass);
    OrmDriverConnectionClass *conn_class = ORM_DRIVER_CONNECTION_CLASS (klass);

    object_class->finalize = orm_sqlite_driver_connection_finalize;

    conn_class->close = orm_sqlite_driver_connection_close;
    conn_class->execute = orm_sqlite_driver_connection_execute;
    conn_class->query = orm_sqlite_driver_connection_query;
    conn_class->get_last_insert_id = orm_sqlite_driver_connection_get_last_insert_id;
    conn_class->get_changes = orm_sqlite_driver_connection_get_changes;
    conn_class->set_isolation_level = orm_sqlite_driver_connection_set_isolation_level;
    conn_class->interrupt = orm_sqlite_driver_connection_interrupt;
}

static void
orm_sqlite_driver_connection_init (OrmSqliteDriverConnection *self)
{
    self->db = NULL;
}

/* ------------------------------------------------------------------ */
/* Driver                                                             */
/* ------------------------------------------------------------------ */

struct _OrmSqliteDriver
{
    OrmDriver parent_instance;
};

G_DEFINE_FINAL_TYPE (OrmSqliteDriver, orm_sqlite_driver, ORM_TYPE_DRIVER)

static const gchar *
orm_sqlite_driver_get_name (OrmDriver *driver)
{
    return "sqlite";
}

static const gchar * const *
orm_sqlite_driver_get_schemes (OrmDriver *driver)
{
    static const gchar * const schemes[] = { "sqlite", "sqlite3", NULL };

    return schemes;
}

static OrmDialectType
orm_sqlite_driver_get_dialect_type (OrmDriver *driver)
{
    return ORM_DIALECT_SQLITE;
}

static OrmDialect *
orm_sqlite_driver_create_dialect (OrmDriver *driver)
{
    return ORM_DIALECT (orm_sqlite_dialect_new ());
}

static OrmDriverConnection *
orm_sqlite_driver_open (OrmDriver  *driver,
                        OrmEngine  *engine,
                        GError    **error)
{
    OrmSqliteDriverConnection *self;
    const gchar               *path;
    sqlite3                   *db = NULL;
    gint                       rc;

    path = orm_engine_get_database_path (engine);
    if (path == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_URL,
                     "No database path in the connection URL");
        return NULL;
    }

    rc = sqlite3_open (path, &db);
    if (rc != SQLITE_OK)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                     "Failed to open SQLite database: %s",
                     db != NULL ? sqlite3_errmsg (db) : sqlite3_errstr (rc));
        sqlite3_close (db);
        return NULL;
    }

    /*
     * Foreign keys are off by default in SQLite, which silently turns
     * every foreign key in a schema into documentation.
     */
    sqlite3_exec (db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);

    self = g_object_new (ORM_TYPE_SQLITE_DRIVER_CONNECTION, NULL);
    self->db = db;

    return ORM_DRIVER_CONNECTION (self);
}

static void
orm_sqlite_driver_class_init (OrmSqliteDriverClass *klass)
{
    OrmDriverClass *driver_class = ORM_DRIVER_CLASS (klass);

    driver_class->get_name = orm_sqlite_driver_get_name;
    driver_class->get_schemes = orm_sqlite_driver_get_schemes;
    driver_class->get_dialect_type = orm_sqlite_driver_get_dialect_type;
    driver_class->create_dialect = orm_sqlite_driver_create_dialect;
    driver_class->open = orm_sqlite_driver_open;
}

static void
orm_sqlite_driver_init (OrmSqliteDriver *self)
{
}

/**
 * orm_sqlite_driver_new:
 *
 * Creates the SQLite driver.
 *
 * Returns: (transfer full): A new #OrmSqliteDriver
 */
OrmSqliteDriver *
orm_sqlite_driver_new (void)
{
    return g_object_new (ORM_TYPE_SQLITE_DRIVER, NULL);
}
