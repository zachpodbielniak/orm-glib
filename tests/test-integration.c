/* test-integration.c
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

/* ============================================================================
 * Full CRUD Lifecycle Tests
 * ============================================================================ */

static void
test_integration_create_user (TestDbFixture *fixture,
                              gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Setup */
    test_create_users_table (fixture->connection, NULL);
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    /* Create */
    g_autoptr(TestUser) user = test_user_new_with_values ("NewUser", "newuser@example.com");
    test_user_set_active (user, TRUE);

    orm_session_add (fixture->session, G_OBJECT (user));
    g_assert_true (orm_session_is_dirty (fixture->session));

    /* Commit */
    gboolean result = orm_session_commit (fixture->session, &error);
    g_assert_no_error (error);
    g_assert_true (result);
    g_assert_false (orm_session_is_dirty (fixture->session));

    /* Verify user has ID assigned */
    gint64 id = test_user_get_id (user);
    g_assert_cmpint (id, >, 0);
}

static void
test_integration_read_user (TestDbFixture *fixture,
                            gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Setup */
    test_create_users_table (fixture->connection, NULL);
    test_insert_sample_users (fixture->connection, NULL);
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    /* Read by primary key */
    g_autoptr(OrmValue) pk = orm_value_new_integer (1);
    GObject *user = orm_session_get (fixture->session, TEST_TYPE_USER, pk, &error);

    g_assert_no_error (error);
    g_assert_nonnull (user);
    g_assert_true (TEST_IS_USER (user));

    TestUser *test_user = TEST_USER (user);
    g_assert_cmpstr (test_user_get_name (test_user), ==, "Alice");
    g_assert_cmpstr (test_user_get_email (test_user), ==, "alice@example.com");

    g_object_unref (user);
}

static void
test_integration_update_user (TestDbFixture *fixture,
                              gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Setup */
    test_create_users_table (fixture->connection, NULL);
    test_insert_sample_users (fixture->connection, NULL);
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    /* Read existing user */
    g_autoptr(OrmValue) pk = orm_value_new_integer (1);
    GObject *user = orm_session_get (fixture->session, TEST_TYPE_USER, pk, &error);
    g_assert_nonnull (user);

    /* Update */
    test_user_set_name (TEST_USER (user), "Alice Updated");
    test_user_set_email (TEST_USER (user), "alice.updated@example.com");

    /* Commit */
    gboolean result = orm_session_commit (fixture->session, &error);
    g_assert_no_error (error);
    g_assert_true (result);

    g_object_unref (user);

    /* Verify update by reading again */
    GObject *user2 = orm_session_get (fixture->session, TEST_TYPE_USER, pk, &error);
    g_assert_nonnull (user2);
    g_assert_cmpstr (test_user_get_name (TEST_USER (user2)), ==, "Alice Updated");

    g_object_unref (user2);
}

static void
test_integration_delete_user (TestDbFixture *fixture,
                              gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Setup */
    test_create_users_table (fixture->connection, NULL);
    test_insert_sample_users (fixture->connection, NULL);
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    /* Read existing user */
    g_autoptr(OrmValue) pk = orm_value_new_integer (1);
    GObject *user = orm_session_get (fixture->session, TEST_TYPE_USER, pk, &error);
    g_assert_nonnull (user);

    /* Delete */
    orm_session_delete (fixture->session, user);

    /* Commit */
    gboolean result = orm_session_commit (fixture->session, &error);
    g_assert_no_error (error);
    g_assert_true (result);

    g_object_unref (user);

    /* Verify deletion */
    GObject *user2 = orm_session_get (fixture->session, TEST_TYPE_USER, pk, &error);
    g_assert_null (user2);
}

/* ============================================================================
 * Query Integration Tests
 * ============================================================================ */

static void
test_integration_query_all (TestDbFixture *fixture,
                            gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Setup */
    test_create_users_table (fixture->connection, NULL);
    test_insert_sample_users (fixture->connection, NULL);
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    /* Query all */
    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    GList *users = orm_query_all (query, &error);

    g_assert_no_error (error);
    g_assert_nonnull (users);
    g_assert_cmpuint (g_list_length (users), ==, 3);

    /* Verify each user is properly loaded */
    {
        GList *l;
        for (l = users; l != NULL; l = l->next)
        {
            TestUser *user;
            g_assert_true (TEST_IS_USER (l->data));
            user = TEST_USER (l->data);
            g_assert_nonnull (test_user_get_name (user));
        }
    }

    g_list_free_full (users, g_object_unref);
}

static void
test_integration_query_filter_and_order (TestDbFixture *fixture,
                                         gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Setup */
    test_create_users_table (fixture->connection, NULL);
    test_insert_sample_users (fixture->connection, NULL);
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    /* Query with filter and order */
    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    g_autoptr(OrmValue) active = orm_value_new_integer (1);

    orm_query_filter_by (query, "active", active);
    orm_query_order_by (query, "name", ORM_SORT_ASC);

    GList *users = orm_query_all (query, &error);

    g_assert_no_error (error);
    g_assert_cmpuint (g_list_length (users), ==, 2);

    /* Verify order: Alice should come before Bob */
    TestUser *first = TEST_USER (users->data);
    g_assert_cmpstr (test_user_get_name (first), ==, "Alice");

    g_list_free_full (users, g_object_unref);
}

static void
test_integration_query_pagination (TestDbFixture *fixture,
                                   gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Setup */
    test_create_users_table (fixture->connection, NULL);
    test_insert_sample_users (fixture->connection, NULL);
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    /* Query with pagination */
    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);

    orm_query_order_by (query, "id", ORM_SORT_ASC);
    orm_query_limit (query, 2);
    orm_query_offset (query, 1);

    GList *users = orm_query_all (query, &error);

    g_assert_no_error (error);
    g_assert_cmpuint (g_list_length (users), ==, 2);

    /* Should skip first user (Alice) and get Bob and Charlie */
    TestUser *first = TEST_USER (users->data);
    g_assert_cmpstr (test_user_get_name (first), ==, "Bob");

    g_list_free_full (users, g_object_unref);
}

/* ============================================================================
 * Transaction Tests
 * ============================================================================ */

static void
test_integration_transaction_commit (TestDbFixture *fixture,
                                     gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Setup */
    test_create_users_table (fixture->connection, NULL);
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    /* Begin transaction implicitly by adding objects */
    g_autoptr(TestUser) user1 = test_user_new_with_values ("TxUser1", "tx1@example.com");
    g_autoptr(TestUser) user2 = test_user_new_with_values ("TxUser2", "tx2@example.com");

    orm_session_add (fixture->session, G_OBJECT (user1));
    orm_session_add (fixture->session, G_OBJECT (user2));

    /* Commit transaction */
    gboolean result = orm_session_commit (fixture->session, &error);
    g_assert_no_error (error);
    g_assert_true (result);

    /* Verify both users exist */
    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_USER);
    gint64 count = orm_query_count (query, &error);
    g_assert_cmpint (count, ==, 2);
}

static void
test_integration_transaction_rollback (TestDbFixture *fixture,
                                       gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Setup */
    test_create_users_table (fixture->connection, NULL);
    test_insert_sample_users (fixture->connection, NULL);  /* Start with 3 users */
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    /* Add a new user */
    g_autoptr(TestUser) user = test_user_new_with_values ("RollbackUser", "rollback@example.com");
    orm_session_add (fixture->session, G_OBJECT (user));

    /* Rollback instead of commit */
    orm_session_rollback (fixture->session);

    /* Session should not be dirty */
    g_assert_false (orm_session_is_dirty (fixture->session));
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

static void
test_integration_invalid_query (TestDbFixture *fixture,
                                gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Execute invalid SQL */
    gboolean result = orm_session_execute (fixture->session,
                                           "SELECT * FROM nonexistent_table",
                                           &error);

    g_assert_error (error, ORM_ERROR, ORM_ERROR_EXECUTE);
    g_assert_false (result);
}

static void
test_integration_constraint_violation (TestDbFixture *fixture,
                                       gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Setup - create table with unique constraint */
    test_create_users_table (fixture->connection, NULL);
    test_insert_sample_users (fixture->connection, NULL);
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    /* Try to insert user with duplicate email */
    g_autoptr(TestUser) user = test_user_new_with_values ("Duplicate", "alice@example.com");
    orm_session_add (fixture->session, G_OBJECT (user));

    gboolean result = orm_session_commit (fixture->session, &error);

    /* Should fail due to unique constraint */
    g_assert_nonnull (error);
    g_assert_false (result);
}

/* ============================================================================
 * Relationship Tests (Posts -> Users)
 * ============================================================================ */

static void
test_integration_with_relationships (TestDbFixture *fixture,
                                     gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Setup */
    test_create_users_table (fixture->connection, NULL);
    test_create_posts_table (fixture->connection, NULL);
    test_insert_sample_users (fixture->connection, NULL);
    test_insert_sample_posts (fixture->connection, NULL);

    g_autoptr(OrmMapper) user_mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    g_autoptr(OrmMapper) post_mapper = orm_mapper_new_from_serializable (TEST_TYPE_POST);
    orm_session_register_mapper (fixture->session, user_mapper);
    orm_session_register_mapper (fixture->session, post_mapper);

    /* Query posts */
    g_autoptr(OrmQuery) query = orm_session_query (fixture->session, TEST_TYPE_POST);
    g_autoptr(OrmValue) user_id = orm_value_new_integer (1);

    orm_query_filter_by (query, "user-id", user_id);

    GList *posts = orm_query_all (query, &error);

    g_assert_no_error (error);
    g_assert_nonnull (posts);
    g_assert_cmpuint (g_list_length (posts), ==, 2);  /* Alice has 2 posts */

    g_list_free_full (posts, g_object_unref);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* CRUD lifecycle tests */
    TEST_ADD_FIXTURE ("/integration/create-user", test_integration_create_user);
    TEST_ADD_FIXTURE ("/integration/read-user", test_integration_read_user);
    TEST_ADD_FIXTURE ("/integration/update-user", test_integration_update_user);
    TEST_ADD_FIXTURE ("/integration/delete-user", test_integration_delete_user);

    /* Query tests */
    TEST_ADD_FIXTURE ("/integration/query-all", test_integration_query_all);
    TEST_ADD_FIXTURE ("/integration/query-filter-and-order", test_integration_query_filter_and_order);
    TEST_ADD_FIXTURE ("/integration/query-pagination", test_integration_query_pagination);

    /* Transaction tests */
    TEST_ADD_FIXTURE ("/integration/transaction-commit", test_integration_transaction_commit);
    TEST_ADD_FIXTURE ("/integration/transaction-rollback", test_integration_transaction_rollback);

    /* Error handling tests */
    TEST_ADD_FIXTURE ("/integration/invalid-query", test_integration_invalid_query);
    TEST_ADD_FIXTURE ("/integration/constraint-violation", test_integration_constraint_violation);

    /* Relationship tests */
    TEST_ADD_FIXTURE ("/integration/with-relationships", test_integration_with_relationships);

    return g_test_run ();
}
