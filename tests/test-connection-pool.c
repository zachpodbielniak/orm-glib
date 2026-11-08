/* test-connection-pool.c
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
 * OrmConnectionPool, over a SQLite file.
 *
 * A file rather than ":memory:" because every in-memory connection is its
 * own database, which is exactly what a pool must refuse.
 *
 * The properties worth pinning down are the ones that fail quietly:
 * that a connection comes back clean (no transaction, even one begun as
 * raw SQL; default isolation; no cursor left open) and a stale
 * OrmTransaction or OrmResult cannot reach into the next borrower's work;
 * that one-shot query results hold no connection, so keeping them cannot
 * starve the pool; that a connection unreferenced instead of released
 * gives its slot back;
 * that waiters are served in arrival order and time out or cancel
 * cleanly; that dead connections are replaced rather than lent; and that
 * the reaper, which lives in the main loop, actually trims idle
 * connections.  Every loop runs behind a timeout so a hang fails the test
 * instead of the suite.
 */

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>

#include "test-fixtures.h"

#define POOL_TIMEOUT_SECONDS    (30)
#define POOL_ROWS               (50)
#define POOL_THREADS            (8)
#define POOL_QUERIES_PER_THREAD (25)

typedef struct
{
    gchar             *dir;
    gchar             *path;
    OrmEngine         *engine;
    OrmConnectionPool *pool;

    /* Async scratch */
    guint              pending;
    GPtrArray         *connections;   /* Delivered, in order */
    GPtrArray         *errors;        /* GError *, in order */
    GPtrArray         *results;       /* OrmResult *, kept alive */
    OrmRowStream      *stream;
    GError            *stream_error;
} PoolFixture;

static void
pool_fixture_setup (PoolFixture   *fixture,
                    gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;

    fixture->dir = g_dir_make_tmp ("orm-pool-XXXXXX", &error);
    g_assert_no_error (error);
    fixture->path = g_build_filename (fixture->dir, "pool.db", NULL);

    fixture->engine = orm_engine_new_sqlite (fixture->path, &error);
    g_assert_no_error (error);

    g_assert_true (orm_engine_execute (fixture->engine,
                                       "CREATE TABLE items (id INTEGER PRIMARY KEY, label TEXT)",
                                       &error));
    g_assert_no_error (error);

    /* WAL lets the readers in the threaded tests run alongside writers */
    g_assert_true (orm_engine_execute (fixture->engine,
                                       "PRAGMA journal_mode = WAL", &error));
    g_assert_no_error (error);

    fixture->pool = orm_connection_pool_new (fixture->engine, 4, &error);
    g_assert_no_error (error);
    g_assert_nonnull (fixture->pool);

    fixture->pending = 0;
    fixture->connections = g_ptr_array_new ();
    fixture->errors = g_ptr_array_new_with_free_func ((GDestroyNotify) g_error_free);
    fixture->results = g_ptr_array_new_with_free_func (g_object_unref);
    fixture->stream = NULL;
    fixture->stream_error = NULL;
}

static void
pool_fixture_teardown (PoolFixture   *fixture,
                       gconstpointer  user_data)
{
    g_autofree gchar *wal = NULL;
    g_autofree gchar *shm = NULL;

    g_clear_pointer (&fixture->results, g_ptr_array_unref);
    g_clear_object (&fixture->stream);
    g_clear_error (&fixture->stream_error);
    g_clear_pointer (&fixture->errors, g_ptr_array_unref);
    g_clear_pointer (&fixture->connections, g_ptr_array_unref);
    g_clear_object (&fixture->pool);
    g_clear_object (&fixture->engine);

    wal = g_strconcat (fixture->path, "-wal", NULL);
    shm = g_strconcat (fixture->path, "-shm", NULL);
    g_unlink (fixture->path);
    g_unlink (wal);
    g_unlink (shm);
    g_rmdir (fixture->dir);

    g_clear_pointer (&fixture->path, g_free);
    g_clear_pointer (&fixture->dir, g_free);
}

static gboolean
pool_on_timeout (gpointer data)
{
    gboolean *timed_out = (gboolean *) data;

    *timed_out = TRUE;
    return G_SOURCE_REMOVE;
}

/*
 * Iterates the default context until @fixture->pending reaches zero.
 */
static void
pool_run_until_done (PoolFixture *fixture)
{
    gboolean timed_out = FALSE;
    guint    timeout_id;

    timeout_id = g_timeout_add_seconds (POOL_TIMEOUT_SECONDS, pool_on_timeout,
                                        &timed_out);

    while (fixture->pending > 0 && !timed_out)
        g_main_context_iteration (NULL, TRUE);

    g_assert_false (timed_out);
    g_source_remove (timeout_id);
}

/*
 * Turns the loop over for @ms milliseconds, for anything that is meant
 * to happen "soon" rather than in response to one callback.
 */
static void
pool_run_for (guint ms)
{
    gboolean done = FALSE;

    g_timeout_add (ms, pool_on_timeout, &done);

    while (!done)
        g_main_context_iteration (NULL, TRUE);
}

static OrmConnection *
pool_acquire (PoolFixture *fixture)
{
    g_autoptr(GError) error = NULL;
    OrmConnection    *connection;

    connection = orm_connection_pool_acquire (fixture->pool, NULL, &error);
    g_assert_no_error (error);
    g_assert_nonnull (connection);

    return connection;
}

static gint64
pool_count_items (PoolFixture *fixture)
{
    g_autoptr(OrmResult) result = NULL;
    g_autoptr(OrmValue)  scalar = NULL;
    g_autoptr(GError)    error = NULL;

    result = orm_connection_pool_query (fixture->pool,
                                        "SELECT COUNT(*) FROM items", NULL,
                                        &error);
    g_assert_no_error (error);

    scalar = orm_result_get_scalar (result);
    return orm_value_get_integer (scalar);
}

/* Records a delivered connection, or the error */
static void
on_acquired (GObject      *source,
             GAsyncResult *result,
             gpointer      user_data)
{
    PoolFixture   *fixture = (PoolFixture *) user_data;
    OrmConnection *connection;
    GError        *error = NULL;

    connection = orm_connection_pool_acquire_finish (ORM_CONNECTION_POOL (source),
                                                     result, &error);
    if (connection != NULL)
        g_ptr_array_add (fixture->connections, connection);
    else
        g_ptr_array_add (fixture->errors, error);

    fixture->pending--;
}

/* ------------------------------------------------------------------ */
/* Construction                                                       */
/* ------------------------------------------------------------------ */

static void
test_pool_rejects_shared_memory (void)
{
    g_autoptr(OrmEngine)         engine = NULL;
    g_autoptr(OrmConnectionPool) pool = NULL;
    g_autoptr(GError)            error = NULL;

    engine = orm_engine_new ("sqlite:///:memory:", &error);
    g_assert_no_error (error);

    pool = orm_connection_pool_new (engine, 2, &error);
    g_assert_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED);
    g_assert_null (pool);
    g_clear_error (&error);

    /* One connection is one database, so a pool of one is fine */
    pool = orm_connection_pool_new (engine, 1, &error);
    g_assert_no_error (error);
    g_assert_nonnull (pool);
    g_assert_cmpuint (orm_connection_pool_get_size (pool), ==, 0);
}

/* ------------------------------------------------------------------ */
/* Leasing                                                            */
/* ------------------------------------------------------------------ */

static void
test_pool_reuses_connections (PoolFixture   *fixture,
                              gconstpointer  user_data)
{
    OrmConnection *first;
    OrmConnection *second;

    first = pool_acquire (fixture);
    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 1);
    g_assert_cmpuint (orm_connection_pool_get_idle_count (fixture->pool), ==, 0);

    orm_connection_pool_release (fixture->pool, first);
    g_assert_cmpuint (orm_connection_pool_get_idle_count (fixture->pool), ==, 1);

    second = pool_acquire (fixture);
    g_assert_true (second == first);
    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 1);

    orm_connection_pool_release (fixture->pool, second);
}

static void
test_pool_bounded (PoolFixture   *fixture,
                   gconstpointer  user_data)
{
    OrmConnection     *held[4];
    g_autoptr(GError)  error = NULL;
    OrmConnection     *extra;
    guint              i;
    guint              j;

    for (i = 0; i < G_N_ELEMENTS (held); i++)
        held[i] = pool_acquire (fixture);

    /* Four distinct connections: the pool is the unit of parallelism */
    for (i = 0; i < G_N_ELEMENTS (held); i++)
        for (j = i + 1; j < G_N_ELEMENTS (held); j++)
            g_assert_true (held[i] != held[j]);

    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 4);

    orm_connection_pool_set_acquire_timeout (fixture->pool, 50);
    extra = orm_connection_pool_acquire (fixture->pool, NULL, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT);
    g_assert_null (extra);
    g_assert_cmpuint (orm_connection_pool_get_waiting_count (fixture->pool), ==, 0);

    for (i = 0; i < G_N_ELEMENTS (held); i++)
        orm_connection_pool_release (fixture->pool, held[i]);

    g_assert_cmpuint (orm_connection_pool_get_idle_count (fixture->pool), ==, 4);
}

/* ------------------------------------------------------------------ */
/* Waiting                                                            */
/* ------------------------------------------------------------------ */

/*
 * Queued async acquirers get connections in the order they asked, and
 * get the very connection that was handed back.
 */
static void
test_pool_waiters_fifo (PoolFixture   *fixture,
                        gconstpointer  user_data)
{
    g_autoptr(GError)  error = NULL;
    g_autoptr(OrmConnectionPool) pool = NULL;
    OrmConnection     *held;
    gint               order[3] = { 0, 1, 2 };
    guint              i;

    pool = orm_connection_pool_new (fixture->engine, 1, &error);
    g_assert_no_error (error);
    g_set_object (&fixture->pool, pool);

    held = pool_acquire (fixture);

    for (i = 0; i < G_N_ELEMENTS (order); i++)
    {
        fixture->pending++;
        orm_connection_pool_acquire_async (fixture->pool, NULL, on_acquired, fixture);
    }

    pool_run_for (20);
    g_assert_cmpuint (orm_connection_pool_get_waiting_count (fixture->pool), ==, 3);
    g_assert_cmpuint (fixture->connections->len, ==, 0);

    /* Each release serves exactly the next waiter, with the same connection */
    orm_connection_pool_release (fixture->pool, held);
    for (i = 0; i < G_N_ELEMENTS (order); i++)
    {
        while (fixture->connections->len < i + 1)
            g_main_context_iteration (NULL, TRUE);

        g_assert_true (g_ptr_array_index (fixture->connections, i) == held);
        g_assert_cmpuint (orm_connection_pool_get_waiting_count (fixture->pool),
                          ==, G_N_ELEMENTS (order) - i - 1);

        orm_connection_pool_release (fixture->pool,
                                     g_ptr_array_index (fixture->connections, i));
    }

    g_assert_cmpuint (fixture->errors->len, ==, 0);
    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 1);
}

typedef struct
{
    OrmConnectionPool *pool;
    OrmConnection     *connection;
    GError            *error;
} SyncAcquire;

static gpointer
sync_acquire_thread (gpointer data)
{
    SyncAcquire *acquire = (SyncAcquire *) data;

    acquire->connection = orm_connection_pool_acquire (acquire->pool, NULL,
                                                       &acquire->error);
    return NULL;
}

/* A blocked synchronous acquirer is woken by a release on another thread */
static void
test_pool_sync_waiter (PoolFixture   *fixture,
                       gconstpointer  user_data)
{
    g_autoptr(GError)            error = NULL;
    g_autoptr(OrmConnectionPool) pool = NULL;
    OrmConnection               *held;
    SyncAcquire                  acquire = { NULL, NULL, NULL };
    GThread                     *thread;
    gint64                       deadline;

    pool = orm_connection_pool_new (fixture->engine, 1, &error);
    g_assert_no_error (error);

    held = orm_connection_pool_acquire (pool, NULL, &error);
    g_assert_no_error (error);

    acquire.pool = pool;
    thread = g_thread_new ("pool-waiter", sync_acquire_thread, &acquire);

    deadline = g_get_monotonic_time () + POOL_TIMEOUT_SECONDS * G_USEC_PER_SEC;
    while (orm_connection_pool_get_waiting_count (pool) == 0)
    {
        g_assert_cmpint (g_get_monotonic_time (), <, deadline);
        g_usleep (1000);
    }

    orm_connection_pool_release (pool, held);
    g_thread_join (thread);

    g_assert_no_error (acquire.error);
    g_assert_true (acquire.connection == held);
    orm_connection_pool_release (pool, acquire.connection);
}

static void
test_pool_cancel_waiter (PoolFixture   *fixture,
                         gconstpointer  user_data)
{
    g_autoptr(GError)            error = NULL;
    g_autoptr(OrmConnectionPool) pool = NULL;
    g_autoptr(GCancellable)      cancellable = g_cancellable_new ();
    OrmConnection               *held;

    pool = orm_connection_pool_new (fixture->engine, 1, &error);
    g_assert_no_error (error);
    g_set_object (&fixture->pool, pool);

    held = pool_acquire (fixture);

    fixture->pending = 1;
    orm_connection_pool_acquire_async (fixture->pool, cancellable, on_acquired, fixture);
    pool_run_for (10);
    g_assert_cmpuint (orm_connection_pool_get_waiting_count (fixture->pool), ==, 1);

    g_cancellable_cancel (cancellable);
    pool_run_until_done (fixture);

    g_assert_cmpuint (fixture->errors->len, ==, 1);
    g_assert_error ((GError *) g_ptr_array_index (fixture->errors, 0),
                    G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_assert_cmpuint (orm_connection_pool_get_waiting_count (fixture->pool), ==, 0);

    /* Nobody is waiting any more, so the connection goes idle */
    orm_connection_pool_release (fixture->pool, held);
    g_assert_cmpuint (orm_connection_pool_get_idle_count (fixture->pool), ==, 1);
}

static void
test_pool_async_timeout (PoolFixture   *fixture,
                         gconstpointer  user_data)
{
    g_autoptr(GError)            error = NULL;
    g_autoptr(OrmConnectionPool) pool = NULL;
    OrmConnection               *held;

    pool = orm_connection_pool_new (fixture->engine, 1, &error);
    g_assert_no_error (error);
    g_set_object (&fixture->pool, pool);
    orm_connection_pool_set_acquire_timeout (fixture->pool, 30);

    held = pool_acquire (fixture);

    fixture->pending = 1;
    orm_connection_pool_acquire_async (fixture->pool, NULL, on_acquired, fixture);
    pool_run_until_done (fixture);

    g_assert_cmpuint (fixture->errors->len, ==, 1);
    g_assert_error ((GError *) g_ptr_array_index (fixture->errors, 0),
                    G_IO_ERROR, G_IO_ERROR_TIMED_OUT);

    orm_connection_pool_release (fixture->pool, held);
}

/*
 * A connection delivered to a caller that never collects it -- here, no
 * callback at all -- still finds its way back.
 */
static void
test_pool_uncollected_acquire (PoolFixture   *fixture,
                               gconstpointer  user_data)
{
    orm_connection_pool_acquire_async (fixture->pool, NULL, NULL, NULL);
    pool_run_for (100);

    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 1);
    g_assert_cmpuint (orm_connection_pool_get_idle_count (fixture->pool), ==, 1);
}

/* ------------------------------------------------------------------ */
/* Reset on release                                                   */
/* ------------------------------------------------------------------ */

static void
test_pool_rolls_back_on_release (PoolFixture   *fixture,
                                 gconstpointer  user_data)
{
    g_autoptr(GError)         error = NULL;
    g_autoptr(OrmTransaction) stale = NULL;
    g_autoptr(OrmTransaction) fresh = NULL;
    OrmConnection            *connection;

    /* --- A transaction abandoned at release is rolled back --- */
    connection = pool_acquire (fixture);
    stale = orm_connection_begin_transaction (connection, &error);
    g_assert_no_error (error);
    g_assert_true (orm_connection_execute (connection,
                                           "INSERT INTO items (label) VALUES ('lost')",
                                           &error));
    orm_connection_pool_release (fixture->pool, connection);

    g_assert_false (orm_transaction_is_active (stale));
    g_assert_cmpint (pool_count_items (fixture), ==, 0);

    /* --- The next borrower starts clean --- */
    connection = pool_acquire (fixture);
    g_assert_false (orm_connection_in_transaction (connection));

    fresh = orm_connection_begin_transaction (connection, &error);
    g_assert_no_error (error);
    g_assert_true (orm_connection_execute (connection,
                                           "INSERT INTO items (label) VALUES ('kept')",
                                           &error));

    /* --- The stale transaction cannot touch it, explicitly or on finalize --- */
    g_assert_false (orm_transaction_commit (stale, &error));
    g_assert_error (error, ORM_ERROR, ORM_ERROR_TRANSACTION_FAILED);
    g_clear_error (&error);
    g_clear_object (&stale);

    g_assert_true (orm_transaction_is_active (fresh));
    g_assert_true (orm_transaction_commit (fresh, &error));
    g_assert_no_error (error);

    orm_connection_pool_release (fixture->pool, connection);
    g_assert_cmpint (pool_count_items (fixture), ==, 1);
}

/*
 * A transaction the connection never heard of -- BEGIN sent as raw SQL --
 * is still rolled back, because the reset asks the backend.
 */
static void
test_pool_rolls_back_raw_begin (PoolFixture   *fixture,
                                gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;
    OrmConnection    *connection;

    connection = pool_acquire (fixture);
    g_assert_true (orm_connection_execute (connection, "BEGIN", &error));
    g_assert_no_error (error);
    g_assert_true (orm_connection_execute (connection,
                                           "INSERT INTO items (label) VALUES ('raw')",
                                           &error));
    g_assert_no_error (error);
    g_assert_false (orm_connection_in_transaction (connection));
    orm_connection_pool_release (fixture->pool, connection);

    g_assert_cmpint (pool_count_items (fixture), ==, 0);

    /* Same connection back, and a second BEGIN is legal: nothing is open */
    connection = pool_acquire (fixture);
    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 1);
    g_assert_true (orm_connection_execute (connection, "BEGIN", &error));
    g_assert_no_error (error);
    g_assert_true (orm_connection_execute (connection, "COMMIT", &error));
    g_assert_no_error (error);
    orm_connection_pool_release (fixture->pool, connection);
}

static void
pool_insert_rows (PoolFixture *fixture,
                  guint        n_rows)
{
    g_autoptr(GError) error = NULL;
    guint             i;

    for (i = 0; i < n_rows; i++)
    {
        g_assert_true (orm_connection_pool_execute (fixture->pool,
                                                    "INSERT INTO items (label) VALUES ('row')",
                                                    NULL, &error));
        g_assert_no_error (error);
    }
}

/*
 * A result still open at release is closed then: reading it afterwards
 * fails visibly rather than returning rows from a snapshot the next
 * borrower's writes are invisible to.  Freeing it later must not wait
 * for the connection, which by then belongs to someone else.
 */
static void
test_pool_release_closes_results (PoolFixture   *fixture,
                                  gconstpointer  user_data)
{
    g_autoptr(GError)    error = NULL;
    g_autoptr(OrmResult) open_result = NULL;
    OrmConnection       *connection;
    OrmConnection       *next;
    const GError        *read_error;

    pool_insert_rows (fixture, 3);

    connection = pool_acquire (fixture);
    open_result = orm_connection_query (connection, "SELECT id FROM items", &error);
    g_assert_no_error (error);
    g_assert_true (orm_result_next (open_result));

    orm_connection_pool_release (fixture->pool, connection);

    /* --- Reading on reports why it stopped --- */
    g_assert_false (orm_result_next (open_result));
    read_error = orm_result_get_error (open_result);
    g_assert_nonnull (read_error);
    g_assert_error ((GError *) read_error, ORM_ERROR, ORM_ERROR_CONNECTION_CLOSED);
    g_assert_cmpstr (read_error->message, ==,
                     "Result is no longer readable: its connection was "
                     "returned to a pool");

    /* --- The next borrower writes and reads freely on the same connection --- */
    next = pool_acquire (fixture);
    g_assert_true (next == connection);
    g_assert_true (orm_connection_execute (next,
                                           "INSERT INTO items (label) VALUES ('after')",
                                           &error));
    g_assert_no_error (error);

    /* Freed while lent to someone else: must not deadlock or touch it */
    g_clear_object (&open_result);

    orm_connection_pool_release (fixture->pool, next);
    g_assert_cmpint (pool_count_items (fixture), ==, 4);
}

static void
on_stream_ready (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    PoolFixture *fixture = (PoolFixture *) user_data;

    fixture->stream = orm_connection_query_stream_finish (ORM_CONNECTION (source),
                                                          result,
                                                          &fixture->stream_error);
    fixture->pending--;
}

static void
on_stream_fetched (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    PoolFixture *fixture = (PoolFixture *) user_data;
    GPtrArray   *rows;

    rows = orm_row_stream_fetch_finish (ORM_ROW_STREAM (source), result,
                                        &fixture->stream_error);
    if (rows != NULL)
        g_ptr_array_add (fixture->results, rows);

    fixture->pending--;
}

/* Row streams are cursors too, and are closed the same way */
static void
test_pool_release_closes_streams (PoolFixture   *fixture,
                                  gconstpointer  user_data)
{
    OrmConnection *connection;

    pool_insert_rows (fixture, 3);

    connection = pool_acquire (fixture);
    fixture->pending = 1;
    orm_connection_query_stream_async (connection, "SELECT id FROM items", NULL,
                                       NULL, on_stream_ready, fixture);
    pool_run_until_done (fixture);
    g_assert_no_error (fixture->stream_error);
    g_assert_nonnull (fixture->stream);

    orm_connection_pool_release (fixture->pool, connection);

    /* The results array takes a GPtrArray here; it must stay empty */
    g_ptr_array_set_free_func (fixture->results, (GDestroyNotify) g_ptr_array_unref);

    fixture->pending = 1;
    orm_row_stream_fetch_async (fixture->stream, 10, NULL, on_stream_fetched, fixture);
    pool_run_until_done (fixture);

    g_assert_error (fixture->stream_error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION);
    g_assert_cmpuint (fixture->results->len, ==, 0);
}

static void
test_pool_restores_isolation (PoolFixture   *fixture,
                              gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;
    OrmConnection    *connection;

    connection = pool_acquire (fixture);
    g_assert_true (orm_connection_set_isolation_level (connection,
                                                       ORM_ISOLATION_READ_UNCOMMITTED,
                                                       &error));
    g_assert_no_error (error);
    orm_connection_pool_release (fixture->pool, connection);

    connection = pool_acquire (fixture);
    g_assert_cmpint (orm_connection_get_isolation_level (connection), ==,
                     ORM_ISOLATION_SERIALIZABLE);
    orm_connection_pool_release (fixture->pool, connection);
}

/* ------------------------------------------------------------------ */
/* Health                                                             */
/* ------------------------------------------------------------------ */

static void
test_pool_discards_closed (PoolFixture   *fixture,
                           gconstpointer  user_data)
{
    OrmConnection *connection;

    connection = pool_acquire (fixture);
    orm_connection_close (connection);
    orm_connection_pool_release (fixture->pool, connection);

    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 0);

    connection = pool_acquire (fixture);
    g_assert_true (orm_connection_is_open (connection));
    orm_connection_pool_release (fixture->pool, connection);
}

/*
 * A connection that dies while idle is caught by validation and replaced,
 * so the borrower never sees it.
 */
static void
test_pool_validates_idle (PoolFixture   *fixture,
                          gconstpointer  user_data)
{
    OrmConnection *connection;

    orm_connection_pool_set_validation_interval (fixture->pool, 0);

    connection = pool_acquire (fixture);
    orm_connection_pool_release (fixture->pool, connection);

    /* Still referenced by the pool's idle list; kill it behind its back */
    orm_connection_close (connection);

    connection = pool_acquire (fixture);
    g_assert_true (orm_connection_is_open (connection));
    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 1);
    orm_connection_pool_release (fixture->pool, connection);
}

static void
test_pool_init_sql (PoolFixture   *fixture,
                    gconstpointer  user_data)
{
    g_autoptr(GError)    error = NULL;
    g_autoptr(OrmResult) result = NULL;
    OrmConnection       *connection;

    /* A temp table exists only on the connection that made it */
    orm_connection_pool_set_init_sql (fixture->pool,
                                      "CREATE TEMP TABLE pool_marker (x INTEGER)");

    connection = pool_acquire (fixture);
    result = orm_connection_query (connection, "SELECT COUNT(*) FROM pool_marker",
                                   &error);
    g_assert_no_error (error);
    g_clear_object (&result);
    orm_connection_pool_release (fixture->pool, connection);

    /* Broken init SQL fails the acquire and leaves nothing behind */
    orm_connection_pool_close (fixture->pool);
    g_clear_object (&fixture->pool);
    fixture->pool = orm_connection_pool_new (fixture->engine, 2, &error);
    g_assert_no_error (error);
    orm_connection_pool_set_init_sql (fixture->pool, "THIS IS NOT SQL");

    connection = orm_connection_pool_acquire (fixture->pool, NULL, &error);
    g_assert_null (connection);
    g_assert_nonnull (error);
    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 0);
}

/* ------------------------------------------------------------------ */
/* Main loop: the idle reaper                                         */
/* ------------------------------------------------------------------ */

static void
test_pool_reaps_idle (PoolFixture   *fixture,
                      gconstpointer  user_data)
{
    OrmConnection *held[3];
    gint64         deadline;
    guint          i;

    orm_connection_pool_set_idle_timeout (fixture->pool, 1);
    orm_connection_pool_set_min_size (fixture->pool, 1);

    for (i = 0; i < G_N_ELEMENTS (held); i++)
        held[i] = pool_acquire (fixture);
    for (i = 0; i < G_N_ELEMENTS (held); i++)
        orm_connection_pool_release (fixture->pool, held[i]);

    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 3);

    /* The reaper is a source in this thread's context: iterate it */
    deadline = g_get_monotonic_time () + 10 * G_USEC_PER_SEC;
    while (orm_connection_pool_get_size (fixture->pool) > 1 &&
           g_get_monotonic_time () < deadline)
        pool_run_for (100);

    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 1);
    g_assert_cmpuint (orm_connection_pool_get_idle_count (fixture->pool), ==, 1);
}

/* ------------------------------------------------------------------ */
/* Close                                                              */
/* ------------------------------------------------------------------ */

static void
test_pool_close (PoolFixture   *fixture,
                 gconstpointer  user_data)
{
    g_autoptr(GError)            error = NULL;
    g_autoptr(OrmConnectionPool) pool = NULL;
    OrmConnection               *held;
    OrmConnection               *late;

    pool = orm_connection_pool_new (fixture->engine, 1, &error);
    g_assert_no_error (error);
    g_set_object (&fixture->pool, pool);

    held = pool_acquire (fixture);

    fixture->pending = 1;
    orm_connection_pool_acquire_async (fixture->pool, NULL, on_acquired, fixture);
    pool_run_for (10);

    orm_connection_pool_close (fixture->pool);
    pool_run_until_done (fixture);

    g_assert_cmpuint (fixture->errors->len, ==, 1);
    g_assert_error ((GError *) g_ptr_array_index (fixture->errors, 0),
                    ORM_ERROR, ORM_ERROR_CONNECTION_CLOSED);

    /* A lent connection is closed on its way back, not reused */
    orm_connection_pool_release (fixture->pool, held);
    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 0);

    late = orm_connection_pool_acquire (fixture->pool, NULL, &error);
    g_assert_error (error, ORM_ERROR, ORM_ERROR_CONNECTION_CLOSED);
    g_assert_null (late);
}

/*
 * g_object_unref() on a lent connection instead of release.  The pool
 * cannot reuse that connection, but it must not lose the slot: before the
 * fix, the pool went on counting a connection that no longer existed,
 * and enough such mistakes left every acquire waiting forever.
 */
static void
test_pool_unref_instead_of_release (PoolFixture   *fixture,
                                    gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;
    OrmConnection    *held[4];
    OrmConnection    *again;
    guint             i;

    orm_connection_pool_set_acquire_timeout (fixture->pool, 2000);

    for (i = 0; i < 4; i++)
        held[i] = pool_acquire (fixture);

    /* --- A queued acquirer is served by the recovered slot --- */
    fixture->pending = 1;
    orm_connection_pool_acquire_async (fixture->pool, NULL, on_acquired, fixture);
    g_assert_cmpuint (orm_connection_pool_get_waiting_count (fixture->pool), ==, 1);

    g_test_expect_message (NULL, G_LOG_LEVEL_WARNING,
                           "*finalized without orm_connection_pool_release*");
    g_object_unref (held[0]);
    g_test_assert_expected_messages ();

    pool_run_until_done (fixture);
    g_assert_cmpuint (fixture->errors->len, ==, 0);
    g_assert_cmpuint (fixture->connections->len, ==, 1);
    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 4);

    /* --- With nobody waiting, the slot is simply free again --- */
    g_test_expect_message (NULL, G_LOG_LEVEL_WARNING,
                           "*finalized without orm_connection_pool_release*");
    g_object_unref (held[1]);
    g_test_assert_expected_messages ();
    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 3);

    again = orm_connection_pool_acquire (fixture->pool, NULL, &error);
    g_assert_no_error (error);
    g_assert_nonnull (again);

    /* --- Released ones are still released normally --- */
    orm_connection_pool_release (fixture->pool, again);
    orm_connection_pool_release (fixture->pool, held[2]);
    orm_connection_pool_release (fixture->pool, held[3]);
    orm_connection_pool_release (fixture->pool,
                                 (OrmConnection *) g_ptr_array_index (fixture->connections, 0));
    g_assert_cmpuint (orm_connection_pool_get_idle_count (fixture->pool), ==, 4);
}

/*
 * A second release of the same connection is reported and otherwise
 * ignored.  It must not drop a reference: the caller no longer owns one,
 * so that would be the pool's, freeing the idle connection under it.
 */
static void
test_pool_double_release (PoolFixture   *fixture,
                          gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;
    OrmConnection    *connection;
    OrmConnection    *again;

    connection = pool_acquire (fixture);
    orm_connection_pool_release (fixture->pool, connection);

    g_test_expect_message (NULL, G_LOG_LEVEL_CRITICAL, "*was already released*");
    orm_connection_pool_release (fixture->pool, connection);
    g_test_assert_expected_messages ();

    /* The idle connection is intact and still works */
    again = pool_acquire (fixture);
    g_assert_true (again == connection);
    g_assert_true (orm_connection_execute (again, "SELECT 1", &error));
    g_assert_no_error (error);
    orm_connection_pool_release (fixture->pool, again);

    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 1);
    g_assert_cmpuint (orm_connection_pool_get_idle_count (fixture->pool), ==, 1);
}

/* ------------------------------------------------------------------ */
/* One-shot helpers and parallelism                                   */
/* ------------------------------------------------------------------ */

/*
 * A one-shot query's result is read in full and its connection released
 * before the call returns, so results can be kept -- more of them than
 * the pool has connections -- without holding anything.
 */
static void
test_pool_query_buffered (PoolFixture   *fixture,
                          gconstpointer  user_data)
{
    g_autoptr(GError) error = NULL;
    OrmResult        *result;
    guint             i;
    guint             n_rows;

    pool_insert_rows (fixture, POOL_ROWS);

    for (i = 0; i < 8; i++)
    {
        result = orm_connection_pool_query (fixture->pool,
                                            "SELECT id, label FROM items ORDER BY id",
                                            NULL, &error);
        g_assert_no_error (error);
        g_assert_nonnull (result);
        g_ptr_array_add (fixture->results, result);

        /* Nothing lent: every query reuses the one idle connection */
        g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 1);
        g_assert_cmpuint (orm_connection_pool_get_idle_count (fixture->pool), ==, 1);
    }

    /* --- Every kept result reads in full, columns included --- */
    for (i = 0; i < fixture->results->len; i++)
    {
        result = (OrmResult *) g_ptr_array_index (fixture->results, i);

        g_assert_cmpint (orm_result_get_column_count (result), ==, 2);
        g_assert_cmpstr (orm_result_get_column_name (result, 1), ==, "label");
        g_assert_cmpint (orm_result_get_column_type (result, 0), ==, ORM_VALUE_INTEGER);
        g_assert_null (orm_result_get_column_name (result, 2));

        n_rows = 0;
        while (orm_result_next (result))
        {
            OrmRow *row = orm_result_get_row (result);

            n_rows++;
            g_assert_cmpint (orm_value_get_integer (orm_row_get_value (row, 0)), ==, n_rows);
            g_assert_cmpstr (orm_value_get_string (orm_row_get_value_by_name (row, "label")),
                             ==, "row");
        }
        g_assert_cmpuint (n_rows, ==, POOL_ROWS);
        g_assert_null (orm_result_get_error (result));
        g_assert_false (orm_result_next (result));
    }

    /* --- A failed query returns its connection too --- */
    result = orm_connection_pool_query (fixture->pool, "SELECT nope FROM nowhere",
                                        NULL, &error);
    g_assert_nonnull (error);
    g_assert_null (result);
    g_clear_error (&error);
    g_assert_cmpuint (orm_connection_pool_get_idle_count (fixture->pool),
                      ==, orm_connection_pool_get_size (fixture->pool));
}

/* Keeps the result: the test is that keeping it costs no connection */
static void
on_queried (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
    PoolFixture *fixture = (PoolFixture *) user_data;
    OrmResult   *rows;
    GError      *error = NULL;

    rows = orm_connection_pool_query_finish (ORM_CONNECTION_POOL (source), result,
                                             &error);
    if (rows != NULL)
        g_ptr_array_add (fixture->results, rows);
    else
        g_ptr_array_add (fixture->errors, error);

    fixture->pending--;
}

static void
on_executed (GObject      *source,
             GAsyncResult *result,
             gpointer      user_data)
{
    PoolFixture *fixture = (PoolFixture *) user_data;
    GError      *error = NULL;

    if (!orm_connection_pool_execute_finish (ORM_CONNECTION_POOL (source), result,
                                             &error))
        g_ptr_array_add (fixture->errors, error);

    fixture->pending--;
}

/*
 * Many independent statements fan out across the pool: more requests
 * than connections, all served, and the pool grows to max-size and no
 * further.  Every query result is kept until the end -- twice as many as
 * there are connections -- which used to starve the queue when a result
 * pinned its connection.
 *
 * No init-sql: the SQLite driver's default busy timeout is what lets
 * four connections write to one file without "database is locked".
 */
static void
test_pool_async_fan_out (PoolFixture   *fixture,
                         gconstpointer  user_data)
{
    g_autoptr(OrmValue) label = NULL;
    GList              *params;
    guint               i;

    label = orm_value_new_string ("fan-out");
    params = g_list_append (NULL, label);

    for (i = 0; i < 16; i++)
    {
        fixture->pending++;
        orm_connection_pool_execute_async (fixture->pool,
                                           "INSERT INTO items (label) VALUES (?)",
                                           params, NULL, on_executed, fixture);
    }
    g_list_free (params);

    for (i = 0; i < 8; i++)
    {
        fixture->pending++;
        orm_connection_pool_query_async (fixture->pool, "SELECT id FROM items",
                                         NULL, NULL, on_queried, fixture);
    }

    pool_run_until_done (fixture);

    g_assert_cmpuint (fixture->errors->len, ==, 0);
    g_assert_cmpuint (fixture->results->len, ==, 8);
    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), ==, 4);
    g_assert_cmpuint (orm_connection_pool_get_idle_count (fixture->pool), ==, 4);
    g_assert_cmpint (pool_count_items (fixture), ==, 16);
}

typedef struct
{
    OrmConnection *writer;
    gboolean       committed;
} BusyHolder;

static gpointer
busy_holder_thread (gpointer data)
{
    BusyHolder        *holder = (BusyHolder *) data;
    g_autoptr(GError)  error = NULL;

    g_usleep (300 * G_TIME_SPAN_MILLISECOND);
    holder->committed = orm_connection_execute (holder->writer, "COMMIT", &error);
    g_assert_no_error (error);

    return NULL;
}

/*
 * SQLite connections wait for a lock by default instead of failing at
 * once: one connection holds the write lock for a moment, and a second
 * one's write waits for it and succeeds.
 */
static void
test_pool_sqlite_waits_for_locks (PoolFixture   *fixture,
                                  gconstpointer  user_data)
{
    g_autoptr(GError)    error = NULL;
    g_autoptr(OrmResult) timeout = NULL;
    g_autoptr(OrmValue)  ms = NULL;
    OrmConnection       *waiter;
    BusyHolder           holder;
    GThread             *thread;

    holder.writer = pool_acquire (fixture);
    holder.committed = FALSE;
    waiter = pool_acquire (fixture);

    /* --- The default is visible, and is what init-sql would override --- */
    timeout = orm_connection_query (waiter, "PRAGMA busy_timeout", &error);
    g_assert_no_error (error);
    ms = orm_result_get_scalar (timeout);
    g_assert_cmpint (orm_value_get_integer (ms), ==, 5000);
    g_clear_object (&timeout);

    /* --- Contention waits rather than fails --- */
    g_assert_true (orm_connection_execute (holder.writer, "BEGIN IMMEDIATE", &error));
    g_assert_no_error (error);

    thread = g_thread_new ("busy-holder", busy_holder_thread, &holder);

    g_assert_true (orm_connection_execute (waiter,
                                           "INSERT INTO items (label) VALUES ('waited')",
                                           &error));
    g_assert_no_error (error);

    g_thread_join (thread);
    g_assert_true (holder.committed);

    orm_connection_pool_release (fixture->pool, holder.writer);
    orm_connection_pool_release (fixture->pool, waiter);
    g_assert_cmpint (pool_count_items (fixture), ==, 1);
}

typedef struct
{
    OrmConnectionPool *pool;
    guint              failures;
} Hammer;

static gpointer
hammer_thread (gpointer data)
{
    Hammer *hammer = (Hammer *) data;
    guint   i;

    for (i = 0; i < POOL_QUERIES_PER_THREAD; i++)
    {
        g_autoptr(GError)    error = NULL;
        g_autoptr(OrmResult) result = NULL;

        result = orm_connection_pool_query (hammer->pool,
                                            "SELECT COUNT(*) FROM items", NULL,
                                            &error);
        if (result == NULL || orm_result_next (result) == FALSE)
        {
            g_printerr ("# hammer failure: %s\n",
                        error != NULL ? error->message
                                      : orm_result_get_error (result) != NULL
                                        ? orm_result_get_error (result)->message
                                        : "no row");
            g_atomic_int_inc ((gint *) &hammer->failures);
        }
    }

    return NULL;
}

/* The synchronous API from many threads at once, as a server would use it */
static void
test_pool_threads (PoolFixture   *fixture,
                   gconstpointer  user_data)
{
    GThread *threads[POOL_THREADS];
    Hammer   hammer;
    guint    i;

    hammer.pool = fixture->pool;
    hammer.failures = 0;

    /*
     * No init-sql on purpose.  With SQLite's own default of no busy
     * timeout, several connections opening one WAL file together failed
     * with "database is locked" in about five runs of twelve; the
     * driver's default timeout is what this proves.
     */
    for (i = 0; i < POOL_THREADS; i++)
        threads[i] = g_thread_new ("pool-hammer", hammer_thread, &hammer);
    for (i = 0; i < POOL_THREADS; i++)
        g_thread_join (threads[i]);

    g_assert_cmpuint (hammer.failures, ==, 0);
    g_assert_cmpuint (orm_connection_pool_get_size (fixture->pool), <=, 4);
    g_assert_cmpuint (orm_connection_pool_get_waiting_count (fixture->pool), ==, 0);
    g_assert_cmpuint (orm_connection_pool_get_idle_count (fixture->pool), ==,
                      orm_connection_pool_get_size (fixture->pool));
}

/* ------------------------------------------------------------------ */

#define POOL_TEST(path, func) \
    g_test_add (path, PoolFixture, NULL, pool_fixture_setup, func, \
                pool_fixture_teardown)

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/pool/rejects-shared-memory", test_pool_rejects_shared_memory);
    POOL_TEST ("/pool/reuses-connections", test_pool_reuses_connections);
    POOL_TEST ("/pool/bounded", test_pool_bounded);
    POOL_TEST ("/pool/waiters-fifo", test_pool_waiters_fifo);
    POOL_TEST ("/pool/sync-waiter", test_pool_sync_waiter);
    POOL_TEST ("/pool/cancel-waiter", test_pool_cancel_waiter);
    POOL_TEST ("/pool/async-timeout", test_pool_async_timeout);
    POOL_TEST ("/pool/uncollected-acquire", test_pool_uncollected_acquire);
    POOL_TEST ("/pool/rolls-back-on-release", test_pool_rolls_back_on_release);
    POOL_TEST ("/pool/rolls-back-raw-begin", test_pool_rolls_back_raw_begin);
    POOL_TEST ("/pool/release-closes-results", test_pool_release_closes_results);
    POOL_TEST ("/pool/release-closes-streams", test_pool_release_closes_streams);
    POOL_TEST ("/pool/restores-isolation", test_pool_restores_isolation);
    POOL_TEST ("/pool/discards-closed", test_pool_discards_closed);
    POOL_TEST ("/pool/validates-idle", test_pool_validates_idle);
    POOL_TEST ("/pool/init-sql", test_pool_init_sql);
    POOL_TEST ("/pool/reaps-idle", test_pool_reaps_idle);
    POOL_TEST ("/pool/close", test_pool_close);
    POOL_TEST ("/pool/unref-instead-of-release", test_pool_unref_instead_of_release);
    POOL_TEST ("/pool/double-release", test_pool_double_release);
    POOL_TEST ("/pool/query-buffered", test_pool_query_buffered);
    POOL_TEST ("/pool/async-fan-out", test_pool_async_fan_out);
    POOL_TEST ("/pool/sqlite-waits-for-locks", test_pool_sqlite_waits_for_locks);
    POOL_TEST ("/pool/threads", test_pool_threads);

    return g_test_run ();
}
