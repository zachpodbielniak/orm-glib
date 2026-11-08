# Connection pools

An `OrmConnection` runs one statement at a time, on one worker thread.
That is a property of every backend, not a limitation of the library — no
client library will run two statements at once on one handle. So the way
to get several threads of database work is several connections, and
`OrmConnectionPool` is what manages them: a bounded set of open
connections for one `OrmEngine`, lent out to one borrower at a time.

```c
g_autoptr(OrmConnectionPool) pool = NULL;

pool = orm_connection_pool_new (engine, 8, &error);
orm_connection_pool_set_init_sql (pool, "SET search_path TO app");
```

Nothing is opened until somebody asks. The pool grows on demand up to
`max-size` and shrinks again when connections sit idle.

## Three ways to use it

### One statement at a time

For independent statements, the one-shot helpers acquire a connection,
run the statement on its worker thread, and give the connection back.
Fire as many as you like; they spread across the pool and queue once it
is full.

```c
static void
on_count (GObject *source, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(OrmResult) rows = NULL;
    g_autoptr(GError) error = NULL;

    rows = orm_connection_pool_query_finish (ORM_CONNECTION_POOL (source),
                                             result, &error);
    if (rows == NULL)
        return report (error);

    show_count (orm_result_get_scalar (rows));
}   /* rows freed here: its connection goes back to the pool */

orm_connection_pool_query_async (pool, "SELECT COUNT(*) FROM orders",
                                 NULL, cancellable, on_count, self);
orm_connection_pool_execute_async (pool, "UPDATE stats SET seen = seen + 1",
                                   NULL, NULL, NULL, NULL);
```

`orm_connection_pool_query()` and `orm_connection_pool_execute()` do the
same synchronously, which is the shape a threaded server wants: each
request thread calls them and blocks only itself.

A one-shot query reads **every row** before it hands you the result —
on the connection's worker thread, for the `_async` form — and releases
the connection first. The `OrmResult` you get is plain memory: keep it as
long as you like, keep a hundred of them, free them in any order; none of
it ties up the pool. The cost is that the whole result is in memory at
once, which is what PostgreSQL and MySQL do anyway. For a result too big
for that, take a lease and stream it (below).

### A lease, for a unit of work

When several statements belong together — a transaction, an insert and
then its `last_insert_rowid`, a session — borrow a connection for the
duration:

```c
OrmConnection *conn;

conn = orm_connection_pool_acquire (pool, NULL, &error);
if (conn == NULL)
    return FALSE;

txn = orm_connection_begin_transaction (conn, &error);
/* ... */
orm_transaction_commit (txn, &error);

orm_connection_pool_release (pool, conn);   /* takes the reference */
```

`orm_connection_pool_acquire_async()` is the same without blocking. While
lent, the connection is yours alone and is an ordinary `OrmConnection`:
everything in [async.md](async.md) applies to it unchanged.

`orm_connection_pool_release()` takes your reference. Do not unref the
connection yourself as well, and do not keep using it afterwards.

If you do unref a lent connection instead of releasing it, the pool
notices when the connection is finalized: it warns ("a lent connection
was finalized without orm_connection_pool_release()"), gives the slot to
the next waiter or back to the pool, and carries on. The connection itself
is gone rather than reused. Treat the warning as a bug to fix, but it will
not slowly strangle the pool.

### Streaming a large result

```c
conn = orm_connection_pool_acquire (pool, NULL, &error);
orm_connection_query_stream_async (conn, "SELECT * FROM events", NULL,
                                   cancellable, on_stream_ready, self);
/* ... fetch batches with orm_row_stream_fetch_async() ...
 * then, once the stream is drained or closed: */
orm_connection_pool_release (pool, conn);
```

Rows arrive a batch at a time and only one batch is in memory. The
connection is yours until you release it — and releasing it closes the
stream, as the next section explains, so drain first.

## What release resets

A connection comes back to the pool as close to fresh as the library can
make it. Each of these is something the next borrower would otherwise
inherit from you:

- **Open cursors are closed.** An `OrmResult` you are still holding from
  `orm_connection_query()` on the leased connection, or an `OrmRowStream`
  from `orm_connection_query_stream_async()`, is shut. Reading on fails
  visibly: `orm_result_next()` returns `FALSE` and
  `orm_result_get_error()` gives `ORM_ERROR_CONNECTION_CLOSED` ("its
  connection was returned to a pool"); a stream's next fetch fails with
  `ORM_ERROR_INVALID_OPERATION`. On SQLite a half-read statement keeps
  its read snapshot, so without this you would silently read data from
  before the next borrower's writes. Freeing such a result later is safe
  and does not wait for the connection. Rows you already took out with
  `orm_result_get_row()` or `orm_result_get_all()` are yours and stay
  valid.
- **An open transaction is rolled back** — including one begun with a raw
  `BEGIN` or `START TRANSACTION`, because the pool asks the backend rather
  than relying on what the library issued. Its `OrmTransaction`, if there
  is one, stops working: `commit`, `rollback` and the savepoint calls fail
  with `ORM_ERROR_TRANSACTION_FAILED`, `orm_transaction_is_active()`
  returns `FALSE`, and finalizing it does nothing. Without that, a
  forgotten transaction object would roll back the *next* borrower's work
  when it was finally freed.
- **The isolation level** is put back to the backend's default if
  `orm_connection_set_isolation_level()` changed it.

What it cannot see, it cannot reset: a `SET` of some other session
variable, a temporary table, a `PRAGMA`. Use `init-sql` for
per-connection setup instead of doing it on each lease, and do not change
session state on a lease that you do not change back.

A connection that is closed, lost its server, or fails to reset is closed
and dropped; the next acquire opens a replacement.

## Waiting

When every connection is lent and the pool is at `max-size`, acquirers
queue **in arrival order**. A released connection goes straight to the
one that has waited longest, so a request is never overtaken by one that
arrived later.

A queued acquire ends one of three ways:

| Outcome | Error |
|---|---|
| A connection comes back | none |
| `acquire-timeout` ms pass (default 30000; 0 waits forever) | `G_IO_ERROR_TIMED_OUT` |
| Its `GCancellable` is cancelled | `G_IO_ERROR_CANCELLED` |
| The pool is closed | `ORM_ERROR_CONNECTION_CLOSED` |

`orm_connection_pool_get_waiting_count()` is the number to watch if you
are sizing a pool: a queue that is often non-empty means `max-size` is too
small for the load, or something is holding connections too long.

## Main loop integration

A pool is bound to one `GMainContext` — the thread-default context of the
thread that created it, or the one passed as `main-context`:

```c
pool = g_initable_new (ORM_TYPE_CONNECTION_POOL, NULL, &error,
                       "engine", engine,
                       "max-size", 8,
                       "main-context", worker_context,
                       "idle-timeout", 120,
                       NULL);
```

That context gets two things:

- **The idle reaper.** A timer that closes connections idle longer than
  `idle-timeout` seconds (default 300; 0 never reaps), oldest first, but
  never below `min-size`. It is an ordinary `GSource`, so it runs only
  while that context is iterated. A program that never runs a main loop
  simply never reaps, which is harmless.
- **Connection signals.** Every connection the pool opens delivers its
  `state-changed` and `notice` signals there, whichever thread it was
  opened on.

Callbacks follow the usual GIO rule instead: an `_async` call's callback
runs in the thread-default context of the thread that *made the call*.
Create the pool on your UI thread and call it from there, and everything
— callbacks, signals, reaping — happens on the UI thread, with every
handshake and statement off it:

| Work | Runs on |
|---|---|
| Opening a connection (TCP, TLS, auth) | a GIO worker thread |
| Validating an idle connection | a GIO worker thread |
| Each statement | that connection's own worker thread |
| Callbacks | the calling thread's main context |
| Reaper, connection signals | the pool's main context |

An idle connection that needs no check is handed over on the next loop
iteration with no thread hop at all.

## Keeping connections healthy

The drivers do not reliably notice a server that went away while a
connection sat idle — a dead socket surfaces as a failed statement like
any other. So before lending a connection that has been idle at least
`validation-interval` seconds (default 30), the pool runs `SELECT 1` on
it, and replaces it if that fails. Set the interval to 0 to check on
every acquire, or `G_MAXUINT` never to.

Connections handed directly from one borrower to the next are not
checked: they were working moments ago.

## Per-connection setup: `init-sql`

`init-sql` runs once on every connection the pool opens, before it is
first lent. If it fails, the connection is closed and the acquire fails
with that error.

```c
/* PostgreSQL */
orm_connection_pool_set_init_sql (pool, "SET search_path TO app, public");

/* SQLite: wait up to 30 s for a lock rather than the default 5 s */
orm_connection_pool_set_init_sql (pool, "PRAGMA busy_timeout = 30000");
```

## SQLite

Three things are different about SQLite.

**`:memory:` cannot be pooled.** Each connection to `sqlite:///:memory:`
is a separate, private, empty database. `orm_connection_pool_new()`
refuses `max-size` above 1 for such an engine with
`ORM_ERROR_NOT_SUPPORTED`. Use a file.

**Lock contention waits, up to 5 s.** SQLite's own default is to fail at
once with "database is locked" whenever another connection holds a
conflicting lock, and with several connections on one file that happened
even under read-only load while connections were opening. orm-glib opens
every SQLite connection with a 5-second busy timeout instead. If a
statement can legitimately wait longer than that, raise it with
`PRAGMA busy_timeout` in `init-sql`.

**Use WAL.** `PRAGMA journal_mode = WAL` on the database (it is
persistent, so once is enough) lets readers run while a writer works;
without it every write blocks every reader.

SQLite allows one writer at a time whatever you do, so a pool speeds up
reads; writes still take turns.

## Shutting down

`orm_connection_pool_close()` closes idle connections at once, fails every
queued acquirer with `ORM_ERROR_CONNECTION_CLOSED`, and closes lent
connections as they are released. Every acquire afterwards fails.
Dropping the last reference to the pool closes it too.

Release every connection you acquire before the pool is finalized; the
pool warns if it is finalized with connections still lent.

## Mistakes the pool absorbs

Each of these used to be a silent failure or a slow leak. Now each one is
contained, and the tests in `tests/test-connection-pool.c` hold that line:

| You… | What happens |
|---|---|
| keep one-shot query results around | Nothing: they hold no connection |
| release with a transaction open, even a raw `BEGIN` | Rolled back; the stale `OrmTransaction` refuses to act |
| release with a result or stream half-read | Cursor closed; reading on reports `ORM_ERROR_CONNECTION_CLOSED` / `_INVALID_OPERATION` |
| release after changing the isolation level | Restored to the backend default |
| `g_object_unref()` a lent connection instead of releasing it | Warning; the slot is recovered |
| `release()` a connection twice, or one from another pool | `g_critical`; the call is ignored and no reference is dropped |
| never call `_acquire_finish()` | The connection goes back when GTask drops the result |
| run several SQLite connections on one file | Lock waits up to 5 s instead of failing at once |
| pool `sqlite:///:memory:` | Refused at construction |

What still bites:

- **Keeping a lease.** A connection you hold is unavailable to everyone
  else, and a pool of four with four leases held by code that waits on a
  fifth waits until `acquire-timeout`. Keep leases short; prefer the
  one-shot helpers.
- **Session state you set by hand** (see *What release resets*).
- **Using a connection after releasing it.** Nothing stops you, and the
  statement runs on someone else's lease. The `g_critical` on a second
  release catches some of this, not all.

## Settings

| Property | Default | Meaning |
|---|---|---|
| `engine` | — | Where connections come from (construct-only) |
| `max-size` | 10 | Most connections open at once (construct-only) |
| `min-size` | 0 | Floor for idle reaping; never opens connections by itself |
| `idle-timeout` | 300 s | Idle time before a connection is closed; 0 = never |
| `acquire-timeout` | 30000 ms | Wait on an exhausted pool; 0 = forever |
| `validation-interval` | 30 s | Idle time before a `SELECT 1` on acquire; 0 = always, `G_MAXUINT` = never |
| `init-sql` | `NULL` | Run on each new connection |
| `main-context` | thread-default | Where the reaper and signals live (construct-only) |

Everything except the construct-only properties can be changed at any
time from any thread. `orm_connection_pool_get_size()`,
`_get_idle_count()` and `_get_waiting_count()` report the live state.

## What is not here

- **No pre-warming.** `min-size` stops the reaper from going lower; it
  does not open connections ahead of demand.
- **No maximum lifetime.** A connection lives until it idles out, fails,
  or the pool closes.
- **No `OrmSession` integration.** Acquire a connection and build the
  session on it with `orm_session_new_with_connection()`; release the
  connection when the session is done.
