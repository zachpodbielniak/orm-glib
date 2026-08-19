/* orm-postgres-driver.c
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

#include "orm-postgres-driver.h"
#include "../../core/orm-error.h"
#include "../../engine/orm-engine.h"
#include "../../dialect/postgres/orm-postgres-dialect.h"

#include <libpq-fe.h>

/*
 * The PostgreSQL backend: driver, connection and result.
 *
 * All three live in one file because they are one implementation -- the
 * result holds a PGresult produced by the connection's PGconn, and
 * splitting them across translation units would mean exporting that
 * relationship rather than keeping it private.
 *
 * PostgreSQL materializes: PQexecParams does not return until the server
 * has sent every row, so a result is an array being walked rather than a
 * cursor being stepped.  ORM_QUERY_FLAGS_STREAMING is therefore accepted
 * and ignored; single-row mode needs the asynchronous PQsendQueryParams
 * path and arrives with the async work.
 */

/* Internal: the engine holds the parsed connection parameters. */
extern const gchar * orm_engine_get_host (OrmEngine *engine);
extern gint orm_engine_get_port (OrmEngine *engine);
extern const gchar * orm_engine_get_username (OrmEngine *engine);
extern const gchar * orm_engine_get_password (OrmEngine *engine);
extern const gchar * orm_engine_get_database (OrmEngine *engine);

/*
 * PostgreSQL identifies types by OID, and resolving one to a name means
 * asking the server (format_type, or a pg_type lookup).  That is a round
 * trip per column, on a connection the caller is probably about to use
 * for something else, to answer a question about metadata -- so the
 * common types are answered from this table instead and anything else
 * reports its bare OID.
 *
 * The OIDs are fixed: they are assigned in the catalog at initdb time
 * for built-in types and have been stable across every release.
 */
typedef struct
{
    Oid           oid;
    const gchar  *name;
    OrmValueType  value_type;
} OrmPostgresTypeInfo;

static const OrmPostgresTypeInfo orm_postgres_types[] = {
    {   16, "boolean",                     ORM_VALUE_BOOLEAN  },
    {   17, "bytea",                       ORM_VALUE_BLOB     },
    {   18, "char",                        ORM_VALUE_STRING   },
    {   19, "name",                        ORM_VALUE_STRING   },
    {   20, "bigint",                      ORM_VALUE_INTEGER  },
    {   21, "smallint",                    ORM_VALUE_INTEGER  },
    {   23, "integer",                     ORM_VALUE_INTEGER  },
    {   25, "text",                        ORM_VALUE_STRING   },
    {   26, "oid",                         ORM_VALUE_INTEGER  },
    {  114, "json",                        ORM_VALUE_STRING   },
    {  700, "real",                        ORM_VALUE_FLOAT    },
    {  701, "double precision",            ORM_VALUE_FLOAT    },
    {  705, "unknown",                     ORM_VALUE_STRING   },
    { 1042, "character",                   ORM_VALUE_STRING   },
    { 1043, "character varying",           ORM_VALUE_STRING   },
    { 1082, "date",                        ORM_VALUE_DATETIME },
    { 1083, "time without time zone",      ORM_VALUE_STRING   },
    { 1114, "timestamp without time zone", ORM_VALUE_DATETIME },
    { 1184, "timestamp with time zone",    ORM_VALUE_DATETIME },
    { 1186, "interval",                    ORM_VALUE_STRING   },
    { 1266, "time with time zone",         ORM_VALUE_STRING   },
    { 1700, "numeric",                     ORM_VALUE_FLOAT    },
    { 2950, "uuid",                        ORM_VALUE_STRING   },
    { 3802, "jsonb",                       ORM_VALUE_STRING   },
    {    0, NULL,                          ORM_VALUE_NULL     }
};

static const OrmPostgresTypeInfo *
orm_postgres_type_for_oid (Oid oid)
{
    gint i;

    for (i = 0; orm_postgres_types[i].name != NULL; i++)
    {
        if (orm_postgres_types[i].oid == oid)
            return &orm_postgres_types[i];
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/* Result                                                             */
/* ------------------------------------------------------------------ */

#define ORM_TYPE_POSTGRES_DRIVER_RESULT (orm_postgres_driver_result_get_type ())

G_DECLARE_FINAL_TYPE (OrmPostgresDriverResult, orm_postgres_driver_result,
                      ORM, POSTGRES_DRIVER_RESULT, OrmDriverResult)

struct _OrmPostgresDriverResult
{
    OrmDriverResult  parent_instance;

    PGresult        *result;
    gint             row_count;
    gint             current_row;

    /*
     * Names for OIDs the table does not carry, built on demand.
     * Owned here because get_column_type_name is (transfer none) and the
     * caller therefore needs the string to outlive the call.
     */
    GHashTable      *unknown_type_names;
};

G_DEFINE_FINAL_TYPE (OrmPostgresDriverResult, orm_postgres_driver_result,
                     ORM_TYPE_DRIVER_RESULT)

/*
 * Reads one field of @row into an OrmValue.
 *
 * Text format means every field arrives as a string, so the column's type
 * OID is the only thing that says what the string means.  The OIDs are
 * fixed by the catalog and are what libpq reports; the interesting ones
 * are named below.
 */
static OrmValue *
orm_postgres_field_to_value (PGresult *result,
                             gint      row,
                             gint      col)
{
    const gchar *val;
    Oid          type_oid;

    if (PQgetisnull (result, row, col))
        return orm_value_new_null ();

    val = PQgetvalue (result, row, col);
    type_oid = PQftype (result, col);

    switch (type_oid)
    {
    case 16:  /* bool */
        return orm_value_new_boolean (val[0] == 't' || val[0] == 'T');

    case 21:  /* int2 */
    case 23:  /* int4 */
    case 20:  /* int8 */
    case 26:  /* oid */
        return orm_value_new_integer (g_ascii_strtoll (val, NULL, 10));

    case 700:  /* float4 */
    case 701:  /* float8 */
    case 1700: /* numeric */
        return orm_value_new_float (g_ascii_strtod (val, NULL));

    case 17:  /* bytea */
        /*
         * bytea in text format is escape- or hex-encoded rather than raw
         * bytes, so handing it back as a string is what the caller gets
         * until the decode exists.
         */
        return orm_value_new_string (val);

    default:
        /* text, varchar and everything else the catalog spells as text. */
        return orm_value_new_string (val);
    }
}

static gint
orm_postgres_driver_result_get_column_count (OrmDriverResult *result)
{
    OrmPostgresDriverResult *self = ORM_POSTGRES_DRIVER_RESULT (result);

    if (self->result == NULL)
        return 0;

    return PQnfields (self->result);
}

static const gchar *
orm_postgres_driver_result_get_column_name (OrmDriverResult *result,
                                            gint             index)
{
    OrmPostgresDriverResult *self = ORM_POSTGRES_DRIVER_RESULT (result);

    if (self->result == NULL)
        return NULL;

    return PQfname (self->result, index);
}

static OrmValueType
orm_postgres_driver_result_get_column_value_type (OrmDriverResult *result,
                                                  gint             index)
{
    OrmPostgresDriverResult   *self = ORM_POSTGRES_DRIVER_RESULT (result);
    const OrmPostgresTypeInfo *info;

    if (self->result == NULL || index < 0 || index >= PQnfields (self->result))
        return ORM_VALUE_NULL;

    info = orm_postgres_type_for_oid (PQftype (self->result, index));

    /*
     * An OID we do not recognise is almost always a user-defined type or
     * an array, and PostgreSQL hands those over as text.
     */
    return (info != NULL) ? info->value_type : ORM_VALUE_STRING;
}

static const gchar *
orm_postgres_driver_result_get_column_type_name (OrmDriverResult *result,
                                                 gint             index)
{
    OrmPostgresDriverResult   *self = ORM_POSTGRES_DRIVER_RESULT (result);
    const OrmPostgresTypeInfo *info;
    Oid                        oid;
    gchar                     *name;

    if (self->result == NULL || index < 0 || index >= PQnfields (self->result))
        return NULL;

    oid = PQftype (self->result, index);

    info = orm_postgres_type_for_oid (oid);
    if (info != NULL)
        return info->name;

    if (self->unknown_type_names == NULL)
        self->unknown_type_names = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                          NULL, g_free);

    name = g_hash_table_lookup (self->unknown_type_names, GUINT_TO_POINTER (oid));
    if (name == NULL)
    {
        name = g_strdup_printf ("oid:%u", (guint) oid);
        g_hash_table_insert (self->unknown_type_names, GUINT_TO_POINTER (oid), name);
    }

    return name;
}

static OrmRow *
orm_postgres_driver_result_fetch_row (OrmDriverResult  *result,
                                      GError          **error)
{
    OrmPostgresDriverResult *self = ORM_POSTGRES_DRIVER_RESULT (result);
    GPtrArray               *names;
    GPtrArray               *values;
    gint                     col_count;
    gint                     i;

    if (self->result == NULL || self->current_row >= self->row_count)
        return NULL;

    /*
     * The cursor sits before the first row until it is moved, so the
     * advance leads the bounds check and the first fetch lands on row 0.
     */
    self->current_row++;

    if (self->current_row >= self->row_count)
        return NULL;

    col_count = PQnfields (self->result);
    names = g_ptr_array_new_full (col_count, g_free);
    values = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_value_free);

    for (i = 0; i < col_count; i++)
    {
        g_ptr_array_add (names, g_strdup (PQfname (self->result, i)));
        g_ptr_array_add (values,
                         orm_postgres_field_to_value (self->result,
                                                      self->current_row, i));
    }

    return orm_row_new (names, values);
}

static void
orm_postgres_driver_result_close (OrmDriverResult *result)
{
    OrmPostgresDriverResult *self = ORM_POSTGRES_DRIVER_RESULT (result);

    if (self->result != NULL)
    {
        PQclear (self->result);
        self->result = NULL;
    }

    g_clear_pointer (&self->unknown_type_names, g_hash_table_unref);

    self->row_count = 0;
}

static void
orm_postgres_driver_result_finalize (GObject *object)
{
    orm_postgres_driver_result_close (ORM_DRIVER_RESULT (object));

    G_OBJECT_CLASS (orm_postgres_driver_result_parent_class)->finalize (object);
}

static void
orm_postgres_driver_result_class_init (OrmPostgresDriverResultClass *klass)
{
    GObjectClass         *object_class = G_OBJECT_CLASS (klass);
    OrmDriverResultClass *result_class = ORM_DRIVER_RESULT_CLASS (klass);

    object_class->finalize = orm_postgres_driver_result_finalize;

    result_class->get_column_count = orm_postgres_driver_result_get_column_count;
    result_class->get_column_name = orm_postgres_driver_result_get_column_name;
    result_class->get_column_value_type = orm_postgres_driver_result_get_column_value_type;
    result_class->get_column_type_name = orm_postgres_driver_result_get_column_type_name;
    result_class->fetch_row = orm_postgres_driver_result_fetch_row;
    result_class->close = orm_postgres_driver_result_close;
}

static void
orm_postgres_driver_result_init (OrmPostgresDriverResult *self)
{
    self->result = NULL;
    self->row_count = 0;
    self->current_row = -1;
}

/* ------------------------------------------------------------------ */
/* Connection                                                         */
/* ------------------------------------------------------------------ */

#define ORM_TYPE_POSTGRES_DRIVER_CONNECTION (orm_postgres_driver_connection_get_type ())

G_DECLARE_FINAL_TYPE (OrmPostgresDriverConnection, orm_postgres_driver_connection,
                      ORM, POSTGRES_DRIVER_CONNECTION, OrmDriverConnection)

struct _OrmPostgresDriverConnection
{
    OrmDriverConnection  parent_instance;

    PGconn              *conn;
    PGcancel            *cancel;
    gint                 last_changes;
};

G_DEFINE_FINAL_TYPE (OrmPostgresDriverConnection, orm_postgres_driver_connection,
                     ORM_TYPE_DRIVER_CONNECTION)

/*
 * PostgreSQL sends messages that belong to no result: NOTICE, WARNING,
 * whatever a function RAISEs, the "skipping" from a DROP ... IF EXISTS.
 * libpq's default receiver prints them to stderr, which is nobody's idea
 * of a user interface.
 *
 * They are surfaced as a signal on this type rather than through a
 * method on OrmDriverConnection because no other backend has the notion:
 * SQLite has no such channel at all, and MySQL keeps its warnings until
 * SHOW WARNINGS asks for them.  OrmConnection looks the signal up by
 * name, so a backend that grows one later needs no change there either.
 */
enum {
    SIGNAL_NOTICE,
    N_SIGNALS
};

static guint pg_signals[N_SIGNALS];

/*
 * libpq calls this from inside whichever thread is running the
 * statement, and clears @result itself once this returns -- so the
 * message is copied out and nothing here touches the PGresult
 * afterwards.
 */
static void
orm_postgres_notice_receiver (void             *arg,
                              const PGresult   *result)
{
    OrmPostgresDriverConnection *self = (OrmPostgresDriverConnection *) arg;
    const gchar                 *message;
    g_autofree gchar            *trimmed = NULL;

    message = PQresultErrorMessage (result);
    if (message == NULL || *message == '\0')
        return;

    /* libpq terminates these with a newline; a signal argument should not. */
    trimmed = g_strdup (message);
    g_strchomp (trimmed);

    g_signal_emit (self, pg_signals[SIGNAL_NOTICE], 0, trimmed);
}

/*
 * Renders one OrmValue as the text PostgreSQL parses parameters from.
 *
 * Returns: (transfer full) (nullable): The text, or %NULL for a SQL NULL
 */
static gchar *
orm_postgres_value_to_string (OrmValue *value)
{
    if (value == NULL)
        return NULL;

    switch (orm_value_get_value_type (value))
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
            /* BYTEA in text format: the hex encoding, \x followed by pairs. */
            GBytes       *bytes = orm_value_get_blob (value);
            gsize         size;
            const guchar *data;
            GString      *hex;
            gsize         i;

            data = g_bytes_get_data (bytes, &size);
            hex = g_string_sized_new (size * 2 + 3);
            g_string_append (hex, "\\x");

            for (i = 0; i < size; i++)
                g_string_append_printf (hex, "%02x", data[i]);

            return g_string_free (hex, FALSE);
        }

    default:
        return NULL;
    }
}

/*
 * Renders @params into the array PQexecParams binds from, and reports its
 * length in @n_params.
 *
 * Returns: (transfer full) (nullable): The parameter texts, %NULL when
 *   there are none
 */
static gchar **
orm_postgres_params_new (GList *params,
                         gint  *n_params)
{
    gchar **param_strings;
    GList  *l;
    gint    i;

    *n_params = (gint) g_list_length (params);

    if (*n_params == 0)
        return NULL;

    param_strings = g_new0 (gchar *, *n_params);

    i = 0;
    for (l = params; l != NULL; l = l->next, i++)
        param_strings[i] = orm_postgres_value_to_string ((OrmValue *) l->data);

    return param_strings;
}

/*
 * Releases an array from orm_postgres_params_new().
 */
static void
orm_postgres_params_free (gchar **param_strings,
                          gint    n_params)
{
    gint i;

    if (param_strings == NULL)
        return;

    for (i = 0; i < n_params; i++)
        g_free (param_strings[i]);

    g_free (param_strings);
}

/*
 * Spells an isolation level the way PostgreSQL spells it.
 */
static const gchar *
orm_postgres_isolation_words (OrmIsolationLevel level)
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
orm_postgres_driver_connection_close (OrmDriverConnection *connection)
{
    OrmPostgresDriverConnection *self = ORM_POSTGRES_DRIVER_CONNECTION (connection);

    /* The cancel object is derived from the connection: it goes first. */
    if (self->cancel != NULL)
    {
        PQfreeCancel (self->cancel);
        self->cancel = NULL;
    }

    if (self->conn != NULL)
    {
        PQfinish (self->conn);
        self->conn = NULL;
    }
}

static gboolean
orm_postgres_driver_connection_execute (OrmDriverConnection  *connection,
                                        const gchar          *sql,
                                        GList                *params,
                                        GError              **error)
{
    OrmPostgresDriverConnection *self = ORM_POSTGRES_DRIVER_CONNECTION (connection);
    PGresult                    *res;

    g_return_val_if_fail (self->conn != NULL, FALSE);

    /*
     * No parameters means PQexec, which handles several statements
     * separated by semicolons; the parameterised protocol handles exactly
     * one.  Callers rely on the multi-statement behaviour for schema setup.
     */
    if (params == NULL)
    {
        res = PQexec (self->conn, sql);
    }
    else
    {
        gchar **param_strings;
        gint    n_params;

        param_strings = orm_postgres_params_new (params, &n_params);

        res = PQexecParams (self->conn,
                            sql,
                            n_params,
                            NULL,        /* paramTypes - let the server infer */
                            (const gchar * const *) param_strings,
                            NULL,        /* paramLengths */
                            NULL,        /* paramFormats */
                            0);          /* resultFormat - text */

        orm_postgres_params_free (param_strings, n_params);
    }

    if (PQresultStatus (res) != PGRES_COMMAND_OK &&
        PQresultStatus (res) != PGRES_TUPLES_OK)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                     "SQL error: %s", PQerrorMessage (self->conn));
        PQclear (res);
        return FALSE;
    }

    /*
     * Read the affected-row count off the result before clearing it --
     * PQcmdTuples reads the PGresult, and after PQclear the number is
     * gone.  An empty string means the statement was not one that
     * affects rows (DDL, SET), which reads as zero.
     */
    {
        const gchar *tuples = PQcmdTuples (res);

        self->last_changes = (tuples != NULL && *tuples != '\0')
            ? (gint) g_ascii_strtoll (tuples, NULL, 10)
            : 0;
    }

    PQclear (res);
    return TRUE;
}

static OrmDriverResult *
orm_postgres_driver_connection_query (OrmDriverConnection  *connection,
                                      const gchar          *sql,
                                      GList                *params,
                                      OrmQueryFlags         flags,
                                      GError              **error)
{
    OrmPostgresDriverConnection *self = ORM_POSTGRES_DRIVER_CONNECTION (connection);
    OrmPostgresDriverResult     *result;
    PGresult                    *res;
    gchar                      **param_strings;
    gint                         n_params;

    g_return_val_if_fail (self->conn != NULL, NULL);

    param_strings = orm_postgres_params_new (params, &n_params);

    /*
     * ORM_QUERY_FLAGS_STREAMING is accepted and ignored: PQexecParams
     * collects the whole result before returning, and single-row mode is
     * only reachable from the asynchronous send path.
     */
    res = PQexecParams (self->conn,
                        sql,
                        n_params,
                        NULL,        /* paramTypes - let the server infer */
                        (const gchar * const *) param_strings,
                        NULL,        /* paramLengths */
                        NULL,        /* paramFormats */
                        0);          /* resultFormat - text */

    orm_postgres_params_free (param_strings, n_params);

    if (PQresultStatus (res) != PGRES_TUPLES_OK &&
        PQresultStatus (res) != PGRES_COMMAND_OK)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                     "Query failed: %s", PQerrorMessage (self->conn));
        PQclear (res);
        return NULL;
    }

    result = g_object_new (ORM_TYPE_POSTGRES_DRIVER_RESULT, NULL);
    result->result = res;
    result->row_count = PQntuples (res);

    return ORM_DRIVER_RESULT (result);
}

static gint64
orm_postgres_driver_connection_get_last_insert_id (OrmDriverConnection *connection)
{
    /*
     * PostgreSQL has no connection-wide last insert id.  The value lives
     * in a sequence the server picked, and the only way to learn which one
     * without guessing is to ask the statement for it: INSERT ... RETURNING
     * id, read back through query().
     */
    return 0;
}

static gint
orm_postgres_driver_connection_get_changes (OrmDriverConnection *connection)
{
    OrmPostgresDriverConnection *self = ORM_POSTGRES_DRIVER_CONNECTION (connection);

    /*
     * PostgreSQL counts affected rows per result rather than per
     * connection, so the count is read off each PGresult (PQcmdTuples)
     * and cached here before that result is cleared.
     *
     * Callers depend on this being a real number rather than a
     * placeholder: an UPDATE that was meant to touch one row and touched
     * none, or four, is only detectable by asking how many it touched.
     */
    return self->last_changes;
}

static gboolean
orm_postgres_driver_connection_set_isolation_level (OrmDriverConnection  *connection,
                                                    OrmIsolationLevel     level,
                                                    gboolean              for_next_transaction,
                                                    GError              **error)
{
    const gchar      *words;
    g_autofree gchar *sql = NULL;

    words = orm_postgres_isolation_words (level);
    if (words == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                     "Unknown isolation level: %d", (gint) level);
        return FALSE;
    }

    /*
     * The per-transaction form must be the first statement inside the
     * transaction it governs; the session form stands on its own and
     * outlives it.
     */
    sql = g_strdup_printf (for_next_transaction
                           ? "SET TRANSACTION ISOLATION LEVEL %s"
                           : "SET SESSION CHARACTERISTICS AS TRANSACTION "
                             "ISOLATION LEVEL %s",
                           words);

    return orm_driver_connection_execute (connection, sql, NULL, error);
}

static gboolean
orm_postgres_driver_connection_interrupt (OrmDriverConnection  *connection,
                                          GError              **error)
{
    OrmPostgresDriverConnection *self = ORM_POSTGRES_DRIVER_CONNECTION (connection);
    gchar                        errbuf[256];

    if (self->conn == NULL || self->cancel == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                     "Connection is closed");
        return FALSE;
    }

    /*
     * Only PQcancel is safe here.  It touches nothing but the PGcancel
     * object, which is why libpq documents it as callable from another
     * thread or a signal handler, whereas PQgetCancel reads the PGconn a
     * query may be using -- so the PGcancel is taken once at open time and
     * kept.
     */
    errbuf[0] = '\0';

    if (PQcancel (self->cancel, errbuf, (gint) sizeof errbuf) == 0)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_EXECUTE,
                     "Failed to cancel the statement in flight: %s", errbuf);
        return FALSE;
    }

    return TRUE;
}

static void
orm_postgres_driver_connection_finalize (GObject *object)
{
    orm_postgres_driver_connection_close (ORM_DRIVER_CONNECTION (object));

    G_OBJECT_CLASS (orm_postgres_driver_connection_parent_class)->finalize (object);
}

static void
orm_postgres_driver_connection_class_init (OrmPostgresDriverConnectionClass *klass)
{
    GObjectClass             *object_class = G_OBJECT_CLASS (klass);
    OrmDriverConnectionClass *conn_class = ORM_DRIVER_CONNECTION_CLASS (klass);

    object_class->finalize = orm_postgres_driver_connection_finalize;

    conn_class->close = orm_postgres_driver_connection_close;
    conn_class->execute = orm_postgres_driver_connection_execute;
    conn_class->query = orm_postgres_driver_connection_query;
    conn_class->get_last_insert_id = orm_postgres_driver_connection_get_last_insert_id;
    conn_class->get_changes = orm_postgres_driver_connection_get_changes;
    conn_class->set_isolation_level = orm_postgres_driver_connection_set_isolation_level;
    conn_class->interrupt = orm_postgres_driver_connection_interrupt;

    /*
     * OrmPostgresDriverConnection::notice:
     * @self: The connection
     * @message: The server's message, with no trailing newline
     *
     * Emitted on the thread the statement is running on.  OrmConnection
     * marshals it into the owning main context before passing it on.
     */
    pg_signals[SIGNAL_NOTICE] =
        g_signal_new ("notice",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 1,
                      G_TYPE_STRING);
}

static void
orm_postgres_driver_connection_init (OrmPostgresDriverConnection *self)
{
    self->conn = NULL;
    self->cancel = NULL;
}

/* ------------------------------------------------------------------ */
/* Driver                                                             */
/* ------------------------------------------------------------------ */

struct _OrmPostgresDriver
{
    OrmDriver parent_instance;
};

G_DEFINE_FINAL_TYPE (OrmPostgresDriver, orm_postgres_driver, ORM_TYPE_DRIVER)

/*
 * Quotes a value for a libpq keyword/value connection string.
 *
 * libpq splits that string on whitespace, so a password containing a space
 * silently becomes a truncated password plus a garbage keyword.  Wrapping
 * in single quotes and backslash-escaping quotes and backslashes is the
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

static const gchar *
orm_postgres_driver_get_name (OrmDriver *driver)
{
    return "postgres";
}

static const gchar * const *
orm_postgres_driver_get_schemes (OrmDriver *driver)
{
    static const gchar * const schemes[] = { "postgresql", "postgres", NULL };

    return schemes;
}

static OrmDialectType
orm_postgres_driver_get_dialect_type (OrmDriver *driver)
{
    return ORM_DIALECT_POSTGRES;
}

static OrmDialect *
orm_postgres_driver_create_dialect (OrmDriver *driver)
{
    return ORM_DIALECT (orm_postgres_dialect_new ());
}

static OrmDriverConnection *
orm_postgres_driver_open (OrmDriver  *driver,
                          OrmEngine  *engine,
                          GError    **error)
{
    OrmPostgresDriverConnection *self;
    PGconn                      *conn;
    const gchar                 *host;
    const gchar                 *user;
    const gchar                 *pass;
    const gchar                 *dbname;
    gint                         port;
    g_autofree gchar            *conninfo = NULL;
    g_autofree gchar            *q_host = NULL;
    g_autofree gchar            *q_dbname = NULL;
    g_autofree gchar            *q_user = NULL;
    g_autofree gchar            *q_pass = NULL;

    host = orm_engine_get_host (engine);
    port = orm_engine_get_port (engine);
    user = orm_engine_get_username (engine);
    pass = orm_engine_get_password (engine);
    dbname = orm_engine_get_database (engine);

    /*
     * Every value is quoted: libpq splits the connection string on
     * whitespace, so an unquoted password with a space in it becomes a
     * truncated password and a stray keyword, and the resulting error says
     * nothing useful.
     */
    q_host = orm_conninfo_quote (host);
    q_dbname = orm_conninfo_quote (dbname);

    if (pass != NULL)
    {
        q_user = orm_conninfo_quote (user);
        q_pass = orm_conninfo_quote (pass);
        conninfo = g_strdup_printf ("host=%s port=%d dbname=%s user=%s password=%s",
                                    q_host, port, q_dbname, q_user, q_pass);
    }
    else if (user != NULL)
    {
        q_user = orm_conninfo_quote (user);
        conninfo = g_strdup_printf ("host=%s port=%d dbname=%s user=%s",
                                    q_host, port, q_dbname, q_user);
    }
    else
    {
        conninfo = g_strdup_printf ("host=%s port=%d dbname=%s",
                                    q_host, port, q_dbname);
    }

    conn = PQconnectdb (conninfo);

    if (PQstatus (conn) != CONNECTION_OK)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                     "Failed to connect to PostgreSQL: %s",
                     PQerrorMessage (conn));
        PQfinish (conn);
        return NULL;
    }

    self = g_object_new (ORM_TYPE_POSTGRES_DRIVER_CONNECTION, NULL);
    self->conn = conn;

    /*
     * The cancel object is taken now, while nothing else can be using the
     * connection: PQgetCancel reads the PGconn, so asking for it once a
     * query is in flight is the race that interrupt() exists to avoid.
     */
    self->cancel = PQgetCancel (conn);

    /*
     * Replaces libpq's default receiver, which writes to stderr.  Set
     * before the connection is handed out so no notice can arrive
     * unclaimed.
     */
    PQsetNoticeReceiver (conn, orm_postgres_notice_receiver, self);

    return ORM_DRIVER_CONNECTION (self);
}

static void
orm_postgres_driver_class_init (OrmPostgresDriverClass *klass)
{
    OrmDriverClass *driver_class = ORM_DRIVER_CLASS (klass);

    driver_class->get_name = orm_postgres_driver_get_name;
    driver_class->get_schemes = orm_postgres_driver_get_schemes;
    driver_class->get_dialect_type = orm_postgres_driver_get_dialect_type;
    driver_class->create_dialect = orm_postgres_driver_create_dialect;
    driver_class->open = orm_postgres_driver_open;
}

static void
orm_postgres_driver_init (OrmPostgresDriver *self)
{
}

/**
 * orm_postgres_driver_new:
 *
 * Creates the PostgreSQL driver.
 *
 * Returns: (transfer full): A new #OrmPostgresDriver
 */
OrmPostgresDriver *
orm_postgres_driver_new (void)
{
    return g_object_new (ORM_TYPE_POSTGRES_DRIVER, NULL);
}
