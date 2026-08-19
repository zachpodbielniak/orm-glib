# Async

Every call in orm-glib blocks by default, and on a local SQLite file that
is usually fine. It stops being fine the moment the database is on the
other end of a network, or the query is a report over a large table: the
thread that asked is the thread that waits, and if that is the thread
drawing your window then your application is frozen for as long as the
server takes.

The asynchronous API moves that wait somewhere else. It is ordinary GIO —
`_async` starts the work, a `GAsyncReadyCallback` runs when it is done,
`_finish` collects the result or the error, and a `GCancellable` gives up
on it.

```c
static void
on_query_ready (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
    g_autoptr(OrmResult) rows = NULL;
    g_autoptr(GError) error = NULL;

    rows = orm_connection_query_finish (ORM_CONNECTION (source), result, &error);
    if (rows == NULL)
    {
        g_warning ("query failed: %s", error->message);
        return;
    }

    while (orm_result_next (rows))
        show_row (orm_result_get_row (rows));
}

orm_connection_query_async (connection,
                            "SELECT id, name FROM users WHERE active = ?",
                            params, cancellable, on_query_ready, NULL);
```

## One thread per connection

None of the three backends will run two statements at once on one
connection handle, so the work cannot go to a thread pool: two pool
threads picking up two queries on the same connection is corruption, not
concurrency.

Instead each `OrmConnection` gets **one** worker thread, created the first
time you make an `_async` call on it and kept until the connection is
closed. Everything asynchronous for that connection runs there, in the
order it was submitted. Three queries started in a row complete in that
order, always — which matters, because "insert the row, then read it
back" is a thing people write.

If you want two queries to run at the same time, open two connections.
That is the honest unit of parallelism and it is what a connection pool
would give you anyway.

### Mixing with the synchronous API

You do not have to choose. The synchronous calls keep working exactly as
before, and once a worker exists they route through it as well: a
synchronous call waits its turn behind whatever is queued instead of
reaching into the backend alongside it.

```c
orm_connection_execute_async (conn, "INSERT INTO ...", NULL, NULL, NULL, NULL);

/* Queues behind the insert.  Blocks until both are done, and sees the
   inserted rows -- it cannot run alongside the insert or before it. */
count = orm_connection_query (conn, "SELECT COUNT(*) FROM ...", &error);
```

The one thing to know is that a synchronous call *does* still block. It
blocks for longer than it used to if there is a queue ahead of it. That
is the price of the two styles being safe together, and it beats the
alternative of them quietly racing.

Code that runs *on* the worker — an `OrmResult` being read inside a job,
say — runs inline rather than queueing behind itself, so there is no way
to deadlock a connection against its own thread.

## Cancellation

Pass a `GCancellable` and cancel it. The task fails with
`G_IO_ERROR_CANCELLED`, and the connection is still usable afterwards:
being interrupted is not the same as being broken.

What happens underneath depends on the backend, and the difference is
worth knowing:

| Backend | Interrupt | What cancelling does |
|---|---|---|
| SQLite | `sqlite3_interrupt` | The statement really stops. The connection is free immediately. |
| PostgreSQL | `PQcancel` | The server is asked to abort the query. The connection is free once it acknowledges. |
| MySQL | none that is safe from another thread | The task fails at once, but the statement runs to completion in the background and its result is thrown away. The connection stays busy until the server finishes. |

So on MySQL, cancelling gets *you* back promptly; it does not get the
*connection* back promptly. Anything you queue behind a cancelled MySQL
query still waits for it.

Cancelling something that has not started yet simply means it never runs.

## Streaming rows

`orm_connection_query_finish()` hands back an `OrmResult`, which is read
synchronously. On PostgreSQL and MySQL every row is already in memory by
then, so that costs nothing. On SQLite the statement has only been
prepared, and the actual work happens as you step it — on whichever
thread is doing the stepping.

`OrmRowStream` is the way to keep that off the calling thread too:

```c
static void
on_batch (GObject *source, GAsyncResult *result, gpointer user_data)
{
    OrmRowStream *stream = ORM_ROW_STREAM (source);
    g_autoptr(GPtrArray) rows = NULL;
    g_autoptr(GError) error = NULL;
    guint i;

    rows = orm_row_stream_fetch_finish (stream, result, &error);
    if (rows == NULL)
        return;               /* NULL is an error, never end-of-results */

    if (rows->len == 0)
    {
        orm_row_stream_close_async (stream, NULL, on_closed, NULL);
        return;               /* an empty batch is end-of-results */
    }

    for (i = 0; i < rows->len; i++)
        append_row (g_ptr_array_index (rows, i));

    orm_row_stream_fetch_async (stream, 100, NULL, on_batch, user_data);
}

orm_connection_query_stream_async (connection, sql, params, cancellable,
                                   on_stream_ready, NULL);
```

Three things about it:

- **It is a pull.** Nothing is fetched until you ask for the next batch.
  That is the backpressure: a slow consumer simply asks more slowly,
  rather than buffering without bound or stalling in a way you cannot
  see. This is also why there is no "row" signal — a push API would give
  you no way to say "not so fast", and it would fire on the worker
  thread, which is a footgun with no upside.
- **An empty array means the end, `NULL` means failure.** Never
  conflate them; that is how a broken query becomes an empty table.
- **A stream holds its connection busy** until it is drained or closed.
  Everything else queued on that connection waits behind it. Close it if
  you stop reading early.

Column metadata (`orm_row_stream_get_column_count()` and friends) is
captured when the stream is created, so it is available before the first
fetch and costs nothing to ask for.

`ORM_QUERY_FLAGS_STREAMING` is passed to the driver, but only SQLite acts
on it today. PostgreSQL and MySQL still materialize the whole result
before the stream is created, so on those two a stream buys you a
responsive thread, not a smaller memory footprint.

## Connecting

Opening is the slowest thing the library does over a network — a TCP
connection, possibly TLS, then authentication — so it has its own
asynchronous form:

```c
orm_engine_connect_async (engine, cancellable, on_connected, NULL);
```

The connection adopts the calling thread's `GMainContext`, so its signals
arrive where you expect even though it was built on another thread.

## Introspection

Every `OrmInspector` operation has an `_async` form, running on the
inspected connection's worker. Reading a catalog is several dependent
queries and is the slowest read there is on a large schema — and it is
exactly what a database browser wants to do the moment it connects, while
still drawing itself.

```c
orm_inspector_list_relations_async (inspector, NULL, cancellable,
                                    on_relations, NULL);
orm_inspector_estimate_row_count_async (inspector, "orders", NULL,
                                        cancellable, on_count, NULL);
```

`orm_inspector_estimate_row_count_finish()` takes the `is_estimate` flag
as an out parameter, since a `GTask` carries one value and the count is
meaningless without knowing whether it was counted or guessed.

## Signals

`OrmConnection` has exactly two, and both are emitted in the
`GMainContext` that was the thread-default when the connection was
created — never on the worker. A handler may touch a widget directly.

### `state-changed`

```c
g_signal_connect (connection, "state-changed", G_CALLBACK (on_state), self);

static void
on_state (OrmConnection *conn, OrmConnectionState old, OrmConnectionState new_,
          gpointer user_data)
{
    gtk_widget_set_sensitive (run_button, new_ == ORM_CONNECTION_IDLE);
}
```

The states are `ORM_CONNECTION_CLOSED`, `CONNECTING`, `IDLE` and `BUSY`,
and `orm_connection_get_state()` returns the current one. The pair that
earns the signal is `IDLE` against `BUSY`: because a connection runs one
operation at a time, a second query started while the first is running
has only been *queued*, and saying so is the difference between an
application that looks responsive and one that looks hung.

The signal never fires with `old == new`.

### `notice`

Messages the server sends outside any result: a PostgreSQL `NOTICE` or
`WARNING`, a `RAISE NOTICE` from a function, the "skipping" from
`DROP TABLE IF EXISTS`. libpq's default is to print these to stderr,
which is nobody's idea of a user interface.

**PostgreSQL only.** SQLite has no such channel at all, and MySQL keeps
its warnings until `SHOW WARNINGS` asks for them, so neither ever emits
this. The signal exists on every connection regardless, so you can bind
it once without asking what you are connected to.

## What is not here

- **No signals on `OrmRowStream`.** The pull API already reports
  completion and applies backpressure; a signal would only add a way to
  be called on the wrong thread.
- **No true server-side streaming on PostgreSQL or MySQL.**
  `PQsetSingleRowMode` and `mysql_use_result` are not wired up. The
  streaming API works correctly on both — it just does not save memory
  there yet.
- **No connection pool.** One worker per connection is the concurrency
  model; more parallelism means more connections, opened by you.
