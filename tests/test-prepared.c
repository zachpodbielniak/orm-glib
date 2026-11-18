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
 * orm_connection_query_prepared() and its async form.
 *
 * On every backend the answer matches orm_connection_query_with_params().
 * The GFlags nick "prepared" is registered, which is what bindings look up.
 * On PostgreSQL (ORM_TEST_URL=postgresql://...) the server lists one
 * prepared statement per SQL text however often it runs, including the
 * async entry; a 65th text is not kept; DEALLOCATE ALL is prepared again;
 * a text that fails to prepare is reported and not listed.
 */

#include <glib.h>
#include <glib-object.h>
#include <string.h>
#include "orm.h"

#define DOUBLE_SQL "SELECT ($1::int * 2)::text"
#define TRIPLE_SQL "SELECT ($1::int * 3)::text"
#define SQLITE_SQL "SELECT CAST(? AS INTEGER) * 2"

/* At most this many texts are kept prepared on one PostgreSQL connection. */
#define PREPARED_CAP (64)

typedef struct
{
    GMainLoop *loop;
    OrmResult *result;
    GError    *error;
    gboolean   timed_out;
    guint      timeout_id;
} PreparedAsync;

static gboolean
prepared_async_timeout (gpointer data)
{
    PreparedAsync *state = (PreparedAsync *) data;

    state->timed_out = TRUE;
    state->timeout_id = 0;
    g_main_loop_quit (state->loop);

    return G_SOURCE_REMOVE;
}

static void
on_prepared_ready (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    PreparedAsync *state = (PreparedAsync *) user_data;

    state->result = orm_connection_query_prepared_finish (ORM_CONNECTION (source),
                                                          result,
                                                          &state->error);
    g_main_loop_quit (state->loop);
}

/*
 * Runs the async entry and returns its result.  The parameter list is
 * freed before the loop runs, so a missing copy shows up as a bad read.
 */
static OrmResult *
run_prepared_async (OrmConnection *connection,
                    const gchar   *sql,
                    const gchar   *value)
{
    PreparedAsync  state;
    GList         *params = NULL;

    state.loop = g_main_loop_new (NULL, FALSE);
    state.result = NULL;
    state.error = NULL;
    state.timed_out = FALSE;
    state.timeout_id = g_timeout_add_seconds (30, prepared_async_timeout, &state);

    if (value != NULL)
        params = g_list_append (NULL, orm_value_new_string (value));

    orm_connection_query_prepared_async (connection, sql, params, NULL,
                                         on_prepared_ready, &state);
    g_list_free_full (params, (GDestroyNotify) orm_value_free);

    g_main_loop_run (state.loop);

    if (state.timeout_id != 0)
        g_source_remove (state.timeout_id);
    g_main_loop_unref (state.loop);

    g_assert_false (state.timed_out);
    g_assert_no_error (state.error);
    g_assert_nonnull (state.result);

    return state.result;
}

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

/* The nick bindings look up, not only the C enumerator. */
static void
test_prepared_flag_registered (void)
{
    GFlagsClass *klass = g_type_class_ref (ORM_TYPE_QUERY_FLAGS);
    GFlagsValue *value = g_flags_get_value_by_nick (klass, "prepared");

    g_assert_nonnull (value);
    g_assert_cmpuint (value->value, ==, ORM_QUERY_FLAGS_PREPARED);
    g_type_class_unref (klass);
}

static gint64
run_sqlite_product (OrmConnection *connection,
                    gboolean       prepared)
{
    g_autoptr(GError)    error = NULL;
    g_autoptr(OrmResult) result = NULL;
    GList               *params = g_list_append (NULL, orm_value_new_string ("21"));

    if (prepared)
        result = orm_connection_query_prepared (connection, SQLITE_SQL, params, &error);
    else
        result = orm_connection_query_with_params (connection, SQLITE_SQL, params, &error);

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
    g_assert_no_error (error);
    g_assert_nonnull (result);
    g_assert_true (orm_result_next (result));

    return orm_row_get_integer (orm_result_get_row (result), 0);
}

/* SQLite: the same rows as an unprepared query, sync and async. */
static void
test_prepared_answers (void)
{
    g_autoptr(GError)        error = NULL;
    g_autoptr(OrmEngine)     engine = orm_engine_new ("sqlite:///:memory:", &error);
    g_autoptr(OrmConnection) connection = NULL;
    g_autoptr(OrmResult)     async_result = NULL;
    guint                    i;

    g_assert_no_error (error);
    connection = orm_engine_connect (engine, &error);
    g_assert_no_error (error);

    for (i = 0; i < 3; i++)
    {
        g_assert_cmpint (run_sqlite_product (connection, FALSE), ==, 42);
        g_assert_cmpint (run_sqlite_product (connection, TRUE), ==, 42);
    }

    async_result = run_prepared_async (connection, SQLITE_SQL, "21");
    g_assert_true (orm_result_next (async_result));
    g_assert_cmpint (orm_row_get_integer (orm_result_get_row (async_result), 0),
                     ==, 42);
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

    /*
     * The async entry prepares too.  A third text, used only there, is
     * listed once -- an unprepared async call would leave this at zero.
     */
    {
        g_autoptr(OrmResult) result = run_prepared_async (connection, TRIPLE_SQL, "7");

        g_assert_true (orm_result_next (result));
        g_assert_cmpstr (orm_row_get_string (orm_result_get_row (result), 0), ==, "21");
    }
    g_assert_cmpint (count_prepared (connection, "%($1::int * 3)%"), ==, 1);
    g_assert_cmpint (count_prepared (connection, "%($1::int * 2)%"), ==, 1);
}

/* The 65th distinct text runs, and is not kept. */
static void
test_prepared_cap (void)
{
    g_autoptr(OrmEngine)     engine = NULL;
    g_autoptr(OrmConnection) connection = postgres_connection (&engine);
    guint                    i;

    if (connection == NULL)
    {
        g_test_skip ("needs ORM_TEST_URL=postgresql://...");
        return;
    }

    for (i = 0; i < PREPARED_CAP + 1; i++)
    {
        g_autofree gchar    *sql = g_strdup_printf ("SELECT %u::int", i);
        g_autoptr(GError)    error = NULL;
        g_autoptr(OrmResult) result = NULL;

        result = orm_connection_query_prepared (connection, sql, NULL, &error);
        g_assert_no_error (error);
        g_assert_true (orm_result_next (result));
        g_assert_cmpint (orm_row_get_integer (orm_result_get_row (result), 0),
                         ==, (gint) i);
    }

    g_assert_cmpint (count_prepared (connection, "SELECT %::int"), ==, PREPARED_CAP);
}

/* A statement the server no longer has is prepared again. */
static void
test_prepared_after_deallocate (void)
{
    g_autoptr(OrmEngine)     engine = NULL;
    g_autoptr(OrmConnection) connection = postgres_connection (&engine);
    g_autoptr(GError)        error = NULL;
    g_autofree gchar        *first = NULL;
    g_autofree gchar        *second = NULL;

    if (connection == NULL)
    {
        g_test_skip ("needs ORM_TEST_URL=postgresql://...");
        return;
    }
    first = run_double (connection, "4");
    g_assert_true (orm_connection_execute (connection, "DEALLOCATE ALL", &error));
    g_assert_no_error (error);
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
    g_assert_cmpint (count_prepared (connection, "%no_such_column%"), ==, 0);
    answer = run_double (connection, "1");
    g_assert_cmpstr (answer, ==, "2");
}

int
main (int    argc,
      char **argv)
{
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/prepared/flag-registered", test_prepared_flag_registered);
    g_test_add_func ("/prepared/answers", test_prepared_answers);
    g_test_add_func ("/prepared/once-per-text", test_prepared_once);
    g_test_add_func ("/prepared/cap", test_prepared_cap);
    g_test_add_func ("/prepared/after-deallocate", test_prepared_after_deallocate);
    g_test_add_func ("/prepared/error", test_prepared_error);
    return g_test_run ();
}
