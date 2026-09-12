/* test-migration.c
 *
 * Copyright 2025 Zach Pobiel
 *
 * This file is part of orm-glib.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "test-fixtures.h"
#include <glib/gstdio.h>
#include <sys/wait.h>

static const gchar *test_program;
static const gchar *test_url;
static gboolean     delay_create;

static gboolean
create_table (OrmConnection  *connection,
              OrmDialect     *dialect,
              GError        **error)
{
    g_autoptr(OrmTable) table = orm_table_new ("migration_items", NULL);
    g_autoptr(OrmInteger) integer = orm_integer_new ();
    g_autoptr(OrmColumn) column = orm_column_new ("id", ORM_SQL_TYPE (integer));
    g_autofree gchar *sql = NULL;

    orm_table_add_column (table, column);
    sql = orm_ddl_compiler_compile_create_table (
        orm_dialect_get_ddl_compiler (dialect), table, FALSE);
    if (delay_create)
        g_usleep (200000);
    return orm_connection_execute (connection, sql, error);
}

static gboolean
fail_after_ddl (OrmConnection  *connection,
                OrmDialect     *dialect,
                GError        **error)
{
    (void) dialect;

    if (!orm_connection_execute (connection,
            "CREATE TABLE migration_partial (id INTEGER)", error))
        return FALSE;
    return orm_connection_execute (connection,
        "INSERT INTO migration_missing VALUES (1)", error);
}

static OrmMigrator *
new_migrator (OrmConnection *connection,
              gboolean       callback,
              gboolean       failing,
              gboolean       tampered)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmMigration) first = NULL;
    g_autoptr(OrmMigration) second = NULL;
    OrmMigration *migrations[2];
    OrmMigrator *migrator;

    first = callback ? orm_migration_new_callback (1, "items", "items-v1",
                                                   create_table, NULL) :
        orm_migration_new (1, "items",
            tampered ? "CREATE TABLE migration_items (id BIGINT)" :
                       "CREATE TABLE migration_items (id INTEGER)",
            "DROP TABLE migration_items");
    second = failing ? orm_migration_new_callback (2, "broken", "broken-v1",
                                                    fail_after_ddl, NULL) :
        orm_migration_new (2, "seed", "INSERT INTO migration_items VALUES (7)",
                            "DELETE FROM migration_items WHERE id = 7");
    migrations[0] = first;
    migrations[1] = second;
    migrator = orm_migrator_new (connection, migrations, 2, &error);
    g_assert_no_error (error);
    g_assert_nonnull (migrator);
    return migrator;
}

static gint64
scalar (OrmConnection *connection,
        const gchar   *sql)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmResult) result = orm_connection_query (connection, sql, &error);
    g_autoptr(OrmValue) value = NULL;

    g_assert_no_error (error);
    g_assert_nonnull (result);
    value = orm_result_get_scalar (result);
    g_assert_nonnull (value);
    return orm_value_get_integer (value);
}

static void
drop_bookkeeping (OrmConnection *connection)
{
    g_autoptr(GError) error = NULL;

    g_assert_true (orm_connection_execute (connection,
        "DROP TABLE IF EXISTS schema_migrations", &error));
    g_assert_true (orm_connection_execute (connection,
        "DROP TABLE IF EXISTS migration_items", &error));
    g_assert_true (orm_connection_execute (connection,
        "DROP TABLE IF EXISTS migration_partial", &error));
    g_assert_true (orm_connection_execute (connection,
        "DROP TABLE IF EXISTS schema_migrations_lock", &error));
    g_assert_no_error (error);
}

static void
check_status (OrmMigrator *migrator,
              guint        count)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GArray) applied = NULL;
    g_autoptr(GArray) pending = NULL;
    guint i;

    g_assert_true (orm_migrator_status (migrator, &applied, &pending, &error));
    g_assert_no_error (error);
    g_assert_cmpuint (applied->len, ==, count);
    g_assert_cmpuint (pending->len, ==, 2 - count);
    for (i = 0; i < count; i++)
        g_assert_cmpint (g_array_index (applied, gint64, i), ==, i + 1);
    for (i = 0; i < pending->len; i++)
        g_assert_cmpint (g_array_index (pending, gint64, i), ==, count + i + 1);
}

static void
count_applied (OrmMigrator  *migrator,
               OrmMigration *migration,
               gboolean      down,
               guint        *count)
{
    (void) migrator;
    (void) migration;
    (void) down;
    (*count)++;
}

static void
count_failed (OrmMigrator  *migrator,
              OrmMigration *migration,
              GError       *error,
              guint        *count)
{
    (void) migrator;
    g_assert_nonnull (error);
    g_assert_cmpint (orm_migration_get_version (migration), ==, 2);
    (*count)++;
}

static void
test_migration_boxed (void)
{
    const gchar *up = "CREATE TABLE migration_items (id INTEGER)";
    g_autofree gchar *expected = g_compute_checksum_for_string (G_CHECKSUM_SHA256,
                                                                up, -1);
    g_autoptr(OrmMigration) original = orm_migration_new (1, "items", up,
                                                          "DROP TABLE migration_items");
    g_autoptr(OrmMigration) copy = NULL;

    g_assert_cmpint (orm_migration_get_version (original), ==, 1);
    g_assert_cmpstr (orm_migration_get_name (original), ==, "items");
    g_assert_cmpstr (orm_migration_get_checksum (original), ==, expected);
    copy = orm_migration_copy (original);
    g_assert_cmpint (orm_migration_get_version (copy), ==, 1);
    g_assert_cmpstr (orm_migration_get_name (copy), ==, "items");
    g_assert_cmpstr (orm_migration_get_checksum (copy), ==, expected);
    orm_migration_free (NULL);
}

static void
test_migration_rejects_order (void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmEngine) engine = orm_engine_new (test_url, &error);
    g_autoptr(OrmConnection) connection = NULL;
    g_autoptr(OrmMigration) first = orm_migration_new (2, "later",
        "CREATE TABLE migration_items (id INTEGER)", NULL);
    g_autoptr(OrmMigration) second = orm_migration_new (1, "earlier",
        "CREATE TABLE migration_other (id INTEGER)", NULL);
    OrmMigration *migrations[2];
    g_autoptr(OrmMigrator) migrator = NULL;

    g_assert_no_error (error);
    connection = orm_engine_connect (engine, &error);
    g_assert_no_error (error);
    migrations[0] = first;
    migrations[1] = second;
    migrator = orm_migrator_new (connection, migrations, 2, &error);
    g_assert_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION);
    g_assert_null (migrator);
}

static void
test_migration_unknown_target (void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmEngine) engine = orm_engine_new (test_url, &error);
    g_autoptr(OrmConnection) connection = NULL;
    g_autoptr(OrmMigrator) migrator = NULL;

    g_assert_no_error (error);
    connection = orm_engine_connect (engine, &error);
    g_assert_no_error (error);
    drop_bookkeeping (connection);
    migrator = new_migrator (connection, FALSE, FALSE, FALSE);
    g_assert_false (orm_migrator_up (migrator, 99, &error));
    g_assert_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION);
}

static void
test_migration_irreversible_down (void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmEngine) engine = orm_engine_new (test_url, &error);
    g_autoptr(OrmConnection) connection = NULL;
    g_autoptr(OrmMigration) first = NULL;
    OrmMigration *migrations[1];
    g_autoptr(OrmMigrator) migrator = NULL;

    g_assert_no_error (error);
    connection = orm_engine_connect (engine, &error);
    g_assert_no_error (error);
    drop_bookkeeping (connection);
    first = orm_migration_new_callback (1, "items", "items-v1",
                                        create_table, NULL);
    migrations[0] = first;
    migrator = orm_migrator_new (connection, migrations, 1, &error);
    g_assert_no_error (error);
    g_assert_true (orm_migrator_up (migrator, 0, &error));
    g_assert_false (orm_migrator_down (migrator, 0, &error));
    g_assert_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED);
}

static void
test_migration_requires_idle_connection (void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmEngine) engine = orm_engine_new (test_url, &error);
    g_autoptr(OrmConnection) connection = NULL;
    g_autoptr(OrmMigrator) migrator = NULL;
    g_autoptr(OrmTransaction) transaction = NULL;

    g_assert_no_error (error);
    connection = orm_engine_connect (engine, &error);
    g_assert_no_error (error);
    drop_bookkeeping (connection);
    migrator = new_migrator (connection, FALSE, FALSE, FALSE);
    transaction = orm_connection_begin_transaction (connection, &error);
    g_assert_no_error (error);
    g_assert_false (orm_migrator_up (migrator, 0, &error));
    g_assert_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION);
    g_assert_true (orm_transaction_rollback (transaction, NULL));
}

static void
test_migration (gconstpointer data)
{
    const gchar *scenario = data;
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmEngine) engine = orm_engine_new (test_url, &error);
    g_autoptr(OrmConnection) connection = NULL;
    g_autoptr(OrmMigrator) migrator = NULL;
    guint applied = 0;
    guint failed = 0;
    gboolean callback = g_str_equal (scenario, "callback");
    gboolean failing = g_str_equal (scenario, "rollback");

    g_assert_no_error (error);
    connection = orm_engine_connect (engine, &error);
    g_assert_no_error (error);
    drop_bookkeeping (connection);
    migrator = new_migrator (connection, callback, failing, FALSE);
    g_signal_connect (migrator, "migration-applied",
                      G_CALLBACK (count_applied), &applied);
    g_signal_connect (migrator, "migration-failed",
                      G_CALLBACK (count_failed), &failed);

    {
        g_autoptr(OrmResult) result = orm_connection_query (connection,
            "SELECT * FROM schema_migrations", &error);

        g_assert_null (result);
        g_assert_nonnull (error);
        g_clear_error (&error);
    }
    if (g_str_equal (scenario, "concurrent"))
    {
        GPid children[2];
        gchar *args[] = { (gchar *) test_program, "--worker",
                          (gchar *) test_url, NULL };
        gint i;
        gint status;

        for (i = 0; i < 2; i++)
        {
            g_assert_true (g_spawn_async (NULL, args, NULL,
                                          G_SPAWN_DO_NOT_REAP_CHILD,
                                          NULL, NULL, &children[i], &error));
            g_assert_no_error (error);
        }
        for (i = 0; i < 2; i++)
        {
            g_assert_cmpint (waitpid (children[i], &status, 0), ==, children[i]);
            g_assert_true (g_spawn_check_wait_status (status, &error));
            g_assert_no_error (error);
            g_spawn_close_pid (children[i]);
        }
        g_clear_object (&migrator);
        migrator = new_migrator (connection, TRUE, FALSE, FALSE);
        check_status (migrator, 2);
    }
    else if (failing)
    {
        g_assert_false (orm_migrator_up (migrator, 0, &error));
        g_assert_nonnull (error);
        g_clear_error (&error);
        check_status (migrator, 1);
        g_assert_cmpuint (applied, ==, 1);
        g_assert_cmpuint (failed, ==, 1);
        g_assert_cmpint (scalar (connection,
            "SELECT COUNT(*) FROM migration_items"), ==, 0);
        if (orm_engine_get_dialect_type (engine) != ORM_DIALECT_MYSQL)
        {
            g_autoptr(OrmResult) result = orm_connection_query (connection,
                "SELECT * FROM migration_partial", &error);

            g_assert_null (result);
            g_assert_nonnull (error);
        }
        else
            g_assert_cmpint (scalar (connection,
                "SELECT COUNT(*) FROM migration_partial"), ==, 0);
        return;
    }
    else
    {
        check_status (migrator, 0);
        g_assert_true (orm_migrator_up (migrator, 1, &error));
        check_status (migrator, 1);
        g_assert_true (orm_migrator_up (migrator, 0, &error));
        g_assert_no_error (error);
        check_status (migrator, 2);
        g_assert_cmpuint (applied, ==, 2);
    }
    if (g_str_equal (scenario, "noop"))
    {
        g_assert_true (orm_migrator_up (migrator, 0, &error));
        g_assert_no_error (error);
        g_assert_cmpuint (applied, ==, 2);
        check_status (migrator, 2);
    }
    if (g_str_equal (scenario, "down"))
    {
        g_assert_true (orm_migrator_down (migrator, 1, &error));
        g_assert_no_error (error);
        check_status (migrator, 1);
        g_assert_cmpuint (applied, ==, 3);
        g_assert_cmpint (scalar (connection,
            "SELECT COUNT(*) FROM migration_items"), ==, 0);
        return;
    }
    if (g_str_equal (scenario, "checksum"))
    {
        g_autoptr(GArray) history = NULL;
        g_autoptr(GArray) pending = NULL;

        g_clear_object (&migrator);
        migrator = new_migrator (connection, FALSE, FALSE, TRUE);
        g_assert_false (orm_migrator_up (migrator, 0, &error));
        g_assert_error (error, ORM_ERROR, ORM_ERROR_INTEGRITY_ERROR);
        g_clear_error (&error);
        g_assert_false (orm_migrator_down (migrator, 0, &error));
        g_assert_error (error, ORM_ERROR, ORM_ERROR_INTEGRITY_ERROR);
        g_clear_error (&error);
        g_assert_false (orm_migrator_status (migrator, &history, &pending, &error));
        g_assert_error (error, ORM_ERROR, ORM_ERROR_INTEGRITY_ERROR);
        g_assert_null (history);
        g_assert_null (pending);
    }
    g_assert_cmpint (scalar (connection,
        "SELECT COUNT(*) FROM schema_migrations"), ==, 2);
    g_assert_cmpint (scalar (connection,
        "SELECT COUNT(*) FROM migration_items"), ==, 1);
    g_assert_cmpint (scalar (connection,
        "SELECT id FROM migration_items"), ==, 7);
    if (callback)
    {
        g_autoptr(OrmResult) result = orm_connection_query (connection,
            "SELECT * FROM migration_items", NULL);
        g_autoptr(OrmResult) sql_result = NULL;

        g_assert_cmpint (orm_result_get_column_count (result), ==, 1);
        g_assert_cmpstr (orm_result_get_column_name (result, 0), ==, "id");
        g_assert_true (orm_connection_execute (connection,
            "CREATE TABLE migration_sql_items (id INTEGER)", &error));
        sql_result = orm_connection_query (connection,
            "SELECT * FROM migration_sql_items", &error);
        g_assert_no_error (error);
        g_assert_cmpint (orm_result_get_column_count (result), ==,
                         orm_result_get_column_count (sql_result));
        g_assert_cmpstr (orm_result_get_column_type_name (result, 0), ==,
                         orm_result_get_column_type_name (sql_result, 0));
        g_assert_cmpint (orm_result_get_column_type (result, 0), ==,
                         ORM_VALUE_INTEGER);
        g_assert_true (orm_connection_execute (connection,
            "DROP TABLE migration_sql_items", &error));
    }
}

gint
main (gint    argc,
      gchar **argv)
{
    const gchar *scenarios[] = { "fresh", "noop", "down", "checksum",
                                 "rollback", "concurrent", "callback" };
    g_autofree gchar *path = NULL;
    g_autofree gchar *url = NULL;
    guint i;
    gint result;

    test_program = argv[0];
    if (argc == 3 && g_str_equal (argv[1], "--worker"))
    {
        g_autoptr(GError) error = NULL;
        g_autoptr(OrmEngine) engine = orm_engine_new (argv[2], &error);
        g_autoptr(OrmConnection) connection = NULL;
        g_autoptr(OrmMigrator) migrator = NULL;

        delay_create = TRUE;
        g_assert_no_error (error);
        connection = orm_engine_connect (engine, &error);
        g_assert_no_error (error);
        migrator = new_migrator (connection, TRUE, FALSE, FALSE);
        g_assert_true (orm_migrator_up (migrator, 0, &error));
        g_assert_no_error (error);
        return 0;
    }
    g_test_init (&argc, &argv, NULL);
    test_url = g_getenv ("ORM_TEST_URL");
    if (test_url == NULL)
    {
        gint fd = g_file_open_tmp ("orm-migrations-XXXXXX", &path, NULL);

        g_assert_cmpint (fd, >=, 0);
        g_close (fd, NULL);
        url = g_strconcat ("sqlite://", path, NULL);
        test_url = url;
    }
    g_test_add_func ("/migration/boxed", test_migration_boxed);
    g_test_add_func ("/migration/rejects-order", test_migration_rejects_order);
    g_test_add_func ("/migration/unknown-target", test_migration_unknown_target);
    g_test_add_func ("/migration/irreversible-down",
                     test_migration_irreversible_down);
    g_test_add_func ("/migration/requires-idle-connection",
                     test_migration_requires_idle_connection);
    for (i = 0; i < G_N_ELEMENTS (scenarios); i++)
    {
        g_autofree gchar *name = g_strconcat ("/migration/", scenarios[i], NULL);

        g_test_add_data_func (name, scenarios[i], test_migration);
    }
    result = g_test_run ();
    if (path != NULL)
        g_unlink (path);
    return result;
}
