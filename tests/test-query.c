/* test-query.c
 *
 * Copyright 2025 Zach Pobiel
 *
 * This file is part of orm-glib.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include <glib-object.h>

#define ORM_INSIDE
#include "orm.h"
#undef ORM_INSIDE

#include "test-fixtures.h"
#include "test-model.h"

/* Helper to set up users for query tests */
static void
setup_users_for_query (TestDbFixture *fixture)
{
    test_create_users_table (fixture->connection, NULL);
    test_insert_sample_users (fixture->connection, NULL);

    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);
}

/* ============================================================================
 * OrmQuery Basic Tests
 * ============================================================================ */

static void
test_query_new (TestDbFixture *fixture,
                gconstpointer  user_data)
{
    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);

    g_assert_nonnull (query);
    g_assert_true (ORM_IS_QUERY (query));
}

static void
test_query_get_sql (TestDbFixture *fixture,
                    gconstpointer  user_data)
{
    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    g_autofree gchar *sql = orm_query_get_sql (query);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "SELECT"));
    g_assert_nonnull (strstr (sql, "FROM"));
}

/* ============================================================================
 * OrmQuery Filter Tests
 * ============================================================================ */

static void
test_query_filter_eq (TestDbFixture *fixture,
                      gconstpointer  user_data)
{
    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    g_autoptr(OrmValue) value = orm_value_new_string ("Alice");

    orm_query_filter (query, "name", ORM_OP_EQ, value);

    g_autofree gchar *sql = orm_query_get_sql (query);
    g_assert_nonnull (strstr (sql, "WHERE"));
    g_assert_nonnull (strstr (sql, "name"));
}

static void
test_query_filter_by (TestDbFixture *fixture,
                      gconstpointer  user_data)
{
    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    g_autoptr(OrmValue) value = orm_value_new_boolean (TRUE);

    orm_query_filter_by (query, "active", value);

    g_autofree gchar *sql = orm_query_get_sql (query);
    g_assert_nonnull (strstr (sql, "WHERE"));
}

static void
test_query_filter_multiple (TestDbFixture *fixture,
                            gconstpointer  user_data)
{
    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    g_autoptr(OrmValue) active = orm_value_new_boolean (TRUE);
    g_autoptr(OrmValue) name = orm_value_new_string ("Alice");

    orm_query_filter_by (query, "active", active);
    orm_query_filter_by (query, "name", name);

    g_autofree gchar *sql = orm_query_get_sql (query);
    g_assert_nonnull (strstr (sql, "AND"));
}

/* ============================================================================
 * OrmQuery Order Tests
 * ============================================================================ */

static void
test_query_order_by (TestDbFixture *fixture,
                     gconstpointer  user_data)
{
    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);

    orm_query_order_by (query, "name", ORM_SORT_ASC);

    g_autofree gchar *sql = orm_query_get_sql (query);
    g_assert_nonnull (strstr (sql, "ORDER BY"));
    g_assert_nonnull (strstr (sql, "ASC"));
}

static void
test_query_order_by_desc (TestDbFixture *fixture,
                          gconstpointer  user_data)
{
    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);

    orm_query_order_by (query, "created_at", ORM_SORT_DESC);

    g_autofree gchar *sql = orm_query_get_sql (query);
    g_assert_nonnull (strstr (sql, "ORDER BY"));
    g_assert_nonnull (strstr (sql, "DESC"));
}

/* ============================================================================
 * OrmQuery Limit/Offset Tests
 * ============================================================================ */

static void
test_query_limit (TestDbFixture *fixture,
                  gconstpointer  user_data)
{
    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);

    orm_query_limit (query, 10);

    g_autofree gchar *sql = orm_query_get_sql (query);
    g_assert_nonnull (strstr (sql, "LIMIT"));
    g_assert_nonnull (strstr (sql, "10"));
}

static void
test_query_offset (TestDbFixture *fixture,
                   gconstpointer  user_data)
{
    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);

    orm_query_limit (query, 10);
    orm_query_offset (query, 5);

    g_autofree gchar *sql = orm_query_get_sql (query);
    g_assert_nonnull (strstr (sql, "OFFSET"));
    g_assert_nonnull (strstr (sql, "5"));
}

/* ============================================================================
 * OrmQuery Execution Tests
 * ============================================================================ */

static void
test_query_all (TestDbFixture *fixture,
                gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    GList *results = orm_query_all (query, &error);

    g_assert_no_error (error);
    g_assert_nonnull (results);
    g_assert_cmpuint (g_list_length (results), ==, 3);  /* Alice, Bob, Charlie */

    g_list_free_full (results, g_object_unref);
}

static void
test_query_all_with_filter (TestDbFixture *fixture,
                            gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    g_autoptr(OrmValue) active = orm_value_new_integer (1);

    orm_query_filter_by (query, "active", active);

    GList *results = orm_query_all (query, &error);

    g_assert_no_error (error);
    g_assert_nonnull (results);
    g_assert_cmpuint (g_list_length (results), ==, 2);  /* Alice, Bob */

    g_list_free_full (results, g_object_unref);
}

static void
test_query_first (TestDbFixture *fixture,
                  gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    orm_query_order_by (query, "id", ORM_SORT_ASC);

    GObject *result = orm_query_first (query, &error);

    g_assert_no_error (error);
    g_assert_nonnull (result);
    g_assert_true (TEST_IS_USER (result));

    g_object_unref (result);
}

static void
test_query_first_empty (TestDbFixture *fixture,
                        gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    g_autoptr(OrmValue) name = orm_value_new_string ("Nonexistent");

    orm_query_filter_by (query, "name", name);

    GObject *result = orm_query_first (query, &error);

    g_assert_null (result);
}

static void
test_query_one (TestDbFixture *fixture,
                gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    g_autoptr(OrmValue) name = orm_value_new_string ("Alice");

    orm_query_filter_by (query, "name", name);

    GObject *result = orm_query_one (query, &error);

    g_assert_no_error (error);
    g_assert_nonnull (result);
    g_assert_cmpstr (test_user_get_name (TEST_USER (result)), ==, "Alice");

    g_object_unref (result);
}

static void
test_query_one_multiple_results (TestDbFixture *fixture,
                                 gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    g_autoptr(OrmValue) active = orm_value_new_integer (1);

    orm_query_filter_by (query, "active", active);

    GObject *result = orm_query_one (query, &error);

    /* Should fail because there are multiple active users */
    g_assert_error (error, ORM_ERROR, ORM_ERROR_QUERY_FAILED);
    g_assert_null (result);
}

static void
test_query_count (TestDbFixture *fixture,
                  gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    gint64 count = orm_query_count (query, &error);

    g_assert_no_error (error);
    g_assert_cmpint (count, ==, 3);
}

static void
test_query_count_with_filter (TestDbFixture *fixture,
                              gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    g_autoptr(OrmValue) active = orm_value_new_integer (0);

    orm_query_filter_by (query, "active", active);

    gint64 count = orm_query_count (query, &error);

    g_assert_no_error (error);
    g_assert_cmpint (count, ==, 1);  /* Only Charlie is inactive */
}

static void
test_query_exists (TestDbFixture *fixture,
                   gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    g_autoptr(OrmValue) name = orm_value_new_string ("Alice");

    orm_query_filter_by (query, "name", name);

    gboolean exists = orm_query_exists (query, &error);

    g_assert_no_error (error);
    g_assert_true (exists);
}

static void
test_query_exists_not_found (TestDbFixture *fixture,
                             gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    g_autoptr(OrmValue) name = orm_value_new_string ("Nonexistent");

    orm_query_filter_by (query, "name", name);

    gboolean exists = orm_query_exists (query, &error);

    g_assert_no_error (error);
    g_assert_false (exists);
}

/* ============================================================================
 * OrmQuery Delete Tests
 * ============================================================================ */

static void
test_query_delete (TestDbFixture *fixture,
                   gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    setup_users_for_query (fixture);

    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    g_autoptr(OrmValue) active = orm_value_new_integer (0);

    orm_query_filter_by (query, "active", active);

    gint64 deleted = orm_query_delete (query, &error);

    g_assert_no_error (error);
    g_assert_cmpint (deleted, >=, 0);

    /* Verify deletion */
    g_autoptr(OrmQuery) count_query = orm_session_query (fixture->session, TEST_TYPE_USER);
    gint64 count = orm_query_count (count_query, &error);
    g_assert_cmpint (count, ==, 2);  /* Only Alice and Bob remain */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Basic tests */
    TEST_ADD_FIXTURE ("/query/new", test_query_new);
    TEST_ADD_FIXTURE ("/query/get-sql", test_query_get_sql);

    /* Filter tests */
    TEST_ADD_FIXTURE ("/query/filter-eq", test_query_filter_eq);
    TEST_ADD_FIXTURE ("/query/filter-by", test_query_filter_by);
    TEST_ADD_FIXTURE ("/query/filter-multiple", test_query_filter_multiple);

    /* Order tests */
    TEST_ADD_FIXTURE ("/query/order-by", test_query_order_by);
    TEST_ADD_FIXTURE ("/query/order-by-desc", test_query_order_by_desc);

    /* Limit/Offset tests */
    TEST_ADD_FIXTURE ("/query/limit", test_query_limit);
    TEST_ADD_FIXTURE ("/query/offset", test_query_offset);

    /* Execution tests */
    TEST_ADD_FIXTURE ("/query/all", test_query_all);
    TEST_ADD_FIXTURE ("/query/all-with-filter", test_query_all_with_filter);
    TEST_ADD_FIXTURE ("/query/first", test_query_first);
    TEST_ADD_FIXTURE ("/query/first-empty", test_query_first_empty);
    TEST_ADD_FIXTURE ("/query/one", test_query_one);
    TEST_ADD_FIXTURE ("/query/one-multiple-results", test_query_one_multiple_results);
    TEST_ADD_FIXTURE ("/query/count", test_query_count);
    TEST_ADD_FIXTURE ("/query/count-with-filter", test_query_count_with_filter);
    TEST_ADD_FIXTURE ("/query/exists", test_query_exists);
    TEST_ADD_FIXTURE ("/query/exists-not-found", test_query_exists_not_found);

    /* Delete tests */
    TEST_ADD_FIXTURE ("/query/delete", test_query_delete);

    return g_test_run ();
}
