/* test-prepared.c
 *
 * Copyright 2026 Zach Podbielniak
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
 * orm_connection_query_prepared().
 *
 * On every backend the answer is the same as orm_connection_query_with_params().
 * On PostgreSQL (ORM_TEST_URL=postgresql://...) the statement is prepared once
 * per connection and SQL text: the server lists exactly one prepared statement
 * for it however often it runs, a second text gets its own, and a statement
 * the server dropped (DEALLOCATE ALL) is prepared again instead of failing.
 */

#include <glib.h>
#include <string.h>
#include "orm.h"

#define DOUBLE_SQL "SELECT ($1::int * 2)::text"

static gchar *
run_double (OrmConnection *connection,
            const gchar   *value)
{
    g_autoptr(GError)    error = NULL;
    g_autoptr(OrmResult) result = NULL;
    GList               *params = g_list_append (NULL, orm_value_new_string (value));
    gchar               *answer;

    result = orm_connection_query_prepared (connection, DOUBLE_SQL, params, &error);
    g_list_free_full (params, (GDestroyNotify) orm_value_free);
    g_assert_no_error (error);
    g_assert_nonnull (result);
    g_assert_true (orm_result_next (result));
    answer = g_strdup (orm_row_get_string (orm_result_get_row (result), 0));
    return answer;
}

static gint64
count_prepared (OrmConnection *connection,
                const gchar   *like)
{
    g_autoptr(GError)    error = NULL;
    g_autoptr(OrmResult) result = NULL;
    GList               *params = g_list_append (NULL, orm_value_new_string (like));
    gint64               count;

    result = orm_connection_query_with_params (connection,
        "SELECT count(*) FROM pg_prepared_statements WHERE statement LIKE $1",
        params, &error);
    g_list_free_full (params, (GDestroyNotify) orm_value_free);
    g_assert_no_error (error);
    g_assert_true (orm_result_next (result));
    count = orm_row_get_integer (orm_result_get_row (result), 0);
    return count;
}

/* SQLite and any backend: the same rows as an unprepared query. */
static void
test_prepared_answers (void)
{
    g_autoptr(GError)        error = NULL;
    g_autoptr(OrmEngine)     engine = orm_engine_new ("sqlite:///:memory:", &error);
    g_autoptr(OrmConnection) connection = NULL;
    guint                    i;

    g_assert_no_error (error);
    connection = orm_engine_connect (engine, &error);
    g_assert_no_error (error);
    for (i = 0; i < 3; i++)
    {
        g_autofree gchar *answer = NULL;
        g_autoptr(GError) local = NULL;
        g_autoptr(OrmResult) result = NULL;
        GList *params = g_list_append (NULL, orm_value_new_string ("21"));

        result = orm_connection_query_prepared (connection, "SELECT CAST(? AS INTEGER) * 2", params, &local);
        g_list_free_full (params, (GDestroyNotify) orm_value_free);
        g_assert_no_error (local);
        g_assert_true (orm_result_next (result));
        g_assert_cmpint (orm_row_get_integer (orm_result_get_row (result), 0), ==, 42);
    }
}

static OrmConnection *
postgres_connection (OrmEngine **engine)
{
    const gchar      *url = g_getenv ("ORM_TEST_URL");
    g_autoptr(GError) error = NULL;
    OrmConnection    *connection;

    if (url == NULL || !g_str_has_prefix (url, "postgres"))
        return NULL;
    *engine = orm_engine_new (url, &error);
    g_assert_no_error (error);
    connection = orm_engine_connect (*engine, &error);
    g_assert_no_error (error);
    return connection;
}

/* One prepared statement per connection and SQL text, reused. */
static void
test_prepared_once (void)
{
    g_autoptr(OrmEngine)     engine = NULL;
    g_autoptr(OrmConnection) connection = postgres_connection (&engine);
    guint                    i;

    if (connection == NULL)
    {
        g_test_skip ("needs ORM_TEST_URL=postgresql://...");
        return;
    }
    for (i = 0; i < 5; i++)
    {
        g_autofree gchar *answer = run_double (connection, "21");
        g_assert_cmpstr (answer, ==, "42");
    }
    g_assert_cmpint (count_prepared (connection, "%($1::int * 2)%"), ==, 1);

    /* Another text is another statement; the first stays. */
    {
        g_autoptr(GError) error = NULL;
        g_autoptr(OrmResult) result = orm_connection_query_prepared (connection, "SELECT 'other'::text", NULL, &error);
        g_assert_no_error (error);
        g_assert_true (orm_result_next (result));
    }
    g_assert_cmpint (count_prepared (connection, "%($1::int * 2)%"), ==, 1);
    g_assert_cmpint (count_prepared (connection, "%'other'%"), ==, 1);
}

/* A statement the server no longer has is prepared again. */
static void
test_prepared_after_deallocate (void)
{
    g_autoptr(OrmEngine)     engine = NULL;
    g_autoptr(OrmConnection) connection = postgres_connection (&engine);
    g_autofree gchar        *first = NULL, *second = NULL;

    if (connection == NULL)
    {
        g_test_skip ("needs ORM_TEST_URL=postgresql://...");
        return;
    }
    first = run_double (connection, "4");
    g_assert_true (orm_connection_execute (connection, "DEALLOCATE ALL", NULL));
    second = run_double (connection, "5");
    g_assert_cmpstr (first, ==, "8");
    g_assert_cmpstr (second, ==, "10");
    g_assert_cmpint (count_prepared (connection, "%($1::int * 2)%"), ==, 1);
}

/* A failing statement is reported, and the connection still works. */
static void
test_prepared_error (void)
{
    g_autoptr(OrmEngine)     engine = NULL;
    g_autoptr(OrmConnection) connection = postgres_connection (&engine);
    g_autoptr(GError)        error = NULL;
    g_autoptr(OrmResult)     bad = NULL;
    g_autofree gchar        *answer = NULL;

    if (connection == NULL)
    {
        g_test_skip ("needs ORM_TEST_URL=postgresql://...");
        return;
    }
    bad = orm_connection_query_prepared (connection, "SELECT no_such_column", NULL, &error);
    g_assert_null (bad);
    g_assert_error (error, ORM_ERROR, ORM_ERROR_EXECUTE);
    answer = run_double (connection, "1");
    g_assert_cmpstr (answer, ==, "2");
}

int
main (int    argc,
      char **argv)
{
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/prepared/answers", test_prepared_answers);
    g_test_add_func ("/prepared/once-per-text", test_prepared_once);
    g_test_add_func ("/prepared/after-deallocate", test_prepared_after_deallocate);
    g_test_add_func ("/prepared/error", test_prepared_error);
    return g_test_run ();
}
