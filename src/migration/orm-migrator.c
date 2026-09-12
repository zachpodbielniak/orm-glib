/* orm-migrator.c
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

#include "orm-migrator.h"
#include <string.h>
#include "../engine/orm-engine.h"
#include "../engine/orm-engine-private.h"
#include "../core/orm-error.h"

struct _OrmMigration
{
    gint64 version;
    gchar *name;
    gchar *up;
    gchar *down;
    gchar *checksum;
    OrmMigrationFunc up_func;
    OrmMigrationFunc down_func;
};

G_DEFINE_BOXED_TYPE (OrmMigration, orm_migration,
                     orm_migration_copy, orm_migration_free)

OrmMigration *
orm_migration_new (gint64 version, const gchar *name,
                   const gchar *up, const gchar *down)
{
    OrmMigration *self;

    g_return_val_if_fail (version > 0, NULL);
    g_return_val_if_fail (name != NULL && *name != '\0', NULL);
    g_return_val_if_fail (up != NULL && *up != '\0', NULL);
    self = g_new0 (OrmMigration, 1);
    self->version = version;
    self->name = g_strdup (name);
    self->up = g_strdup (up);
    self->down = g_strdup (down);
    self->checksum = g_compute_checksum_for_string (G_CHECKSUM_SHA256, up, -1);
    return self;
}

OrmMigration *
orm_migration_new_callback (gint64 version, const gchar *name,
                            const gchar *up_text, OrmMigrationFunc up,
                            OrmMigrationFunc down)
{
    OrmMigration *self;

    g_return_val_if_fail (up != NULL, NULL);
    self = orm_migration_new (version, name, up_text, NULL);
    if (self != NULL)
    {
        self->up_func = up;
        self->down_func = down;
    }
    return self;
}

OrmMigration *
orm_migration_copy (const OrmMigration *self)
{
    OrmMigration *copy;

    g_return_val_if_fail (self != NULL, NULL);
    copy = orm_migration_new (self->version, self->name, self->up, self->down);
    copy->up_func = self->up_func;
    copy->down_func = self->down_func;
    return copy;
}

void
orm_migration_free (OrmMigration *self)
{
    if (self == NULL)
        return;
    g_free (self->name);
    g_free (self->up);
    g_free (self->down);
    g_free (self->checksum);
    g_free (self);
}

gint64
orm_migration_get_version (const OrmMigration *self)
{
    g_return_val_if_fail (self != NULL, 0);
    return self->version;
}

const gchar *
orm_migration_get_name (const OrmMigration *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->name;
}

struct _OrmMigrator
{
    GObject parent_instance;
    OrmConnection *connection;
    OrmDialect *dialect;
    GPtrArray *migrations;
    gboolean running;
};

G_DEFINE_TYPE (OrmMigrator, orm_migrator, G_TYPE_OBJECT)

static guint applied_signal;
static guint failed_signal;

static void
orm_migrator_finalize (GObject *object)
{
    OrmMigrator *self = ORM_MIGRATOR (object);

    g_clear_object (&self->connection);
    g_clear_object (&self->dialect);
    g_ptr_array_unref (self->migrations);
    G_OBJECT_CLASS (orm_migrator_parent_class)->finalize (object);
}

static void
orm_migrator_class_init (OrmMigratorClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = orm_migrator_finalize;
    applied_signal = g_signal_new ("migration-applied", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
        ORM_TYPE_MIGRATION, G_TYPE_BOOLEAN);
    failed_signal = g_signal_new ("migration-failed", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
        ORM_TYPE_MIGRATION, G_TYPE_ERROR);
}

static void
orm_migrator_init (OrmMigrator *self)
{
    self->migrations = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_migration_free);
}

OrmMigrator *
orm_migrator_new (OrmConnection *connection,
                  OrmMigration * const *migrations,
                  guint n_migrations, GError **error)
{
    OrmMigrator *self;
    guint i;

    g_return_val_if_fail (ORM_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (migrations != NULL || n_migrations == 0, NULL);
    for (i = 0; i < n_migrations; i++)
    {
        if (migrations[i] == NULL || (i > 0 &&
            migrations[i]->version <= migrations[i - 1]->version))
        {
            g_set_error_literal (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                                 "Migrations must have strictly increasing versions");
            return NULL;
        }
    }
    self = g_object_new (ORM_TYPE_MIGRATOR, NULL);
    self->connection = g_object_ref (connection);
    self->dialect = g_object_ref (orm_engine_get_dialect (
        orm_connection_get_engine (connection)));
    for (i = 0; i < n_migrations; i++)
        g_ptr_array_add (self->migrations, orm_migration_copy (migrations[i]));
    return self;
}

/* MySQL exposes TEXT through the existing driver's blob value path. */
static gboolean
text_matches (OrmRow *row, gint column, const gchar *expected)
{
    OrmValue *value = orm_row_get_value (row, column);

    if (value == NULL)
        return FALSE;
    if (orm_value_get_value_type (value) == ORM_VALUE_STRING)
        return g_strcmp0 (expected, orm_value_get_string (value)) == 0;
    if (orm_value_get_value_type (value) == ORM_VALUE_BLOB)
    {
        g_autoptr(GBytes) bytes = g_bytes_new (expected, strlen (expected));
        return g_bytes_equal (bytes, orm_value_get_blob (value));
    }
    return FALSE;
}

/* Validate the entire history, including versions beyond an up target. A
 * history must be a prefix: missing definitions are never treated as pending. */
static gboolean
read_history (OrmMigrator *self, guint *count, GError **error)
{
    g_autoptr(OrmResult) result = NULL;

    *count = 0;
    result = orm_connection_query (self->connection,
        "SELECT version, name, checksum FROM schema_migrations ORDER BY version", error);
    if (result == NULL)
        return FALSE;
    while (orm_result_next (result))
    {
        OrmRow *row = orm_result_get_row (result);
        OrmMigration *migration = *count < self->migrations->len ?
            g_ptr_array_index (self->migrations, *count) : NULL;

        if (migration == NULL || migration->version != orm_row_get_integer (row, 0) ||
            !text_matches (row, 1, migration->name) ||
            !text_matches (row, 2, migration->checksum))
        {
            g_set_error (error, ORM_ERROR, ORM_ERROR_INTEGRITY_ERROR,
                         "Migration history/checksum mismatch at version %" G_GINT64_FORMAT,
                         orm_row_get_integer (row, 0));
            return FALSE;
        }
        (*count)++;
    }
    if (orm_result_get_error (result) != NULL)
    {
        g_propagate_error (error, g_error_copy (orm_result_get_error (result)));
        return FALSE;
    }
    return TRUE;
}

static gboolean
record_migration (OrmMigrator *self, OrmMigration *migration,
                  gboolean down, GError **error)
{
    g_autoptr(OrmValue) version = orm_value_new_integer (migration->version);
    g_autoptr(OrmValue) name = orm_value_new_string (migration->name);
    g_autoptr(OrmValue) checksum = orm_value_new_string (migration->checksum);
    GList *params = NULL;
    const gchar *sql;
    gboolean postgres = orm_dialect_get_dialect_type (self->dialect) == ORM_DIALECT_POSTGRES;
    gboolean ok;

    params = g_list_append (params, version);
    if (down)
        sql = postgres ? "DELETE FROM schema_migrations WHERE version = $1" :
                         "DELETE FROM schema_migrations WHERE version = ?";
    else
    {
        params = g_list_append (params, name);
        params = g_list_append (params, checksum);
        sql = postgres ?
            "INSERT INTO schema_migrations (version, name, checksum, applied_at) "
            "VALUES ($1, $2, $3, CURRENT_TIMESTAMP)" :
            "INSERT INTO schema_migrations (version, name, checksum, applied_at) "
            "VALUES (?, ?, ?, CURRENT_TIMESTAMP)";
    }
    ok = orm_connection_execute_with_params (self->connection, sql, params, error);
    g_list_free (params);
    return ok;
}

static gboolean
begin_migration (OrmMigrator *self, OrmDialectType type, GError **error)
{
    if (type == ORM_DIALECT_MYSQL)
        return TRUE;
    if (!orm_connection_execute (self->connection,
            type == ORM_DIALECT_SQLITE ? "BEGIN IMMEDIATE" : "BEGIN", error))
        return FALSE;
    orm_connection_set_in_transaction (self->connection, TRUE);
    return TRUE;
}

static gboolean
end_migration (OrmMigrator *self, OrmDialectType type,
               gboolean commit, GError **error)
{
    gboolean ok;

    if (type == ORM_DIALECT_MYSQL)
        return TRUE;
    ok = orm_connection_execute (self->connection, commit ? "COMMIT" : "ROLLBACK", error);
    if (ok)
        orm_connection_set_in_transaction (self->connection, FALSE);
    return ok;
}

static gboolean
run (OrmMigrator *self, gint64 target, gint direction,
     GArray **applied, GArray **pending, GError **error)
{
    g_autoptr(GError) local_error = NULL;
    g_autoptr(OrmConnection) locker = NULL;
    g_autofree gchar *restore_timeout = NULL;
    OrmDialectType type;
    OrmMigration *migration = NULL;
    guint limit = 0;
    guint count = 0;
    guint i;
    gboolean locked = FALSE;
    gboolean transaction = FALSE;
    gboolean ok = FALSE;

    g_return_val_if_fail (ORM_IS_MIGRATOR (self), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
    if (self->running || !orm_connection_is_open (self->connection) ||
        orm_connection_in_transaction (self->connection))
    {
        g_set_error_literal (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                             "Migrator requires an idle, open connection without a transaction");
        return FALSE;
    }
    for (i = 0; i < self->migrations->len; i++)
    {
        OrmMigration *item = g_ptr_array_index (self->migrations, i);
        if (item->version == target)
            limit = i + 1;
    }
    if (target < 0 || (target != 0 && limit == 0))
    {
        g_set_error_literal (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                             "Unknown migration target");
        return FALSE;
    }
    if (target == 0 && direction > 0)
        limit = self->migrations->len;
    self->running = TRUE;
    type = orm_dialect_get_dialect_type (self->dialect);
    if (type == ORM_DIALECT_POSTGRES)
    {
        /* Session lock survives the commit between migrations. */
        if (!orm_connection_execute (self->connection,
                "SELECT pg_advisory_lock(1869770087, 1835624306)", &local_error))
            goto out;
        locked = TRUE;
    }
    else if (type == ORM_DIALECT_MYSQL)
    {
        /* DDL on the working connection must not release this table lock. */
        locker = orm_engine_connect (orm_connection_get_engine (self->connection), &local_error);
        if (locker == NULL || !orm_connection_execute (locker,
                "CREATE TABLE IF NOT EXISTS schema_migrations_lock (id INTEGER)", &local_error) ||
            !orm_connection_execute (locker,
                "LOCK TABLES schema_migrations_lock WRITE", &local_error))
            goto out;
        locked = TRUE;
    }
    else if (type == ORM_DIALECT_SQLITE)
    {
        g_autoptr(OrmResult) result = orm_connection_query (
            self->connection, "PRAGMA busy_timeout", &local_error);
        g_autoptr(OrmValue) value = NULL;

        if (result == NULL)
            goto out;
        value = orm_result_get_scalar (result);
        if (value == NULL)
        {
            g_set_error_literal (&local_error, ORM_ERROR, ORM_ERROR_QUERY_FAILED,
                                 "Cannot read SQLite busy timeout");
            goto out;
        }
        restore_timeout = g_strdup_printf ("PRAGMA busy_timeout = %" G_GINT64_FORMAT,
                                             orm_value_get_integer (value));
        if (!orm_connection_execute (self->connection, "PRAGMA busy_timeout = 30000", &local_error))
            goto out;
    }
    else
    {
        g_set_error_literal (&local_error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                             "Migration locking is unsupported for this dialect");
        goto out;
    }
    if (!begin_migration (self, type, &local_error))
        goto out;
    transaction = type != ORM_DIALECT_MYSQL;
    if (!orm_connection_execute (self->connection,
            "CREATE TABLE IF NOT EXISTS schema_migrations ("
            "version BIGINT PRIMARY KEY, name TEXT NOT NULL, "
            "checksum VARCHAR(64) NOT NULL, applied_at TIMESTAMP NOT NULL)", &local_error) ||
        !end_migration (self, type, TRUE, &local_error))
        goto out;
    transaction = FALSE;
    for (;;)
    {
        OrmMigrationFunc func;
        const gchar *sql;

        if (!begin_migration (self, type, &local_error))
            goto out;
        transaction = type != ORM_DIALECT_MYSQL;
        if (!read_history (self, &count, &local_error))
            goto out;
        if (direction == 0 || (direction > 0 ? count >= limit : count <= limit))
        {
            if (!end_migration (self, type, TRUE, &local_error))
                goto out;
            transaction = FALSE;
            break;
        }
        migration = g_ptr_array_index (self->migrations, direction > 0 ? count : count - 1);
        func = direction > 0 ? migration->up_func : migration->down_func;
        sql = direction > 0 ? migration->up : migration->down;
        if (func == NULL && sql == NULL)
        {
            g_set_error (&local_error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                         "Migration %" G_GINT64_FORMAT " has no down operation", migration->version);
            goto out;
        }
        if (!(func != NULL ? func (self->connection, self->dialect, &local_error) :
                             orm_connection_execute (self->connection, sql, &local_error)) ||
            !record_migration (self, migration, direction < 0, &local_error) ||
            !end_migration (self, type, TRUE, &local_error))
            goto out;
        transaction = FALSE;
        g_signal_emit (self, applied_signal, 0, migration, direction < 0);
        migration = NULL;
    }
    ok = TRUE;
out:
    if (transaction && !end_migration (self, type, FALSE, NULL))
        orm_connection_close (self->connection);
    if (locked && type == ORM_DIALECT_POSTGRES)
    {
        g_autoptr(GError) unlock_error = NULL;
        if (!orm_connection_execute (self->connection,
                "SELECT pg_advisory_unlock(1869770087, 1835624306)", &unlock_error))
        {
            orm_connection_close (self->connection);
            if (local_error == NULL)
                local_error = g_steal_pointer (&unlock_error);
            ok = FALSE;
        }
    }
    /* Closing the dedicated MySQL session releases its table lock. */
    if (locker != NULL)
        orm_connection_close (locker);
    if (restore_timeout != NULL && orm_connection_is_open (self->connection))
    {
        g_autoptr(GError) restore_error = NULL;
        if (!orm_connection_execute (self->connection, restore_timeout, &restore_error))
        {
            if (local_error == NULL)
                local_error = g_steal_pointer (&restore_error);
            ok = FALSE;
        }
    }
    if (!ok)
    {
        if (local_error == NULL)
            g_set_error_literal (&local_error, ORM_ERROR, ORM_ERROR_EXECUTE,
                                 "Migration callback failed without an error");
        if (migration != NULL)
            g_signal_emit (self, failed_signal, 0, migration, local_error);
        g_propagate_error (error, g_steal_pointer (&local_error));
    }
    else if (direction == 0)
    {
        *applied = g_array_new (FALSE, FALSE, sizeof (gint64));
        *pending = g_array_new (FALSE, FALSE, sizeof (gint64));
        for (i = 0; i < self->migrations->len; i++)
        {
            OrmMigration *item = g_ptr_array_index (self->migrations, i);
            g_array_append_val (i < count ? *applied : *pending, item->version);
        }
    }
    self->running = FALSE;
    return ok;
}

gboolean
orm_migrator_status (OrmMigrator *self, GArray **applied,
                     GArray **pending, GError **error)
{
    g_return_val_if_fail (applied != NULL && pending != NULL, FALSE);
    *applied = NULL;
    *pending = NULL;
    return run (self, 0, 0, applied, pending, error);
}

gboolean
orm_migrator_up (OrmMigrator *self, gint64 target, GError **error)
{
    return run (self, target, 1, NULL, NULL, error);
}

gboolean
orm_migrator_down (OrmMigrator *self, gint64 target, GError **error)
{
    return run (self, target, -1, NULL, NULL, error);
}
