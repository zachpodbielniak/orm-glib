/* test-fixtures.c
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

#include "test-fixtures.h"

/*
 * test_create_memory_engine:
 *
 * Creates an in-memory SQLite engine for testing.
 */
OrmEngine *
test_create_memory_engine (void)
{
    g_autoptr(GError) error = NULL;
    OrmEngine *engine;

    engine = orm_engine_new ("sqlite:///:memory:", &error);
    if (error != NULL)
    {
        g_warning ("Failed to create memory engine: %s", error->message);
        return NULL;
    }

    return engine;
}

/*
 * test_db_fixture_setup:
 *
 * Sets up a test fixture with an in-memory SQLite database.
 */
void
test_db_fixture_setup (TestDbFixture *fixture,
                       gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    (void) user_data;

    fixture->engine = test_create_memory_engine ();
    g_assert_nonnull (fixture->engine);

    fixture->connection = orm_engine_connect (fixture->engine, &error);
    g_assert_no_error (error);
    g_assert_nonnull (fixture->connection);

    fixture->session = orm_session_new_with_connection (fixture->connection);
    g_assert_nonnull (fixture->session);
}

/*
 * test_db_fixture_teardown:
 *
 * Tears down the test fixture and releases resources.
 */
void
test_db_fixture_teardown (TestDbFixture *fixture,
                          gconstpointer  user_data)
{
    (void) user_data;

    if (fixture->session != NULL)
    {
        orm_session_close (fixture->session);
        g_object_unref (fixture->session);
        fixture->session = NULL;
    }

    if (fixture->connection != NULL)
    {
        orm_connection_close (fixture->connection);
        g_object_unref (fixture->connection);
        fixture->connection = NULL;
    }

    if (fixture->engine != NULL)
    {
        g_object_unref (fixture->engine);
        fixture->engine = NULL;
    }
}

/*
 * test_create_users_table:
 *
 * Creates the test users table in the database.
 */
gboolean
test_create_users_table (OrmConnection  *connection,
                         GError        **error)
{
    const gchar *sql =
        "CREATE TABLE IF NOT EXISTS test_user ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name TEXT NOT NULL,"
        "    email TEXT UNIQUE,"
        "    active INTEGER DEFAULT 1,"
        "    created_at TEXT"
        ")";

    return orm_connection_execute (connection, sql, error);
}

/*
 * test_create_posts_table:
 *
 * Creates the test posts table in the database.
 */
gboolean
test_create_posts_table (OrmConnection  *connection,
                         GError        **error)
{
    const gchar *sql =
        "CREATE TABLE IF NOT EXISTS test_post ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    title TEXT NOT NULL,"
        "    content TEXT,"
        "    user_id INTEGER,"
        "    FOREIGN KEY (user_id) REFERENCES test_user(id)"
        ")";

    return orm_connection_execute (connection, sql, error);
}

/*
 * test_insert_sample_users:
 *
 * Inserts sample user data into the test database.
 */
gboolean
test_insert_sample_users (OrmConnection  *connection,
                          GError        **error)
{
    const gchar *sql =
        "INSERT INTO test_user (name, email, active, created_at) VALUES "
        "('Alice', 'alice@example.com', 1, '2025-01-01T00:00:00Z'),"
        "('Bob', 'bob@example.com', 1, '2025-01-02T00:00:00Z'),"
        "('Charlie', 'charlie@example.com', 0, '2025-01-03T00:00:00Z')";

    return orm_connection_execute (connection, sql, error);
}

/*
 * test_insert_sample_posts:
 *
 * Inserts sample post data into the test database.
 */
gboolean
test_insert_sample_posts (OrmConnection  *connection,
                          GError        **error)
{
    const gchar *sql =
        "INSERT INTO test_post (title, content, user_id) VALUES "
        "('Hello World', 'This is my first post.', 1),"
        "('Second Post', 'Another post by Alice.', 1),"
        "('Bob''s Post', 'Bob writes something.', 2)";

    return orm_connection_execute (connection, sql, error);
}
