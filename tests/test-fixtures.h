/* test-fixtures.h
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

#ifndef TEST_FIXTURES_H
#define TEST_FIXTURES_H

#include <glib.h>
#include <glib-object.h>

#define ORM_INSIDE
#include "orm.h"
#undef ORM_INSIDE

G_BEGIN_DECLS

/*
 * TestDbFixture:
 * @engine: The database engine
 * @connection: The database connection
 * @session: The ORM session
 *
 * Shared test fixture for database tests.
 */
typedef struct
{
    OrmEngine     *engine;
    OrmConnection *connection;
    OrmSession    *session;
} TestDbFixture;

/*
 * test_db_fixture_setup:
 * @fixture: The fixture to set up
 * @user_data: Unused
 *
 * Sets up a test fixture with an in-memory SQLite database.
 */
void    test_db_fixture_setup       (TestDbFixture *fixture,
                                     gconstpointer  user_data);

/*
 * test_db_fixture_teardown:
 * @fixture: The fixture to tear down
 * @user_data: Unused
 *
 * Tears down the test fixture and releases resources.
 */
void    test_db_fixture_teardown    (TestDbFixture *fixture,
                                     gconstpointer  user_data);

/*
 * test_create_memory_engine:
 *
 * Creates an in-memory SQLite engine for testing.
 *
 * Returns: (transfer full): A new OrmEngine
 */
OrmEngine *     test_create_memory_engine   (void);

/*
 * test_create_users_table:
 * @connection: A database connection
 * @error: Return location for error
 *
 * Creates the test users table in the database.
 *
 * Returns: %TRUE on success
 */
gboolean        test_create_users_table     (OrmConnection  *connection,
                                             GError        **error);

/*
 * test_create_posts_table:
 * @connection: A database connection
 * @error: Return location for error
 *
 * Creates the test posts table in the database.
 *
 * Returns: %TRUE on success
 */
gboolean        test_create_posts_table     (OrmConnection  *connection,
                                             GError        **error);

/*
 * test_insert_sample_users:
 * @connection: A database connection
 * @error: Return location for error
 *
 * Inserts sample user data into the test database.
 *
 * Returns: %TRUE on success
 */
gboolean        test_insert_sample_users    (OrmConnection  *connection,
                                             GError        **error);

/*
 * test_insert_sample_posts:
 * @connection: A database connection
 * @error: Return location for error
 *
 * Inserts sample post data into the test database.
 *
 * Returns: %TRUE on success
 */
gboolean        test_insert_sample_posts    (OrmConnection  *connection,
                                             GError        **error);

/*
 * Convenience macros for fixture-based tests.
 */
#define TEST_ADD_FIXTURE(path, func) \
    g_test_add (path, TestDbFixture, NULL, \
                test_db_fixture_setup, func, test_db_fixture_teardown)

G_END_DECLS

#endif /* TEST_FIXTURES_H */
