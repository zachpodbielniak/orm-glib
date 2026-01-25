/* test-engine.c
 *
 * Copyright 2025 Zach Pobiel
 *
 * This file is part of orm-glib.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>

#define ORM_INSIDE
#include "orm.h"
#undef ORM_INSIDE

#include "test-fixtures.h"

/* ============================================================================
 * OrmEngine Tests
 * ============================================================================ */

static void
test_engine_new_sqlite_memory (void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmEngine) engine = orm_engine_new ("sqlite:///:memory:", &error);

    g_assert_no_error (error);
    g_assert_nonnull (engine);
    g_assert_true (ORM_IS_ENGINE (engine));
}

static void
test_engine_new_sqlite_file (void)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *path = g_build_filename (g_get_tmp_dir (), "test.db", NULL);
    g_autofree gchar *url = g_strdup_printf ("sqlite:///%s", path);

    g_autoptr(OrmEngine) engine = orm_engine_new (url, &error);

    g_assert_no_error (error);
    g_assert_nonnull (engine);

    /* Clean up test file */
    g_unlink (path);
}

static void
test_engine_get_dialect (void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmEngine) engine = orm_engine_new ("sqlite:///:memory:", &error);

    g_assert_no_error (error);

    OrmDialect *dialect = orm_engine_get_dialect (engine);
    g_assert_nonnull (dialect);
    g_assert_true (ORM_IS_DIALECT (dialect));
}

static void
test_engine_invalid_url (void)
{
    g_autoptr(GError) error = NULL;
    OrmEngine *engine = orm_engine_new ("invalid://url", &error);

    g_assert_error (error, ORM_ERROR, ORM_ERROR_INVALID_URL);
    g_assert_null (engine);
}

/* ============================================================================
 * OrmConnection Tests
 * ============================================================================ */

static void
test_connection_connect (TestDbFixture *fixture,
                         gconstpointer  user_data)
{
    (void) user_data;

    g_assert_nonnull (fixture->connection);
    g_assert_true (ORM_IS_CONNECTION (fixture->connection));
}

static void
test_connection_execute (TestDbFixture *fixture,
                         gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    gboolean result = orm_connection_execute (fixture->connection,
                                              "CREATE TABLE test (id INTEGER)",
                                              &error);

    g_assert_no_error (error);
    g_assert_true (result);
}

static void
test_connection_execute_invalid_sql (TestDbFixture *fixture,
                                     gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    gboolean result = orm_connection_execute (fixture->connection,
                                              "INVALID SQL STATEMENT",
                                              &error);

    /* ORM_ERROR_EXECUTE is used for SQL execution failures */
    g_assert_error (error, ORM_ERROR, ORM_ERROR_EXECUTE);
    g_assert_false (result);
}

static void
test_connection_query (TestDbFixture *fixture,
                       gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Create and populate table */
    orm_connection_execute (fixture->connection,
                           "CREATE TABLE test (id INTEGER, name TEXT)",
                           NULL);
    orm_connection_execute (fixture->connection,
                           "INSERT INTO test VALUES (1, 'Alice'), (2, 'Bob')",
                           NULL);

    g_autoptr(OrmResult) result = orm_connection_query (fixture->connection,
                                                        "SELECT * FROM test",
                                                        &error);

    g_assert_no_error (error);
    g_assert_nonnull (result);
    g_assert_true (ORM_IS_RESULT (result));
}

static void
test_connection_query_with_params (TestDbFixture *fixture,
                                   gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;
    GList *params = NULL;

    (void) user_data;

    /* Create and populate table */
    orm_connection_execute (fixture->connection,
                           "CREATE TABLE test (id INTEGER, name TEXT)",
                           NULL);
    orm_connection_execute (fixture->connection,
                           "INSERT INTO test VALUES (1, 'Alice'), (2, 'Bob')",
                           NULL);

    params = g_list_append (params, orm_value_new_integer (1));

    g_autoptr(OrmResult) result = orm_connection_query_with_params (
        fixture->connection,
        "SELECT * FROM test WHERE id = ?",
        params,
        &error);

    g_assert_no_error (error);
    g_assert_nonnull (result);

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

/* ============================================================================
 * OrmResult Tests
 * ============================================================================ */

static void
test_result_next (TestDbFixture *fixture,
                  gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Create and populate table */
    orm_connection_execute (fixture->connection,
                           "CREATE TABLE test (id INTEGER, name TEXT)",
                           NULL);
    orm_connection_execute (fixture->connection,
                           "INSERT INTO test VALUES (1, 'Alice'), (2, 'Bob')",
                           NULL);

    g_autoptr(OrmResult) result = orm_connection_query (fixture->connection,
                                                        "SELECT * FROM test",
                                                        &error);

    gint count = 0;
    while (orm_result_next (result))
    {
        count++;
    }

    g_assert_cmpint (count, ==, 2);
}

static void
test_result_get_row (TestDbFixture *fixture,
                     gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Create and populate table */
    orm_connection_execute (fixture->connection,
                           "CREATE TABLE test (id INTEGER, name TEXT)",
                           NULL);
    orm_connection_execute (fixture->connection,
                           "INSERT INTO test VALUES (1, 'Alice')",
                           NULL);

    g_autoptr(OrmResult) result = orm_connection_query (fixture->connection,
                                                        "SELECT * FROM test",
                                                        &error);

    g_assert_true (orm_result_next (result));

    OrmRow *row = orm_result_get_row (result);
    g_assert_nonnull (row);
    g_assert_true (ORM_IS_ROW (row));
}

static void
test_result_empty (TestDbFixture *fixture,
                   gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    orm_connection_execute (fixture->connection,
                           "CREATE TABLE test (id INTEGER)",
                           NULL);

    g_autoptr(OrmResult) result = orm_connection_query (fixture->connection,
                                                        "SELECT * FROM test",
                                                        &error);

    g_assert_false (orm_result_next (result));
}

/* ============================================================================
 * OrmRow Tests
 * ============================================================================ */

static void
test_row_get_value_by_index (TestDbFixture *fixture,
                             gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    orm_connection_execute (fixture->connection,
                           "CREATE TABLE test (id INTEGER, name TEXT)",
                           NULL);
    orm_connection_execute (fixture->connection,
                           "INSERT INTO test VALUES (42, 'Alice')",
                           NULL);

    g_autoptr(OrmResult) result = orm_connection_query (fixture->connection,
                                                        "SELECT * FROM test",
                                                        &error);
    g_assert_true (orm_result_next (result));

    OrmRow *row = orm_result_get_row (result);

    OrmValue *id_val = orm_row_get_value (row, 0);
    g_assert_nonnull (id_val);
    g_assert_cmpint (orm_value_get_integer (id_val), ==, 42);

    OrmValue *name_val = orm_row_get_value (row, 1);
    g_assert_nonnull (name_val);
    g_assert_cmpstr (orm_value_get_string (name_val), ==, "Alice");
}

static void
test_row_get_value_by_name (TestDbFixture *fixture,
                            gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    orm_connection_execute (fixture->connection,
                           "CREATE TABLE test (id INTEGER, name TEXT)",
                           NULL);
    orm_connection_execute (fixture->connection,
                           "INSERT INTO test VALUES (42, 'Alice')",
                           NULL);

    g_autoptr(OrmResult) result = orm_connection_query (fixture->connection,
                                                        "SELECT * FROM test",
                                                        &error);
    g_assert_true (orm_result_next (result));

    OrmRow *row = orm_result_get_row (result);

    OrmValue *id_val = orm_row_get_value_by_name (row, "id");
    g_assert_nonnull (id_val);
    g_assert_cmpint (orm_value_get_integer (id_val), ==, 42);

    OrmValue *name_val = orm_row_get_value_by_name (row, "name");
    g_assert_nonnull (name_val);
    g_assert_cmpstr (orm_value_get_string (name_val), ==, "Alice");
}

static void
test_row_null_value (TestDbFixture *fixture,
                     gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    orm_connection_execute (fixture->connection,
                           "CREATE TABLE test (id INTEGER, name TEXT)",
                           NULL);
    orm_connection_execute (fixture->connection,
                           "INSERT INTO test VALUES (1, NULL)",
                           NULL);

    g_autoptr(OrmResult) result = orm_connection_query (fixture->connection,
                                                        "SELECT * FROM test",
                                                        &error);
    g_assert_true (orm_result_next (result));

    OrmRow *row = orm_result_get_row (result);

    OrmValue *name_val = orm_row_get_value_by_name (row, "name");
    g_assert_nonnull (name_val);
    g_assert_true (orm_value_is_null (name_val));
}

/* ============================================================================
 * Main
 * ============================================================================ */

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Engine tests */
    g_test_add_func ("/engine/new-sqlite-memory", test_engine_new_sqlite_memory);
    g_test_add_func ("/engine/new-sqlite-file", test_engine_new_sqlite_file);
    g_test_add_func ("/engine/get-dialect", test_engine_get_dialect);
    g_test_add_func ("/engine/invalid-url", test_engine_invalid_url);

    /* Connection tests */
    TEST_ADD_FIXTURE ("/connection/connect", test_connection_connect);
    TEST_ADD_FIXTURE ("/connection/execute", test_connection_execute);
    TEST_ADD_FIXTURE ("/connection/execute-invalid-sql", test_connection_execute_invalid_sql);
    TEST_ADD_FIXTURE ("/connection/query", test_connection_query);
    TEST_ADD_FIXTURE ("/connection/query-with-params", test_connection_query_with_params);

    /* Result tests */
    TEST_ADD_FIXTURE ("/result/next", test_result_next);
    TEST_ADD_FIXTURE ("/result/get-row", test_result_get_row);
    TEST_ADD_FIXTURE ("/result/empty", test_result_empty);

    /* Row tests */
    TEST_ADD_FIXTURE ("/row/get-value-by-index", test_row_get_value_by_index);
    TEST_ADD_FIXTURE ("/row/get-value-by-name", test_row_get_value_by_name);
    TEST_ADD_FIXTURE ("/row/null-value", test_row_null_value);

    return g_test_run ();
}
