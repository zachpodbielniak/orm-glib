/* orm-engine.c
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

#include "orm-engine.h"
#include "orm-connection.h"
#include "../core/orm-error.h"
#include "../schema/orm-metadata.h"
#include "../dialect/orm-ddl-compiler.h"

#include <string.h>

/*
 * OrmEngine - Database connection factory.
 *
 * The engine parses connection URLs and creates connections
 * to the appropriate database backend.
 */

struct _OrmEngine
{
    GObject parent_instance;

    gchar          *url;
    gchar          *database_path;    /* For SQLite: file path */
    gchar          *host;             /* For network DBs */
    gint            port;             /* For network DBs */
    gchar          *username;         /* For network DBs */
    gchar          *password;         /* For network DBs */
    gchar          *database;         /* Database name */
    OrmDialect     *dialect;
    OrmDialectType  dialect_type;
};

G_DEFINE_TYPE (OrmEngine, orm_engine, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_URL,
    PROP_DIALECT_TYPE,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

/*
 * Parse a database URL and extract components.
 *
 * Supported formats:
 *   sqlite:///path/to/db.sqlite
 *   sqlite:///:memory:
 *   postgresql://user:pass@host:port/database
 *   mysql://user:pass@host:port/database
 */
static gboolean
orm_engine_parse_url (OrmEngine    *self,
                      const gchar  *url,
                      GError      **error)
{
    const gchar *p;
    const gchar *scheme_end;
    g_autofree gchar *scheme = NULL;

    g_return_val_if_fail (url != NULL, FALSE);

    /* Find scheme (driver) */
    scheme_end = strstr (url, "://");
    if (scheme_end == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_URL,
                     "Invalid URL: missing '://' scheme separator");
        return FALSE;
    }

    scheme = g_strndup (url, scheme_end - url);
    p = scheme_end + 3;  /* Skip :// */

    /* Determine dialect type from scheme */
    if (g_strcmp0 (scheme, "sqlite") == 0)
    {
        self->dialect_type = ORM_DIALECT_SQLITE;

        /* SQLite URL format: sqlite:///path or sqlite:///:memory: */
        if (g_strcmp0 (p, "/:memory:") == 0 || g_strcmp0 (p, ":memory:") == 0)
        {
            /* In-memory database */
            self->database_path = g_strdup (":memory:");
        }
        else if (*p == '/')
        {
            /* Absolute path: sqlite:///absolute/path */
            self->database_path = g_strdup (p);
        }
        else
        {
            /* Relative path */
            self->database_path = g_strdup (p);
        }
    }
    else if (g_strcmp0 (scheme, "postgresql") == 0 ||
             g_strcmp0 (scheme, "postgres") == 0)
    {
        self->dialect_type = ORM_DIALECT_POSTGRES;

#ifndef ORM_ENABLE_POSTGRES
        g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                     "PostgreSQL support not compiled in. "
                     "Rebuild with ENABLE_POSTGRES=1");
        return FALSE;
#else
        /*
         * Parse PostgreSQL URL: postgresql://user:pass@host:port/database
         * The format after :// is: [user[:password]@]host[:port]/database
         */
        {
            const gchar *at_sign;
            const gchar *host_start;
            const gchar *colon;
            const gchar *slash;
            const gchar *port_start;

            at_sign = strchr (p, '@');
            if (at_sign != NULL)
            {
                /* Extract user:password */
                colon = strchr (p, ':');

                if (colon != NULL && colon < at_sign)
                {
                    /* Have both user and password */
                    self->username = g_strndup (p, colon - p);
                    self->password = g_strndup (colon + 1, at_sign - colon - 1);
                }
                else
                {
                    /* Only user, no password */
                    self->username = g_strndup (p, at_sign - p);
                    self->password = NULL;
                }
                host_start = at_sign + 1;
            }
            else
            {
                /* No credentials */
                self->username = NULL;
                self->password = NULL;
                host_start = p;
            }

            /* Parse host:port/database */
            slash = strchr (host_start, '/');
            if (slash == NULL)
            {
                g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_URL,
                             "Invalid PostgreSQL URL: missing database name");
                return FALSE;
            }

            /* Extract database name */
            self->database = g_strdup (slash + 1);

            /* Check for port */
            colon = strchr (host_start, ':');
            if (colon != NULL && colon < slash)
            {
                /* Have port */
                self->host = g_strndup (host_start, colon - host_start);
                port_start = colon + 1;
                self->port = (gint) g_ascii_strtoll (port_start, NULL, 10);
            }
            else
            {
                /* No port, use default */
                self->host = g_strndup (host_start, slash - host_start);
                self->port = 5432;
            }
        }
#endif
    }
    else if (g_strcmp0 (scheme, "mysql") == 0 ||
             g_strcmp0 (scheme, "mariadb") == 0)
    {
        self->dialect_type = ORM_DIALECT_MYSQL;

#ifndef ORM_ENABLE_MYSQL
        g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                     "MySQL/MariaDB support not compiled in. "
                     "Rebuild with ENABLE_MYSQL=1");
        return FALSE;
#else
        /*
         * Parse MySQL URL: mysql://user:pass@host:port/database
         * The format after :// is: [user[:password]@]host[:port]/database
         */
        {
            const gchar *at_sign;
            const gchar *host_start;
            const gchar *colon;
            const gchar *slash;
            const gchar *port_start;

            at_sign = strchr (p, '@');
            if (at_sign != NULL)
            {
                /* Extract user:password */
                colon = strchr (p, ':');

                if (colon != NULL && colon < at_sign)
                {
                    /* Have both user and password */
                    self->username = g_strndup (p, colon - p);
                    self->password = g_strndup (colon + 1, at_sign - colon - 1);
                }
                else
                {
                    /* Only user, no password */
                    self->username = g_strndup (p, at_sign - p);
                    self->password = NULL;
                }
                host_start = at_sign + 1;
            }
            else
            {
                /* No credentials */
                self->username = NULL;
                self->password = NULL;
                host_start = p;
            }

            /* Parse host:port/database */
            slash = strchr (host_start, '/');
            if (slash == NULL)
            {
                g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_URL,
                             "Invalid MySQL URL: missing database name");
                return FALSE;
            }

            /* Extract database name */
            self->database = g_strdup (slash + 1);

            /* Check for port */
            colon = strchr (host_start, ':');
            if (colon != NULL && colon < slash)
            {
                /* Have port */
                self->host = g_strndup (host_start, colon - host_start);
                port_start = colon + 1;
                self->port = (gint) g_ascii_strtoll (port_start, NULL, 10);
            }
            else
            {
                /* No port, use default */
                self->host = g_strndup (host_start, slash - host_start);
                self->port = 3306;
            }
        }
#endif
    }
    else
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_URL,
                     "Unknown database scheme: %s", scheme);
        return FALSE;
    }

    self->url = g_strdup (url);
    return TRUE;
}

static void
orm_engine_finalize (GObject *object)
{
    OrmEngine *self = ORM_ENGINE (object);

    g_clear_pointer (&self->url, g_free);
    g_clear_pointer (&self->database_path, g_free);
    g_clear_pointer (&self->host, g_free);
    g_clear_pointer (&self->username, g_free);
    g_clear_pointer (&self->password, g_free);
    g_clear_pointer (&self->database, g_free);
    g_clear_object (&self->dialect);

    G_OBJECT_CLASS (orm_engine_parent_class)->finalize (object);
}

static void
orm_engine_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    OrmEngine *self = ORM_ENGINE (object);

    switch (prop_id)
    {
    case PROP_URL:
        g_value_set_string (value, self->url);
        break;
    case PROP_DIALECT_TYPE:
        g_value_set_int (value, self->dialect_type);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_engine_class_init (OrmEngineClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_engine_finalize;
    object_class->get_property = orm_engine_get_property;

    properties[PROP_URL] =
        g_param_spec_string ("url",
                             "URL",
                             "Database connection URL",
                             NULL,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_DIALECT_TYPE] =
        g_param_spec_int ("dialect-type",
                          "Dialect Type",
                          "Database dialect type",
                          0, 2, 0,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
orm_engine_init (OrmEngine *self)
{
    self->url = NULL;
    self->database_path = NULL;
    self->host = NULL;
    self->port = 0;
    self->username = NULL;
    self->password = NULL;
    self->database = NULL;
    self->dialect = NULL;
    self->dialect_type = ORM_DIALECT_SQLITE;
}

/**
 * orm_engine_new:
 * @url: Database connection URL
 * @error: Return location for error
 *
 * Creates a new database engine from a connection URL.
 *
 * Supported URL formats:
 * - `sqlite:///path/to/database.db` - SQLite file database
 * - `sqlite:///:memory:` - SQLite in-memory database
 *
 * Returns: (transfer full) (nullable): A new #OrmEngine, or %NULL on error
 */
OrmEngine *
orm_engine_new (const gchar  *url,
                GError      **error)
{
    OrmEngine *self;

    g_return_val_if_fail (url != NULL, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    self = g_object_new (ORM_TYPE_ENGINE, NULL);

    if (!orm_engine_parse_url (self, url, error))
    {
        g_object_unref (self);
        return NULL;
    }

    /* Create dialect */
    self->dialect = orm_dialect_for_type (self->dialect_type);
    if (self->dialect == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                     "Dialect not available for type %d", self->dialect_type);
        g_object_unref (self);
        return NULL;
    }

    return self;
}

/**
 * orm_engine_new_sqlite:
 * @path: Path to SQLite database file, or ":memory:"
 * @error: Return location for error
 *
 * Creates a new SQLite database engine. This is a convenience
 * function equivalent to `orm_engine_new("sqlite:///" + path)`.
 *
 * Returns: (transfer full) (nullable): A new #OrmEngine, or %NULL on error
 */
OrmEngine *
orm_engine_new_sqlite (const gchar  *path,
                       GError      **error)
{
    g_autofree gchar *url = NULL;

    g_return_val_if_fail (path != NULL, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    if (g_strcmp0 (path, ":memory:") == 0)
    {
        url = g_strdup ("sqlite:///:memory:");
    }
    else if (path[0] == '/')
    {
        /* Absolute path */
        url = g_strdup_printf ("sqlite://%s", path);
    }
    else
    {
        /* Relative path - prefix with current directory */
        url = g_strdup_printf ("sqlite:///%s", path);
    }

    return orm_engine_new (url, error);
}

/**
 * orm_engine_get_dialect:
 * @self: An #OrmEngine
 *
 * Gets the dialect for this engine.
 *
 * Returns: (transfer none): The dialect
 */
OrmDialect *
orm_engine_get_dialect (OrmEngine *self)
{
    g_return_val_if_fail (ORM_IS_ENGINE (self), NULL);
    return self->dialect;
}

/**
 * orm_engine_get_dialect_type:
 * @self: An #OrmEngine
 *
 * Gets the dialect type for this engine.
 *
 * Returns: The dialect type
 */
OrmDialectType
orm_engine_get_dialect_type (OrmEngine *self)
{
    g_return_val_if_fail (ORM_IS_ENGINE (self), ORM_DIALECT_SQLITE);
    return self->dialect_type;
}

/**
 * orm_engine_get_url:
 * @self: An #OrmEngine
 *
 * Gets the connection URL.
 *
 * Returns: (transfer none): The URL
 */
const gchar *
orm_engine_get_url (OrmEngine *self)
{
    g_return_val_if_fail (ORM_IS_ENGINE (self), NULL);
    return self->url;
}

/**
 * orm_engine_connect:
 * @self: An #OrmEngine
 * @error: Return location for error
 *
 * Creates a new connection to the database.
 *
 * Returns: (transfer full) (nullable): A new #OrmConnection, or %NULL on error
 */
OrmConnection *
orm_engine_connect (OrmEngine  *self,
                    GError    **error)
{
    g_return_val_if_fail (ORM_IS_ENGINE (self), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    return orm_connection_new (self, error);
}

/**
 * orm_engine_execute:
 * @self: An #OrmEngine
 * @sql: SQL statement to execute
 * @error: Return location for error
 *
 * Executes a SQL statement using a temporary connection.
 * The connection is automatically closed after execution.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
orm_engine_execute (OrmEngine    *self,
                    const gchar  *sql,
                    GError      **error)
{
    g_autoptr(OrmConnection) conn = NULL;

    g_return_val_if_fail (ORM_IS_ENGINE (self), FALSE);
    g_return_val_if_fail (sql != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    conn = orm_engine_connect (self, error);
    if (conn == NULL)
    {
        return FALSE;
    }

    return orm_connection_execute (conn, sql, error);
}

/**
 * orm_engine_create_all:
 * @self: An #OrmEngine
 * @metadata: Schema metadata containing tables to create
 * @error: Return location for error
 *
 * Creates all tables defined in the metadata. Uses CREATE TABLE IF NOT EXISTS.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
orm_engine_create_all (OrmEngine    *self,
                       gpointer      metadata,
                       GError      **error)
{
    OrmMetadata *md;
    GList *tables;
    GList *l;
    OrmDdlCompiler *ddl;
    g_autoptr(OrmConnection) conn = NULL;

    g_return_val_if_fail (ORM_IS_ENGINE (self), FALSE);
    g_return_val_if_fail (ORM_IS_METADATA (metadata), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    md = ORM_METADATA (metadata);
    ddl = ORM_DDL_COMPILER (orm_dialect_get_ddl_compiler (self->dialect));

    conn = orm_engine_connect (self, error);
    if (conn == NULL)
    {
        return FALSE;
    }

    tables = orm_metadata_get_tables (md);
    for (l = tables; l != NULL; l = l->next)
    {
        OrmTable *table = ORM_TABLE (l->data);
        g_autofree gchar *sql = NULL;

        sql = orm_ddl_compiler_compile_create_table (ddl, table, TRUE);
        if (!orm_connection_execute (conn, sql, error))
        {
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * orm_engine_drop_all:
 * @self: An #OrmEngine
 * @metadata: Schema metadata containing tables to drop
 * @error: Return location for error
 *
 * Drops all tables defined in the metadata. Uses DROP TABLE IF EXISTS.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
orm_engine_drop_all (OrmEngine    *self,
                     gpointer      metadata,
                     GError      **error)
{
    OrmMetadata *md;
    GList *tables;
    GList *l;
    OrmDdlCompiler *ddl;
    g_autoptr(OrmConnection) conn = NULL;

    g_return_val_if_fail (ORM_IS_ENGINE (self), FALSE);
    g_return_val_if_fail (ORM_IS_METADATA (metadata), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    md = ORM_METADATA (metadata);
    ddl = ORM_DDL_COMPILER (orm_dialect_get_ddl_compiler (self->dialect));

    conn = orm_engine_connect (self, error);
    if (conn == NULL)
    {
        return FALSE;
    }

    /* Drop in reverse order to handle foreign key dependencies */
    tables = g_list_copy (orm_metadata_get_tables (md));
    tables = g_list_reverse (tables);

    for (l = tables; l != NULL; l = l->next)
    {
        OrmTable *table = ORM_TABLE (l->data);
        g_autofree gchar *sql = NULL;

        sql = orm_ddl_compiler_compile_drop_table (ddl, table, TRUE, FALSE);
        if (!orm_connection_execute (conn, sql, error))
        {
            g_list_free (tables);
            return FALSE;
        }
    }

    g_list_free (tables);
    return TRUE;
}

/*
 * orm_engine_get_database_path:
 * @self: An #OrmEngine
 *
 * Gets the database path (SQLite only).
 *
 * Returns: (transfer none): The database path
 */
const gchar *
orm_engine_get_database_path (OrmEngine *self)
{
    g_return_val_if_fail (ORM_IS_ENGINE (self), NULL);
    return self->database_path;
}

/*
 * orm_engine_get_host:
 * @self: An #OrmEngine
 *
 * Gets the database host (PostgreSQL/MySQL only).
 *
 * Returns: (transfer none): The host
 */
const gchar *
orm_engine_get_host (OrmEngine *self)
{
    g_return_val_if_fail (ORM_IS_ENGINE (self), NULL);
    return self->host;
}

/*
 * orm_engine_get_port:
 * @self: An #OrmEngine
 *
 * Gets the database port (PostgreSQL/MySQL only).
 *
 * Returns: The port number
 */
gint
orm_engine_get_port (OrmEngine *self)
{
    g_return_val_if_fail (ORM_IS_ENGINE (self), 0);
    return self->port;
}

/*
 * orm_engine_get_username:
 * @self: An #OrmEngine
 *
 * Gets the database username (PostgreSQL/MySQL only).
 *
 * Returns: (transfer none): The username
 */
const gchar *
orm_engine_get_username (OrmEngine *self)
{
    g_return_val_if_fail (ORM_IS_ENGINE (self), NULL);
    return self->username;
}

/*
 * orm_engine_get_password:
 * @self: An #OrmEngine
 *
 * Gets the database password (PostgreSQL/MySQL only).
 *
 * Returns: (transfer none): The password
 */
const gchar *
orm_engine_get_password (OrmEngine *self)
{
    g_return_val_if_fail (ORM_IS_ENGINE (self), NULL);
    return self->password;
}

/*
 * orm_engine_get_database:
 * @self: An #OrmEngine
 *
 * Gets the database name (PostgreSQL/MySQL only).
 *
 * Returns: (transfer none): The database name
 */
const gchar *
orm_engine_get_database (OrmEngine *self)
{
    g_return_val_if_fail (ORM_IS_ENGINE (self), NULL);
    return self->database;
}
