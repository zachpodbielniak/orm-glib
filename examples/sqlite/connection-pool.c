/* connection-pool.c
 *
 * Copyright 2026 Zach Podbielniak
 *
 * This file is part of orm-glib.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Example: A connection pool driven from a GMainLoop
 * ==================================================
 *
 * Creates a pool of four connections over a SQLite file, fires eight
 * independent queries at it without waiting for any of them, and lets the
 * main loop collect the answers.  The first four run at once, each on its
 * own connection's worker thread; the other four queue and are served as
 * connections come back.
 */

#include <orm.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>

#define EXAMPLE_POOL_SIZE (4)
#define EXAMPLE_QUERIES   (8)

typedef struct
{
    GMainLoop         *loop;
    OrmConnectionPool *pool;
    guint              pending;
} ExampleState;

/*
 * Runs in the main loop's context once a query has finished.  The rows
 * were read on the connection's worker and the connection is already back
 * in the pool, so the result is plain memory to keep or drop.
 */
static void
on_query_ready (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
    ExampleState         *state = (ExampleState *) user_data;
    g_autoptr(OrmResult)  rows = NULL;
    g_autoptr(OrmValue)   total = NULL;
    g_autoptr(GError)     error = NULL;

    rows = orm_connection_pool_query_finish (ORM_CONNECTION_POOL (source), result,
                                             &error);
    if (rows == NULL)
        g_printerr ("query failed: %s\n", error->message);
    else
    {
        total = orm_result_get_scalar (rows);
        g_print ("  sum = %" G_GINT64_FORMAT "   (pool: %u open, %u idle, %u waiting)\n",
                 orm_value_get_integer (total),
                 orm_connection_pool_get_size (state->pool),
                 orm_connection_pool_get_idle_count (state->pool),
                 orm_connection_pool_get_waiting_count (state->pool));
    }

    if (--state->pending == 0)
        g_main_loop_quit (state->loop);
}

int
main (int    argc,
      char **argv)
{
    g_autoptr(GError)            error = NULL;
    g_autoptr(OrmEngine)         engine = NULL;
    g_autoptr(OrmConnectionPool) pool = NULL;
    g_autoptr(GMainLoop)         loop = NULL;
    g_autofree gchar            *dir = NULL;
    g_autofree gchar            *path = NULL;
    ExampleState                 state;
    guint                        i;

    (void) argc;
    (void) argv;

    g_print ("orm-glib Connection Pool Example\n");
    g_print ("================================\n\n");

    /* --- A file, not ":memory:": each in-memory connection is its own database --- */
    dir = g_dir_make_tmp ("orm-pool-example-XXXXXX", &error);
    if (dir == NULL)
    {
        g_printerr ("Failed to make a temp dir: %s\n", error->message);
        return EXIT_FAILURE;
    }
    path = g_build_filename (dir, "example.db", NULL);

    engine = orm_engine_new_sqlite (path, &error);
    if (engine == NULL ||
        !orm_engine_execute (engine,
                             "CREATE TABLE numbers AS "
                             "WITH RECURSIVE seq(n) AS ("
                             "  SELECT 1 UNION ALL SELECT n + 1 FROM seq WHERE n < 100000)"
                             "SELECT n FROM seq", &error))
    {
        g_printerr ("Failed to set up the database: %s\n", error->message);
        return EXIT_FAILURE;
    }

    /* --- The pool binds to this thread's main context: create it before the loop runs --- */
    pool = orm_connection_pool_new (engine, EXAMPLE_POOL_SIZE, &error);
    if (pool == NULL)
    {
        g_printerr ("Failed to create the pool: %s\n", error->message);
        return EXIT_FAILURE;
    }
    loop = g_main_loop_new (NULL, FALSE);
    state.loop = loop;
    state.pool = pool;
    state.pending = EXAMPLE_QUERIES;

    /* --- Fire everything at once; nothing here blocks --- */
    g_print ("Starting %d queries on a pool of %d:\n", EXAMPLE_QUERIES, EXAMPLE_POOL_SIZE);
    for (i = 0; i < EXAMPLE_QUERIES; i++)
        orm_connection_pool_query_async (pool, "SELECT SUM(n) FROM numbers", NULL,
                                         NULL, on_query_ready, &state);

    g_main_loop_run (loop);

    g_print ("\nDone: %u connection(s) open, all idle.\n",
             orm_connection_pool_get_size (pool));

    /* --- Tidy up: the pool closes its connections before the file goes --- */
    orm_connection_pool_close (pool);
    g_unlink (path);
    g_rmdir (dir);

    return EXIT_SUCCESS;
}
