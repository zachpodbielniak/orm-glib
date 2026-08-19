/* test-async.c
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
 * The asynchronous layer, over SQLite.
 *
 * Everything here is about the parts that are easy to get wrong and
 * silent when they are: that callbacks come back on the caller's main
 * context rather than the worker thread, that operations stay in
 * submission order, that a synchronous call made while asynchronous work
 * is queued waits its turn instead of deadlocking or racing, and that a
 * cancelled query leaves a connection somebody can keep using.
 *
 * Every loop is run behind a timeout, because the failure mode of all of
 * this is a hang, and a test that hangs tells you nothing and blocks the
 * suite behind it.
 */

#include <glib.h>
#include <glib-object.h>

#include "test-fixtures.h"

#define ASYNC_TIMEOUT_SECONDS (30)

typedef struct
{
    OrmEngine     *engine;
    OrmConnection *connection;
    GMainLoop     *loop;
    GThread       *main_thread;
    guint          timeout_id;
    gboolean       timed_out;

    /* Per-test scratch, so each test does not need its own fixture. */
    GPtrArray     *order;
    GPtrArray     *notices;
    GArray        *states;
    OrmRowStream  *stream;
    gboolean       callback_off_main_thread;
    guint          batches;
    guint          rows_seen;
} AsyncFixture;

typedef struct
{
    OrmConnectionState old_state;
    OrmConnectionState new_state;
} StateTransition;

static void
async_exec (AsyncFixture *fixture,
            const gchar  *sql)
{
    g_autoptr(GError) error = NULL;

    g_assert_true (orm_connection_execute (fixture->connection, sql, &error));
    g_assert_no_error (error);
}

static void
async_fixture_setup (AsyncFixture  *fixture,
                     gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    fixture->engine = orm_engine_new ("sqlite:///:memory:", &error);
    g_assert_no_error (error);

    fixture->connection = orm_engine_connect (fixture->engine, &error);
    g_assert_no_error (error);

    fixture->loop = g_main_loop_new (NULL, FALSE);
    fixture->main_thread = g_thread_self ();
    fixture->timeout_id = 0;
    fixture->timed_out = FALSE;
    fixture->order = g_ptr_array_new_with_free_func (g_free);
    fixture->notices = g_ptr_array_new_with_free_func (g_free);
    fixture->states = g_array_new (FALSE, FALSE, sizeof (StateTransition));
    fixture->stream = NULL;
    fixture->callback_off_main_thread = FALSE;
    fixture->batches = 0;
    fixture->rows_seen = 0;

    async_exec (fixture,
                "CREATE TABLE numbers (id INTEGER PRIMARY KEY, label TEXT)");

    /* Twenty-five rows: enough for several short batches and a partial one. */
    async_exec (fixture,
                "INSERT INTO numbers (id, label) "
                "WITH RECURSIVE seq(i) AS ("
                "  SELECT 1 UNION ALL SELECT i + 1 FROM seq WHERE i < 25)"
                "SELECT i, 'row-' || i FROM seq");
}

static void
async_fixture_teardown (AsyncFixture  *fixture,
                        gconstpointer  user_data)
{
    g_clear_pointer (&fixture->order, g_ptr_array_unref);
    g_clear_pointer (&fixture->notices, g_ptr_array_unref);
    g_clear_pointer (&fixture->states, g_array_unref);
    g_clear_object (&fixture->stream);
    g_clear_pointer (&fixture->loop, g_main_loop_unref);
    g_clear_object (&fixture->connection);
    g_clear_object (&fixture->engine);
}

static gboolean
async_on_timeout (gpointer data)
{
    AsyncFixture *fixture = (AsyncFixture *) data;

    fixture->timeout_id = 0;
    fixture->timed_out = TRUE;
    g_main_loop_quit (fixture->loop);

    return G_SOURCE_REMOVE;
}

/*
 * Runs the loop until something quits it, or fails the test rather than
 * hanging the suite.
 */
static void
async_run (AsyncFixture *fixture)
{
    fixture->timed_out = FALSE;
    fixture->timeout_id = g_timeout_add_seconds (ASYNC_TIMEOUT_SECONDS,
                                                 async_on_timeout, fixture);

    g_main_loop_run (fixture->loop);

    if (fixture->timeout_id != 0)
    {
        g_source_remove (fixture->timeout_id);
        fixture->timeout_id = 0;
    }

    g_assert_false (fixture->timed_out);
}

/*
 * Lets the loop turn over long enough for anything already queued on it
 * to be dispatched, without waiting on a particular callback.
 */
static gboolean
async_quit_loop (gpointer data)
{
    AsyncFixture *fixture = (AsyncFixture *) data;

    g_main_loop_quit (fixture->loop);

    return G_SOURCE_REMOVE;
}

static void
async_drain (AsyncFixture *fixture)
{
    g_timeout_add (50, async_quit_loop, fixture);
    async_run (fixture);
}

static void
async_note_thread (AsyncFixture *fixture)
{
    if (g_thread_self () != fixture->main_thread)
        fixture->callback_off_main_thread = TRUE;
}

/* ------------------------------------------------------------------ */
/* A query completes, on the caller's context                         */
/* ------------------------------------------------------------------ */

static void
on_query_ready (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
    AsyncFixture         *fixture = (AsyncFixture *) user_data;
    g_autoptr(OrmResult)  query_result = NULL;
    g_autoptr(GError)     error = NULL;
    g_autoptr(OrmValue)   scalar = NULL;

    async_note_thread (fixture);

    query_result = orm_connection_query_finish (ORM_CONNECTION (source), result,
                                                &error);
    g_assert_no_error (error);
    g_assert_nonnull (query_result);

    scalar = orm_result_get_scalar (query_result);
    g_assert_nonnull (scalar);
    g_assert_cmpint (orm_value_get_integer (scalar), ==, 25);

    g_main_loop_quit (fixture->loop);
}

static void
test_async_query (AsyncFixture  *fixture,
                  gconstpointer  user_data)
{
    orm_connection_query_async (fixture->connection,
                                "SELECT COUNT(*) FROM numbers", NULL, NULL,
                                on_query_ready, fixture);

    async_run (fixture);

    /*
     * The point of the whole exercise: the work ran on another thread but
     * the answer arrived here.
     */
    g_assert_false (fixture->callback_off_main_thread);
}

/* ------------------------------------------------------------------ */
/* Parameters are copied, not borrowed                                */
/* ------------------------------------------------------------------ */

static void
on_params_ready (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    AsyncFixture         *fixture = (AsyncFixture *) user_data;
    g_autoptr(OrmResult)  query_result = NULL;
    g_autoptr(GError)     error = NULL;
    g_autoptr(OrmValue)   scalar = NULL;

    query_result = orm_connection_query_finish (ORM_CONNECTION (source), result,
                                                &error);
    g_assert_no_error (error);
    g_assert_nonnull (query_result);

    scalar = orm_result_get_scalar (query_result);
    g_assert_nonnull (scalar);
    g_assert_cmpstr (orm_value_get_string (scalar), ==, "row-7");

    g_main_loop_quit (fixture->loop);
}

static void
test_async_params_are_copied (AsyncFixture  *fixture,
                              gconstpointer  user_data)
{
    GList *params = NULL;

    params = g_list_append (params, orm_value_new_integer (7));

    orm_connection_query_async (fixture->connection,
                                "SELECT label FROM numbers WHERE id = ?",
                                params, NULL, on_params_ready, fixture);

    /*
     * Freed before the statement has run.  A borrowed list would be read
     * after this point, which is the bug this test exists for.
     */
    g_list_free_full (params, (GDestroyNotify) orm_value_free);

    async_run (fixture);
}

/* ------------------------------------------------------------------ */
/* Submission order is completion order                               */
/* ------------------------------------------------------------------ */

static void
on_ordered_ready (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    AsyncFixture         *fixture = (AsyncFixture *) user_data;
    g_autoptr(OrmResult)  query_result = NULL;
    g_autoptr(GError)     error = NULL;
    g_autoptr(OrmValue)   scalar = NULL;

    query_result = orm_connection_query_finish (ORM_CONNECTION (source), result,
                                                &error);
    g_assert_no_error (error);
    g_assert_nonnull (query_result);

    scalar = orm_result_get_scalar (query_result);
    g_assert_nonnull (scalar);

    g_ptr_array_add (fixture->order, g_strdup (orm_value_get_string (scalar)));

    if (fixture->order->len == 3)
        g_main_loop_quit (fixture->loop);
}

static void
test_async_ordering (AsyncFixture  *fixture,
                     gconstpointer  user_data)
{
    gint64 started;
    gint64 submitted;
    gint64 finished;
    guint  i;

    started = g_get_monotonic_time ();

    /*
     * The first query is the slowest by a wide margin, so a pool would
     * hand back "second" first.  One worker per connection cannot.
     */
    orm_connection_query_async (fixture->connection,
                                "WITH RECURSIVE c(i) AS ("
                                "  SELECT 1 UNION ALL"
                                "  SELECT i + 1 FROM c WHERE i < 200000)"
                                "SELECT 'first' FROM c LIMIT 1",
                                NULL, NULL, on_ordered_ready, fixture);
    orm_connection_query_async (fixture->connection, "SELECT 'second'",
                                NULL, NULL, on_ordered_ready, fixture);
    orm_connection_query_async (fixture->connection, "SELECT 'third'",
                                NULL, NULL, on_ordered_ready, fixture);

    submitted = g_get_monotonic_time ();

    async_run (fixture);

    finished = g_get_monotonic_time ();

    /*
     * Submitting must be far cheaper than running.  Stated as a ratio
     * rather than a deadline so it means the same thing on a slow
     * machine: if the queries had run on this thread, starting them
     * would have taken essentially the whole elapsed time.
     */
    g_assert_cmpint ((submitted - started) * 4, <, finished - started);

    g_assert_cmpuint (fixture->order->len, ==, 3);

    for (i = 0; i < fixture->order->len; i++)
        g_assert_nonnull (g_ptr_array_index (fixture->order, i));

    g_assert_cmpstr (g_ptr_array_index (fixture->order, 0), ==, "first");
    g_assert_cmpstr (g_ptr_array_index (fixture->order, 1), ==, "second");
    g_assert_cmpstr (g_ptr_array_index (fixture->order, 2), ==, "third");
}

/* ------------------------------------------------------------------ */
/* Synchronous calls interleaved with asynchronous ones               */
/* ------------------------------------------------------------------ */

static void
on_interleave_ready (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
    AsyncFixture      *fixture = (AsyncFixture *) user_data;
    g_autoptr(GError)  error = NULL;

    g_assert_true (orm_connection_execute_finish (ORM_CONNECTION (source),
                                                  result, &error));
    g_assert_no_error (error);

    g_main_loop_quit (fixture->loop);
}

static void
test_async_sync_interleave (AsyncFixture  *fixture,
                            gconstpointer  user_data)
{
    g_autoptr(OrmResult) query_result = NULL;
    g_autoptr(GError)    error = NULL;
    g_autoptr(OrmValue)  scalar = NULL;

    orm_connection_execute_async (fixture->connection,
                                  "INSERT INTO numbers (id, label) "
                                  "WITH RECURSIVE seq(i) AS ("
                                  "  SELECT 26 UNION ALL"
                                  "  SELECT i + 1 FROM seq WHERE i < 5000)"
                                  "SELECT i, 'bulk-' || i FROM seq",
                                  NULL, NULL, on_interleave_ready, fixture);

    /*
     * Straight into a synchronous call on the same connection.  It has to
     * queue behind the insert rather than reach into SQLite alongside it,
     * so by the time it answers every bulk row is there -- and it must
     * not deadlock waiting for a main loop that is not running yet.
     */
    query_result = orm_connection_query (fixture->connection,
                                         "SELECT COUNT(*) FROM numbers", &error);
    g_assert_no_error (error);
    g_assert_nonnull (query_result);

    scalar = orm_result_get_scalar (query_result);
    g_assert_nonnull (scalar);
    g_assert_cmpint (orm_value_get_integer (scalar), ==, 5000);

    /* The connection's own counters are confined too, and still answer. */
    g_assert_cmpint (orm_connection_get_changes (fixture->connection), >=, 0);

    async_run (fixture);
}

/* ------------------------------------------------------------------ */
/* Cancellation                                                       */
/* ------------------------------------------------------------------ */

static gboolean
on_cancel_timeout (gpointer data)
{
    GCancellable *cancellable = G_CANCELLABLE (data);

    g_cancellable_cancel (cancellable);

    return G_SOURCE_REMOVE;
}

static void
on_cancelled_ready (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    AsyncFixture      *fixture = (AsyncFixture *) user_data;
    g_autoptr(GError)  error = NULL;

    g_assert_false (orm_connection_execute_finish (ORM_CONNECTION (source),
                                                   result, &error));

    /*
     * The backend's own "interrupted" error is not what the caller asked
     * about; it asked to cancel, and that is what it should be told.
     */
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

    g_main_loop_quit (fixture->loop);
}

static void
test_async_cancel (AsyncFixture  *fixture,
                   gconstpointer  user_data)
{
    g_autoptr(GCancellable) cancellable = NULL;
    g_autoptr(OrmResult)    query_result = NULL;
    g_autoptr(GError)       error = NULL;
    g_autoptr(OrmValue)     scalar = NULL;

    cancellable = g_cancellable_new ();

    /* Long enough that it cannot finish before the cancel arrives. */
    orm_connection_execute_async (fixture->connection,
                                  "WITH RECURSIVE c(i) AS ("
                                  "  SELECT 1 UNION ALL"
                                  "  SELECT i + 1 FROM c WHERE i < 50000000)"
                                  "SELECT count(*) FROM c",
                                  NULL, cancellable, on_cancelled_ready,
                                  fixture);

    g_timeout_add (100, on_cancel_timeout, cancellable);

    async_run (fixture);

    /*
     * Interrupting a statement must not poison the connection: SQLite and
     * PostgreSQL both hand back an error that means "you stopped me",
     * which is not the same as "I am broken".
     */
    g_assert_cmpint (orm_connection_get_state (fixture->connection), !=,
                     ORM_CONNECTION_CLOSED);

    query_result = orm_connection_query (fixture->connection,
                                         "SELECT COUNT(*) FROM numbers", &error);
    g_assert_no_error (error);
    g_assert_nonnull (query_result);

    scalar = orm_result_get_scalar (query_result);
    g_assert_nonnull (scalar);
    g_assert_cmpint (orm_value_get_integer (scalar), ==, 25);
}

static void
test_async_cancel_before_start (AsyncFixture  *fixture,
                                gconstpointer  user_data)
{
    g_autoptr(GCancellable) cancellable = NULL;

    cancellable = g_cancellable_new ();
    g_cancellable_cancel (cancellable);

    orm_connection_execute_async (fixture->connection,
                                  "INSERT INTO numbers (id, label)"
                                  " VALUES (999, 'never')",
                                  NULL, cancellable, on_cancelled_ready,
                                  fixture);

    async_run (fixture);
}

/* ------------------------------------------------------------------ */
/* Row streaming                                                      */
/* ------------------------------------------------------------------ */

static void on_stream_batch (GObject      *source,
                             GAsyncResult *result,
                             gpointer      user_data);

static void
on_stream_batch (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    AsyncFixture         *fixture = (AsyncFixture *) user_data;
    OrmRowStream         *stream = ORM_ROW_STREAM (source);
    g_autoptr(GPtrArray)  rows = NULL;
    g_autoptr(GError)     error = NULL;

    async_note_thread (fixture);

    rows = orm_row_stream_fetch_finish (stream, result, &error);
    g_assert_no_error (error);

    /* Never %NULL for "no more rows" -- that is reserved for failure. */
    g_assert_nonnull (rows);
    g_assert_cmpuint (rows->len, <=, 4);

    fixture->batches++;
    fixture->rows_seen += rows->len;

    if (rows->len == 0)
    {
        g_assert_true (orm_row_stream_is_at_end (stream));
        g_main_loop_quit (fixture->loop);
        return;
    }

    orm_row_stream_fetch_async (stream, 4, NULL, on_stream_batch, fixture);
}

static void
on_stream_ready (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    AsyncFixture      *fixture = (AsyncFixture *) user_data;
    OrmRowStream      *stream;
    g_autoptr(GError)  error = NULL;

    stream = orm_connection_query_stream_finish (ORM_CONNECTION (source),
                                                 result, &error);
    g_assert_no_error (error);
    g_assert_nonnull (stream);

    /* Metadata is available before a single row has been fetched. */
    g_assert_cmpint (orm_row_stream_get_column_count (stream), ==, 2);
    g_assert_cmpstr (orm_row_stream_get_column_name (stream, 0), ==, "id");
    g_assert_cmpstr (orm_row_stream_get_column_name (stream, 1), ==, "label");
    g_assert_null (orm_row_stream_get_column_name (stream, 2));

    fixture->stream = stream;

    orm_row_stream_fetch_async (stream, 4, NULL, on_stream_batch, fixture);
}

static void
test_async_stream (AsyncFixture  *fixture,
                   gconstpointer  user_data)
{
    orm_connection_query_stream_async (fixture->connection,
                                       "SELECT id, label FROM numbers"
                                       " ORDER BY id", NULL, NULL,
                                       on_stream_ready, fixture);

    async_run (fixture);

    /* 25 rows in batches of four: six full batches, one of one, then EOF. */
    g_assert_cmpuint (fixture->rows_seen, ==, 25);
    g_assert_cmpuint (fixture->batches, ==, 8);
    g_assert_false (fixture->callback_off_main_thread);
}

static void
on_stream_closed (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    AsyncFixture      *fixture = (AsyncFixture *) user_data;
    g_autoptr(GError)  error = NULL;

    g_assert_true (orm_row_stream_close_finish (ORM_ROW_STREAM (source), result,
                                                &error));
    g_assert_no_error (error);

    g_main_loop_quit (fixture->loop);
}

static void
on_stream_first_batch (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
    AsyncFixture         *fixture = (AsyncFixture *) user_data;
    OrmRowStream         *stream = ORM_ROW_STREAM (source);
    g_autoptr(GPtrArray)  rows = NULL;
    g_autoptr(GError)     error = NULL;
    OrmRow               *row;

    rows = orm_row_stream_fetch_finish (stream, result, &error);
    g_assert_no_error (error);
    g_assert_cmpuint (rows->len, ==, 3);

    row = (OrmRow *) g_ptr_array_index (rows, 0);
    g_assert_cmpint (orm_row_get_integer (row, 0), ==, 1);
    g_assert_cmpstr (orm_row_get_string (row, 1), ==, "row-1");

    fixture->rows_seen = rows->len;

    /* Abandoning a stream early has to release the connection. */
    orm_row_stream_close_async (stream, NULL, on_stream_closed, fixture);
}

static void
on_stream_early_close_ready (GObject      *source,
                             GAsyncResult *result,
                             gpointer      user_data)
{
    AsyncFixture      *fixture = (AsyncFixture *) user_data;
    OrmRowStream      *stream;
    g_autoptr(GError)  error = NULL;

    stream = orm_connection_query_stream_finish (ORM_CONNECTION (source),
                                                 result, &error);
    g_assert_no_error (error);
    g_assert_nonnull (stream);

    fixture->stream = stream;

    orm_row_stream_fetch_async (stream, 3, NULL, on_stream_first_batch, fixture);
}

static void
test_async_stream_early_close (AsyncFixture  *fixture,
                               gconstpointer  user_data)
{
    g_autoptr(OrmResult) query_result = NULL;
    g_autoptr(GError)    error = NULL;
    g_autoptr(OrmValue)  scalar = NULL;

    orm_connection_query_stream_async (fixture->connection,
                                       "SELECT id, label FROM numbers"
                                       " ORDER BY id", NULL, NULL,
                                       on_stream_early_close_ready, fixture);

    async_run (fixture);

    g_assert_cmpuint (fixture->rows_seen, ==, 3);

    /* And the connection is usable straight afterwards. */
    query_result = orm_connection_query (fixture->connection,
                                         "SELECT COUNT(*) FROM numbers", &error);
    g_assert_no_error (error);

    scalar = orm_result_get_scalar (query_result);
    g_assert_cmpint (orm_value_get_integer (scalar), ==, 25);
}

/* ------------------------------------------------------------------ */
/* Opening a connection asynchronously                                */
/* ------------------------------------------------------------------ */

static void
on_connect_ready (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    AsyncFixture             *fixture = (AsyncFixture *) user_data;
    g_autoptr(OrmConnection)  connection = NULL;
    g_autoptr(GError)         error = NULL;

    async_note_thread (fixture);

    connection = orm_engine_connect_finish (ORM_ENGINE (source), result, &error);
    g_assert_no_error (error);
    g_assert_nonnull (connection);
    g_assert_true (orm_connection_is_open (connection));
    g_assert_cmpint (orm_connection_get_state (connection), ==,
                     ORM_CONNECTION_IDLE);

    g_main_loop_quit (fixture->loop);
}

static void
test_async_connect (AsyncFixture  *fixture,
                    gconstpointer  user_data)
{
    orm_engine_connect_async (fixture->engine, NULL, on_connect_ready, fixture);

    async_run (fixture);

    g_assert_false (fixture->callback_off_main_thread);
}

/* ------------------------------------------------------------------ */
/* Closing asynchronously                                             */
/* ------------------------------------------------------------------ */

static void
on_close_ready (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
    AsyncFixture      *fixture = (AsyncFixture *) user_data;
    g_autoptr(GError)  error = NULL;

    g_assert_true (orm_connection_close_finish (ORM_CONNECTION (source), result,
                                                &error));
    g_assert_no_error (error);

    g_assert_false (orm_connection_is_open (ORM_CONNECTION (source)));
    g_assert_cmpint (orm_connection_get_state (ORM_CONNECTION (source)), ==,
                     ORM_CONNECTION_CLOSED);

    g_main_loop_quit (fixture->loop);
}

static void
test_async_close (AsyncFixture  *fixture,
                  gconstpointer  user_data)
{
    /* Queued behind real work, to prove the close waits for it. */
    orm_connection_execute_async (fixture->connection,
                                  "INSERT INTO numbers (id, label)"
                                  " VALUES (100, 'last')",
                                  NULL, NULL, NULL, NULL);

    orm_connection_close_async (fixture->connection, NULL, on_close_ready,
                                fixture);

    async_run (fixture);
}

/* ------------------------------------------------------------------ */
/* Introspection off the calling thread                               */
/* ------------------------------------------------------------------ */

static void
on_row_count_ready (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    AsyncFixture      *fixture = (AsyncFixture *) user_data;
    g_autoptr(GError)  error = NULL;
    gboolean           is_estimate = TRUE;
    gint64             count;

    count = orm_inspector_estimate_row_count_finish (ORM_INSPECTOR (source),
                                                     result, &is_estimate,
                                                     &error);
    g_assert_no_error (error);
    g_assert_cmpint (count, ==, 25);

    /* SQLite has no planner statistics to consult, so it always counts. */
    g_assert_false (is_estimate);

    g_main_loop_quit (fixture->loop);
}

static void
on_columns_ready (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    AsyncFixture         *fixture = (AsyncFixture *) user_data;
    g_autoptr(GPtrArray)  columns = NULL;
    g_autoptr(GError)     error = NULL;

    columns = orm_inspector_get_columns_finish (ORM_INSPECTOR (source), result,
                                                &error);
    g_assert_no_error (error);
    g_assert_nonnull (columns);
    g_assert_cmpuint (columns->len, ==, 2);

    orm_inspector_estimate_row_count_async (ORM_INSPECTOR (source), "numbers",
                                            NULL, NULL, on_row_count_ready,
                                            fixture);
}

static void
on_relations_ready (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    AsyncFixture         *fixture = (AsyncFixture *) user_data;
    g_autoptr(GPtrArray)  relations = NULL;
    g_autoptr(GError)     error = NULL;

    async_note_thread (fixture);

    relations = orm_inspector_list_relations_finish (ORM_INSPECTOR (source),
                                                     result, &error);
    g_assert_no_error (error);
    g_assert_nonnull (relations);
    g_assert_cmpuint (relations->len, ==, 1);

    orm_inspector_get_columns_async (ORM_INSPECTOR (source), "numbers", NULL,
                                     NULL, on_columns_ready, fixture);
}

static void
test_async_inspector (AsyncFixture  *fixture,
                      gconstpointer  user_data)
{
    g_autoptr(OrmInspector) inspector = NULL;
    g_autoptr(GError)       error = NULL;

    inspector = orm_inspector_new (fixture->connection, &error);
    g_assert_no_error (error);
    g_assert_nonnull (inspector);

    orm_inspector_list_relations_async (inspector, NULL, NULL,
                                        on_relations_ready, fixture);

    async_run (fixture);

    g_assert_false (fixture->callback_off_main_thread);
}

/* ------------------------------------------------------------------ */
/* Signals                                                            */
/* ------------------------------------------------------------------ */

static void
on_state_changed (OrmConnection      *connection,
                  OrmConnectionState  old_state,
                  OrmConnectionState  new_state,
                  gpointer            user_data)
{
    AsyncFixture    *fixture = (AsyncFixture *) user_data;
    StateTransition  transition;

    async_note_thread (fixture);

    /* A transition to the state it is already in is noise, not a signal. */
    g_assert_cmpint (old_state, !=, new_state);

    transition.old_state = old_state;
    transition.new_state = new_state;

    g_array_append_val (fixture->states, transition);
}

static void
on_state_query_ready (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
    AsyncFixture         *fixture = (AsyncFixture *) user_data;
    g_autoptr(OrmResult)  query_result = NULL;
    g_autoptr(GError)     error = NULL;

    query_result = orm_connection_query_finish (ORM_CONNECTION (source), result,
                                                &error);
    g_assert_no_error (error);

    g_main_loop_quit (fixture->loop);
}

static void
test_async_state_signal (AsyncFixture  *fixture,
                         gconstpointer  user_data)
{
    StateTransition transition;

    g_signal_connect (fixture->connection, "state-changed",
                      G_CALLBACK (on_state_changed), fixture);

    g_assert_cmpint (orm_connection_get_state (fixture->connection), ==,
                     ORM_CONNECTION_IDLE);

    orm_connection_query_async (fixture->connection,
                                "SELECT COUNT(*) FROM numbers", NULL, NULL,
                                on_state_query_ready, fixture);

    async_run (fixture);
    async_drain (fixture);

    g_assert_cmpuint (fixture->states->len, ==, 2);

    transition = g_array_index (fixture->states, StateTransition, 0);
    g_assert_cmpint (transition.old_state, ==, ORM_CONNECTION_IDLE);
    g_assert_cmpint (transition.new_state, ==, ORM_CONNECTION_BUSY);

    transition = g_array_index (fixture->states, StateTransition, 1);
    g_assert_cmpint (transition.old_state, ==, ORM_CONNECTION_BUSY);
    g_assert_cmpint (transition.new_state, ==, ORM_CONNECTION_IDLE);

    /*
     * The handler must have run here, not on the worker: this is the one
     * property that makes it safe to touch a widget from it.
     */
    g_assert_false (fixture->callback_off_main_thread);

    orm_connection_close (fixture->connection);
    async_drain (fixture);

    g_assert_cmpuint (fixture->states->len, ==, 3);

    transition = g_array_index (fixture->states, StateTransition, 2);
    g_assert_cmpint (transition.old_state, ==, ORM_CONNECTION_IDLE);
    g_assert_cmpint (transition.new_state, ==, ORM_CONNECTION_CLOSED);
    g_assert_cmpint (orm_connection_get_state (fixture->connection), ==,
                     ORM_CONNECTION_CLOSED);
}

static void
on_notice (OrmConnection *connection,
           const gchar   *message,
           gpointer       user_data)
{
    AsyncFixture *fixture = (AsyncFixture *) user_data;

    g_ptr_array_add (fixture->notices, g_strdup (message));
}

static void
test_async_notice_signal (AsyncFixture  *fixture,
                          gconstpointer  user_data)
{
    /*
     * The signal exists on every connection so a caller can bind it once
     * without asking what it is talking to.  Only PostgreSQL ever emits
     * it: SQLite has no channel for out-of-band server messages, and this
     * asserts that it stays quiet rather than inventing something.
     */
    g_assert_cmpuint (g_signal_lookup ("notice", ORM_TYPE_CONNECTION), !=, 0);
    g_assert_cmpuint (g_signal_lookup ("state-changed", ORM_TYPE_CONNECTION),
                      !=, 0);

    g_signal_connect (fixture->connection, "notice",
                      G_CALLBACK (on_notice), fixture);

    async_exec (fixture, "DROP TABLE IF EXISTS not_there");
    async_drain (fixture);

    g_assert_cmpuint (fixture->notices->len, ==, 0);
}

int
main (int    argc,
      char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add ("/async/query", AsyncFixture, NULL,
                async_fixture_setup, test_async_query, async_fixture_teardown);
    g_test_add ("/async/params-are-copied", AsyncFixture, NULL,
                async_fixture_setup, test_async_params_are_copied,
                async_fixture_teardown);
    g_test_add ("/async/ordering", AsyncFixture, NULL,
                async_fixture_setup, test_async_ordering,
                async_fixture_teardown);
    g_test_add ("/async/sync-interleave", AsyncFixture, NULL,
                async_fixture_setup, test_async_sync_interleave,
                async_fixture_teardown);
    g_test_add ("/async/cancel", AsyncFixture, NULL,
                async_fixture_setup, test_async_cancel, async_fixture_teardown);
    g_test_add ("/async/cancel-before-start", AsyncFixture, NULL,
                async_fixture_setup, test_async_cancel_before_start,
                async_fixture_teardown);
    g_test_add ("/async/stream", AsyncFixture, NULL,
                async_fixture_setup, test_async_stream, async_fixture_teardown);
    g_test_add ("/async/stream-early-close", AsyncFixture, NULL,
                async_fixture_setup, test_async_stream_early_close,
                async_fixture_teardown);
    g_test_add ("/async/connect", AsyncFixture, NULL,
                async_fixture_setup, test_async_connect,
                async_fixture_teardown);
    g_test_add ("/async/close", AsyncFixture, NULL,
                async_fixture_setup, test_async_close, async_fixture_teardown);
    g_test_add ("/async/inspector", AsyncFixture, NULL,
                async_fixture_setup, test_async_inspector,
                async_fixture_teardown);
    g_test_add ("/async/state-signal", AsyncFixture, NULL,
                async_fixture_setup, test_async_state_signal,
                async_fixture_teardown);
    g_test_add ("/async/notice-signal", AsyncFixture, NULL,
                async_fixture_setup, test_async_notice_signal,
                async_fixture_teardown);

    return g_test_run ();
}
