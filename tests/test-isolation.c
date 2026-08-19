/* test-isolation.c
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

/*
 * Isolation levels over SQLite, which is the interesting case precisely
 * because SQLite has no isolation-level statement at all.  Two of the four
 * levels have to be refused, and the other two have to succeed without
 * emitting SQL the backend would reject -- so "does nothing, reports
 * success" and "reports failure" are both correct answers here depending
 * on the level, and getting them the wrong way round is silent data-
 * integrity loss.
 *
 * The PostgreSQL and MySQL paths emit real statements and are covered by
 * the live-backend suite.
 */

#include <glib.h>
#include <glib-object.h>

#include "test-fixtures.h"

/*
 * SQLite is serializable natively, so asking for it must succeed without
 * touching the connection.
 */
static void
test_isolation_sqlite_serializable (TestDbFixture *fixture,
                                    gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    g_assert_true (orm_connection_set_isolation_level (
                       fixture->connection, ORM_ISOLATION_SERIALIZABLE, &error));
    g_assert_no_error (error);
    g_assert_cmpint (orm_connection_get_isolation_level (fixture->connection),
                     ==, ORM_ISOLATION_SERIALIZABLE);
}

/*
 * READ UNCOMMITTED is reachable through a pragma rather than a statement.
 */
static void
test_isolation_sqlite_read_uncommitted (TestDbFixture *fixture,
                                        gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    g_assert_true (orm_connection_set_isolation_level (
                       fixture->connection, ORM_ISOLATION_READ_UNCOMMITTED, &error));
    g_assert_no_error (error);
    g_assert_cmpint (orm_connection_get_isolation_level (fixture->connection),
                     ==, ORM_ISOLATION_READ_UNCOMMITTED);
}

/*
 * The two levels SQLite cannot express must fail loudly.  Silently
 * accepting READ COMMITTED here would tell a caller it has weaker
 * isolation than it really has, which is the harmless direction; silently
 * accepting REPEATABLE READ on a backend that did not provide it would be
 * the harmful one.  Neither is acceptable, so both are errors.
 */
static void
test_isolation_sqlite_unsupported (TestDbFixture *fixture,
                                   gconstpointer  user_data)
{
    g_autoptr(GError) read_committed = NULL;
    g_autoptr(GError) repeatable = NULL;

    g_assert_false (orm_connection_set_isolation_level (
                        fixture->connection, ORM_ISOLATION_READ_COMMITTED,
                        &read_committed));
    g_assert_error (read_committed, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED);

    g_assert_false (orm_connection_set_isolation_level (
                        fixture->connection, ORM_ISOLATION_REPEATABLE_READ,
                        &repeatable));
    g_assert_error (repeatable, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED);
}

/*
 * A failed set must not move the tracked level: the getter reports what
 * the connection is actually using, and nothing changed.
 */
static void
test_isolation_failed_set_keeps_level (TestDbFixture *fixture,
                                       gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    g_assert_cmpint (orm_connection_get_isolation_level (fixture->connection),
                     ==, ORM_ISOLATION_SERIALIZABLE);

    g_assert_false (orm_connection_set_isolation_level (
                        fixture->connection, ORM_ISOLATION_READ_COMMITTED, &error));

    g_assert_cmpint (orm_connection_get_isolation_level (fixture->connection),
                     ==, ORM_ISOLATION_SERIALIZABLE);
}

static void
test_isolation_begin_with_isolation (TestDbFixture *fixture,
                                     gconstpointer  user_data)
{
    g_autoptr(OrmTransaction) transaction = NULL;
    g_autoptr(GError)         error = NULL;

    transaction = orm_connection_begin_transaction_with_isolation (
        fixture->connection, ORM_ISOLATION_SERIALIZABLE, &error);

    g_assert_no_error (error);
    g_assert_nonnull (transaction);
    g_assert_true (orm_transaction_is_active (transaction));
    g_assert_true (orm_connection_in_transaction (fixture->connection));

    g_assert_true (orm_transaction_commit (transaction, &error));
    g_assert_no_error (error);
}

/*
 * When the level cannot be honoured, no transaction may be opened: a
 * caller that got a transaction back would reasonably assume it runs at
 * the level it asked for.
 */
static void
test_isolation_begin_unsupported_opens_nothing (TestDbFixture *fixture,
                                                gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;
    OrmTransaction   *transaction;

    transaction = orm_connection_begin_transaction_with_isolation (
        fixture->connection, ORM_ISOLATION_REPEATABLE_READ, &error);

    g_assert_null (transaction);
    g_assert_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED);
    g_assert_false (orm_connection_in_transaction (fixture->connection));
}

gint
main (gint    argc,
      gchar **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add ("/isolation/sqlite/serializable", TestDbFixture, NULL,
                test_db_fixture_setup, test_isolation_sqlite_serializable,
                test_db_fixture_teardown);
    g_test_add ("/isolation/sqlite/read-uncommitted", TestDbFixture, NULL,
                test_db_fixture_setup, test_isolation_sqlite_read_uncommitted,
                test_db_fixture_teardown);
    g_test_add ("/isolation/sqlite/unsupported", TestDbFixture, NULL,
                test_db_fixture_setup, test_isolation_sqlite_unsupported,
                test_db_fixture_teardown);
    g_test_add ("/isolation/failed-set-keeps-level", TestDbFixture, NULL,
                test_db_fixture_setup, test_isolation_failed_set_keeps_level,
                test_db_fixture_teardown);
    g_test_add ("/isolation/begin-with-isolation", TestDbFixture, NULL,
                test_db_fixture_setup, test_isolation_begin_with_isolation,
                test_db_fixture_teardown);
    g_test_add ("/isolation/begin-unsupported-opens-nothing", TestDbFixture, NULL,
                test_db_fixture_setup, test_isolation_begin_unsupported_opens_nothing,
                test_db_fixture_teardown);

    return g_test_run ();
}
