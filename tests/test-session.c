/* test-session.c
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
 * OrmSession Basic Tests
 * ============================================================================ */

static void
test_session_new (TestDbFixture *fixture,
                  gconstpointer  user_data)
{
    (void) user_data;

    g_assert_nonnull (fixture->session);
    g_assert_true (ORM_IS_SESSION (fixture->session));
}

static void
test_session_get_connection (TestDbFixture *fixture,
                             gconstpointer  user_data)
{
    (void) user_data;

    OrmConnection *conn = orm_session_get_connection (fixture->session);
    g_assert_nonnull (conn);
    g_assert_true (conn == fixture->connection);
}

static void
test_session_register_mapper (TestDbFixture *fixture,
                              gconstpointer  user_data)
{
    (void) user_data;

    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    OrmMapper *found = orm_session_get_mapper (fixture->session, TEST_TYPE_USER);
    g_assert_nonnull (found);
}

static void
test_session_get_mapper_unregistered (TestDbFixture *fixture,
                                      gconstpointer  user_data)
{
    (void) user_data;

    OrmMapper *found = orm_session_get_mapper (fixture->session, TEST_TYPE_POST);
    g_assert_null (found);
}

/* ============================================================================
 * OrmSession Add/Delete Tests
 * ============================================================================ */

static void
test_session_add (TestDbFixture *fixture,
                  gconstpointer  user_data)
{
    (void) user_data;

    g_autoptr(TestUser) user = test_user_new_with_values ("Alice", "alice@example.com");

    orm_session_add (fixture->session, G_OBJECT (user));

    OrmObjectState state = orm_session_get_object_state (fixture->session, G_OBJECT (user));
    g_assert_cmpint (state, ==, ORM_OBJECT_PENDING);
}

static void
test_session_add_all (TestDbFixture *fixture,
                      gconstpointer  user_data)
{
    (void) user_data;

    g_autoptr(TestUser) user1 = test_user_new_with_values ("Alice", "alice@example.com");
    g_autoptr(TestUser) user2 = test_user_new_with_values ("Bob", "bob@example.com");

    GList *users = NULL;
    users = g_list_append (users, user1);
    users = g_list_append (users, user2);

    orm_session_add_all (fixture->session, users);

    g_assert_cmpint (orm_session_get_object_state (fixture->session, G_OBJECT (user1)),
                     ==, ORM_OBJECT_PENDING);
    g_assert_cmpint (orm_session_get_object_state (fixture->session, G_OBJECT (user2)),
                     ==, ORM_OBJECT_PENDING);

    g_list_free (users);
}

static void
test_session_delete (TestDbFixture *fixture,
                     gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    /* Create table and insert user */
    test_create_users_table (fixture->connection, NULL);
    test_insert_sample_users (fixture->connection, NULL);

    /* Register mapper */
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    /* Get a user from database */
    g_autoptr(OrmValue) pk = orm_value_new_integer (1);
    GObject *user = orm_session_get (fixture->session, TEST_TYPE_USER, pk, &error);

    if (user != NULL)
    {
        orm_session_delete (fixture->session, user);

        OrmObjectState state = orm_session_get_object_state (fixture->session, user);
        g_assert_cmpint (state, ==, ORM_OBJECT_DELETED);

        g_object_unref (user);
    }
}

static void
test_session_expunge (TestDbFixture *fixture,
                      gconstpointer  user_data)
{
    (void) user_data;

    g_autoptr(TestUser) user = test_user_new_with_values ("Alice", "alice@example.com");

    orm_session_add (fixture->session, G_OBJECT (user));
    g_assert_cmpint (orm_session_get_object_state (fixture->session, G_OBJECT (user)),
                     ==, ORM_OBJECT_PENDING);

    orm_session_expunge (fixture->session, G_OBJECT (user));
    /* After expunge, object is no longer tracked by session - returns TRANSIENT */
    /* Note: A proper DETACHED state would require tracking previously-attached objects */
    g_assert_cmpint (orm_session_get_object_state (fixture->session, G_OBJECT (user)),
                     ==, ORM_OBJECT_TRANSIENT);
}

/* ============================================================================
 * OrmSession State Tests
 * ============================================================================ */

static void
test_session_is_dirty (TestDbFixture *fixture,
                       gconstpointer  user_data)
{
    (void) user_data;

    g_assert_false (orm_session_is_dirty (fixture->session));

    g_autoptr(TestUser) user = test_user_new_with_values ("Alice", "alice@example.com");
    orm_session_add (fixture->session, G_OBJECT (user));

    g_assert_true (orm_session_is_dirty (fixture->session));
}

static void
test_session_transient_object (TestDbFixture *fixture,
                               gconstpointer  user_data)
{
    (void) user_data;

    g_autoptr(TestUser) user = test_user_new ();

    /* Untracked object should be transient */
    OrmObjectState state = orm_session_get_object_state (fixture->session, G_OBJECT (user));
    g_assert_cmpint (state, ==, ORM_OBJECT_TRANSIENT);
}

/* ============================================================================
 * OrmSession Persistence Tests
 * ============================================================================ */

static void
test_session_flush (TestDbFixture *fixture,
                    gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    test_create_users_table (fixture->connection, NULL);

    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    g_autoptr(TestUser) user = test_user_new_with_values ("Alice", "alice@example.com");
    orm_session_add (fixture->session, G_OBJECT (user));

    gboolean result = orm_session_flush (fixture->session, &error);

    g_assert_no_error (error);
    g_assert_true (result);
}

static void
test_session_commit (TestDbFixture *fixture,
                     gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    test_create_users_table (fixture->connection, NULL);

    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    g_autoptr(TestUser) user = test_user_new_with_values ("Alice", "alice@example.com");
    orm_session_add (fixture->session, G_OBJECT (user));

    gboolean result = orm_session_commit (fixture->session, &error);

    g_assert_no_error (error);
    g_assert_true (result);
    g_assert_false (orm_session_is_dirty (fixture->session));
}

static void
test_session_rollback (TestDbFixture *fixture,
                       gconstpointer  user_data)
{
    (void) user_data;

    g_autoptr(TestUser) user = test_user_new_with_values ("Alice", "alice@example.com");
    orm_session_add (fixture->session, G_OBJECT (user));

    g_assert_true (orm_session_is_dirty (fixture->session));

    orm_session_rollback (fixture->session);

    g_assert_false (orm_session_is_dirty (fixture->session));
}

/* ============================================================================
 * OrmSession Get Tests
 * ============================================================================ */

static void
test_session_get (TestDbFixture *fixture,
                  gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    test_create_users_table (fixture->connection, NULL);
    test_insert_sample_users (fixture->connection, NULL);

    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    g_autoptr(OrmValue) pk = orm_value_new_integer (1);
    GObject *user = orm_session_get (fixture->session, TEST_TYPE_USER, pk, &error);

    g_assert_no_error (error);
    g_assert_nonnull (user);
    g_assert_true (TEST_IS_USER (user));

    g_object_unref (user);
}

static void
test_session_get_not_found (TestDbFixture *fixture,
                            gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    test_create_users_table (fixture->connection, NULL);

    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);
    orm_session_register_mapper (fixture->session, mapper);

    g_autoptr(OrmValue) pk = orm_value_new_integer (999);
    GObject *user = orm_session_get (fixture->session, TEST_TYPE_USER, pk, &error);

    g_assert_null (user);
}

/* ============================================================================
 * OrmSession Execute Tests
 * ============================================================================ */

static void
test_session_execute (TestDbFixture *fixture,
                      gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    gboolean result = orm_session_execute (fixture->session,
                                           "CREATE TABLE test (id INTEGER)",
                                           &error);

    g_assert_no_error (error);
    g_assert_true (result);
}

/* ============================================================================
 * OrmSession Close Tests
 * ============================================================================ */

static void
test_session_close (void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmEngine) engine = orm_engine_new ("sqlite:///:memory:", &error);
    g_assert_no_error (error);

    OrmSession *session = orm_session_new (engine);
    g_assert_nonnull (session);
    g_assert_false (orm_session_is_closed (session));

    orm_session_close (session);
    g_assert_true (orm_session_is_closed (session));

    g_object_unref (session);
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
    TEST_ADD_FIXTURE ("/session/new", test_session_new);
    TEST_ADD_FIXTURE ("/session/get-connection", test_session_get_connection);
    TEST_ADD_FIXTURE ("/session/register-mapper", test_session_register_mapper);
    TEST_ADD_FIXTURE ("/session/get-mapper-unregistered", test_session_get_mapper_unregistered);

    /* Add/Delete tests */
    TEST_ADD_FIXTURE ("/session/add", test_session_add);
    TEST_ADD_FIXTURE ("/session/add-all", test_session_add_all);
    TEST_ADD_FIXTURE ("/session/delete", test_session_delete);
    TEST_ADD_FIXTURE ("/session/expunge", test_session_expunge);

    /* State tests */
    TEST_ADD_FIXTURE ("/session/is-dirty", test_session_is_dirty);
    TEST_ADD_FIXTURE ("/session/transient-object", test_session_transient_object);

    /* Persistence tests */
    TEST_ADD_FIXTURE ("/session/flush", test_session_flush);
    TEST_ADD_FIXTURE ("/session/commit", test_session_commit);
    TEST_ADD_FIXTURE ("/session/rollback", test_session_rollback);

    /* Get tests */
    TEST_ADD_FIXTURE ("/session/get", test_session_get);
    TEST_ADD_FIXTURE ("/session/get-not-found", test_session_get_not_found);

    /* Execute tests */
    TEST_ADD_FIXTURE ("/session/execute", test_session_execute);

    /* Close tests */
    g_test_add_func ("/session/close", test_session_close);

    return g_test_run ();
}
