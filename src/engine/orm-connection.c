/* orm-connection.c
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

#include "orm-connection.h"
#include "orm-engine.h"
#include "orm-result.h"
#include "orm-row-stream.h"
#include "orm-transaction.h"
#include "orm-worker.h"
#include "../core/orm-error.h"
#include "../driver/orm-driver.h"
#include "../driver/orm-driver-registry.h"
#include "orm-engine-private.h"

/*
 * OrmConnection - a database connection, whatever the database.
 *
 * This is a facade. Every operation forwards to an OrmDriverConnection
 * supplied by the backend's driver, so nothing here knows what a
 * sqlite3, PGconn or MYSQL is. What the facade owns is the part that is
 * genuinely the same everywhere: the open flag, the transaction flag,
 * and the tracked isolation level.
 *
 * It also owns the connection's thread. None of the backends tolerate
 * two statements at once on one handle, so the asynchronous API cannot
 * be a thread pool: there is exactly one worker per connection, created
 * on the first _async call, and every statement runs on it. Ordering and
 * mutual exclusion both fall out of that, with no locking around the
 * backend at all.
 *
 * Once the worker exists the synchronous API uses it too -- see
 * orm_connection_run_confined() -- so a caller may mix the two styles
 * without the two racing for the handle.
 */

/*
 * The task carries a back pointer to the operation that submitted it, so
 * that a worker finishing its job and a cancellation arriving from
 * another thread can agree on which of them gets to answer the task.
 */
#define ORM_CONNECTION_OP_KEY "orm-connection-op"

typedef struct
{
    gint                 ref_count;

    OrmConnection       *connection;    /* Owned */
    GTask               *task;          /* Owned */
    GCancellable        *cancellable;   /* Owned, may be NULL */
    gulong               cancel_id;

    /*
     * Zero until someone commits to completing @task.  Whoever wins the
     * exchange must return the task exactly once; whoever loses must
     * throw its result away.
     */
    gint                 answered;

    OrmWorkerJobFunc     run;
    gpointer             data;
    GDestroyNotify       data_free;
} OrmConnectionOp;

struct _OrmConnection
{
    GObject parent_instance;

    OrmEngine           *engine;       /* Weak reference */
    OrmDriverConnection *driver_conn;
    OrmDialectType       dialect_type;
    gboolean             is_open;
    gboolean             in_transaction;
    OrmIsolationLevel    isolation_level;

    /*
     * Guards the fields a second thread can reach: the driver connection,
     * which the cancellation path calls interrupt() on, and the state,
     * which the worker changes and the owner reads.
     */
    GMutex               lock;
    OrmConnectionState   state;

    OrmWorker           *worker;
    GMainContext        *owner_context;

    /* Signals must not be emitted on an object that is being destroyed. */
    gboolean             in_finalize;
};

enum {
    SIGNAL_STATE_CHANGED,
    SIGNAL_NOTICE,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (OrmConnection, orm_connection, G_TYPE_OBJECT)

static void orm_connection_set_state (OrmConnection      *self,
                                      OrmConnectionState  state);

/* ------------------------------------------------------------------ */
/* Signal emission                                                    */
/* ------------------------------------------------------------------ */

/*
 * Signals are emitted from whichever thread the work happened on, which
 * for anything asynchronous is the worker.  A handler that redraws a
 * window would then run off the main thread, which is a crash waiting
 * for a busy afternoon -- so every emission is bounced into the context
 * that owned the connection when it was created.
 *
 * g_main_context_invoke_full() short-circuits when it is already in that
 * context, so the synchronous path pays nothing for this.
 */

typedef struct
{
    OrmConnection      *connection;
    OrmConnectionState  old_state;
    OrmConnectionState  new_state;
} OrmStateEmission;

typedef struct
{
    OrmConnection *connection;
    gchar         *message;
} OrmNoticeEmission;

static void
orm_state_emission_free (gpointer data)
{
    OrmStateEmission *emission = (OrmStateEmission *) data;

    g_object_unref (emission->connection);
    g_free (emission);
}

static gboolean
orm_connection_emit_state_changed (gpointer data)
{
    OrmStateEmission *emission = (OrmStateEmission *) data;

    g_signal_emit (emission->connection, signals[SIGNAL_STATE_CHANGED], 0,
                   emission->old_state, emission->new_state);

    return G_SOURCE_REMOVE;
}

static void
orm_notice_emission_free (gpointer data)
{
    OrmNoticeEmission *emission = (OrmNoticeEmission *) data;

    g_object_unref (emission->connection);
    g_free (emission->message);
    g_free (emission);
}

static gboolean
orm_connection_emit_notice (gpointer data)
{
    OrmNoticeEmission *emission = (OrmNoticeEmission *) data;

    g_signal_emit (emission->connection, signals[SIGNAL_NOTICE], 0,
                   emission->message);

    return G_SOURCE_REMOVE;
}

/*
 * Records a state change and reports it, doing neither when the state is
 * already what it is being set to: a signal that fires on every
 * statement with nothing to say trains its handlers to ignore it.
 */
static void
orm_connection_set_state (OrmConnection      *self,
                          OrmConnectionState  state)
{
    OrmStateEmission   *emission;
    OrmConnectionState  old_state;

    g_mutex_lock (&self->lock);
    old_state = self->state;
    self->state = state;
    g_mutex_unlock (&self->lock);

    if (old_state == state || self->in_finalize)
        return;

    /*
     * With nobody listening this would queue an idle source that holds a
     * reference to the connection -- and a caller that never runs a main
     * loop, which is most of the library's synchronous users, would never
     * dispatch it.  The connection would then never be finalized.
     */
    if (!g_signal_has_handler_pending (self, signals[SIGNAL_STATE_CHANGED], 0,
                                       FALSE))
        return;

    emission = g_new0 (OrmStateEmission, 1);
    emission->connection = g_object_ref (self);
    emission->old_state = old_state;
    emission->new_state = state;

    g_main_context_invoke_full (self->owner_context, G_PRIORITY_DEFAULT,
                                orm_connection_emit_state_changed, emission,
                                orm_state_emission_free);
}

/*
 * Relays a backend notice.  Drivers that have such a thing expose it as
 * a "notice" signal on their own connection type -- see the PostgreSQL
 * driver -- and it fires wherever the statement is running.
 */
static void
orm_connection_on_driver_notice (OrmDriverConnection *driver_conn,
                                 const gchar         *message,
                                 gpointer             user_data)
{
    OrmConnection     *self = ORM_CONNECTION (user_data);
    OrmNoticeEmission *emission;

    if (message == NULL || self->in_finalize)
        return;

    if (!g_signal_has_handler_pending (self, signals[SIGNAL_NOTICE], 0, FALSE))
        return;

    emission = g_new0 (OrmNoticeEmission, 1);
    emission->connection = g_object_ref (self);
    emission->message = g_strdup (message);

    g_main_context_invoke_full (self->owner_context, G_PRIORITY_DEFAULT,
                                orm_connection_emit_notice, emission,
                                orm_notice_emission_free);
}

/* ------------------------------------------------------------------ */
/* Thread confinement                                                 */
/* ------------------------------------------------------------------ */

/*
 * Runs @func wherever this connection's backend handle may be touched.
 *
 * Three cases, and the middle one is the whole reason this exists:
 *
 *  - No worker: nothing asynchronous has ever been started, so the
 *    caller's thread is the connection's thread.  Run inline, exactly as
 *    the library did before any of this existed.
 *  - A worker, and we are not it: hand the work over and block.  A
 *    synchronous call that reached into the backend directly here would
 *    be racing whatever the worker has queued.
 *  - A worker, and we are it: run inline again.  Anything else waits for
 *    a queue that only this thread can drain.
 */
void
orm_connection_run_confined (OrmConnection     *self,
                             OrmWorkerSyncFunc  func,
                             gpointer           data)
{
    OrmWorker *worker;

    g_return_if_fail (ORM_IS_CONNECTION (self));

    worker = (OrmWorker *) g_atomic_pointer_get (&self->worker);

    if (worker == NULL || orm_worker_is_current (worker))
        func (data);
    else
        orm_worker_invoke_sync (worker, func, data);
}

/*
 * Starts the worker if this connection does not have one yet.
 *
 * Only the owning thread calls this, so there is nothing to serialize:
 * the worker itself can only observe a pointer that was published before
 * the thread it runs on was created.
 */
static OrmWorker *
orm_connection_ensure_worker (OrmConnection *self)
{
    if (self->worker == NULL)
        self->worker = orm_worker_new ("orm-connection");

    return self->worker;
}

static void
orm_connection_finalize (GObject *object)
{
    OrmConnection *self = ORM_CONNECTION (object);

    self->in_finalize = TRUE;

    orm_connection_close (self);

    g_clear_pointer (&self->owner_context, g_main_context_unref);
    g_mutex_clear (&self->lock);

    G_OBJECT_CLASS (orm_connection_parent_class)->finalize (object);
}

static void
orm_connection_class_init (OrmConnectionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_connection_finalize;

    /*
     * OrmConnection::state-changed:
     * @self: The #OrmConnection
     * @old_state: The state being left
     * @new_state: The state being entered
     *
     * Emitted when the connection opens, closes, or starts and finishes
     * an asynchronous operation.  Always in the connection's owning
     * #GMainContext, whichever thread the work ran on.
     */
    signals[SIGNAL_STATE_CHANGED] =
        g_signal_new ("state-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 2,
                      ORM_TYPE_CONNECTION_STATE,
                      ORM_TYPE_CONNECTION_STATE);

    /*
     * OrmConnection::notice:
     * @self: The #OrmConnection
     * @message: The message, with no trailing newline
     *
     * Emitted for a message the server sent outside any result: a
     * PostgreSQL NOTICE or WARNING, a RAISE NOTICE from a function, the
     * "table does not exist, skipping" from a DROP ... IF EXISTS.
     *
     * PostgreSQL is the only backend that reports these.  SQLite has no
     * such channel, and MySQL keeps its warnings until asked for them
     * with SHOW WARNINGS, so neither emits this signal.
     */
    signals[SIGNAL_NOTICE] =
        g_signal_new ("notice",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 1,
                      G_TYPE_STRING);
}

static void
orm_connection_init (OrmConnection *self)
{
    self->engine = NULL;
    self->driver_conn = NULL;
    self->is_open = FALSE;
    self->in_transaction = FALSE;
    self->isolation_level = ORM_ISOLATION_SERIALIZABLE;
    self->state = ORM_CONNECTION_CONNECTING;
    self->worker = NULL;
    self->in_finalize = FALSE;

    g_mutex_init (&self->lock);

    /*
     * Captured here rather than at the first emission because "the
     * thread that owns this connection" is decided by whoever
     * constructed it, and by the time a signal fires the answer would be
     * whichever thread happened to run the statement.
     */
    self->owner_context = g_main_context_ref_thread_default ();
}

/*
 * Internal: re-points the connection at the context that asked for it.
 *
 * orm_engine_connect_async() builds the connection on a worker thread,
 * where the thread-default context is not the caller's; without this the
 * connection would deliver its signals somewhere the caller never looks.
 */
void
orm_connection_set_owner_context (OrmConnection *self,
                                  GMainContext  *context)
{
    g_return_if_fail (ORM_IS_CONNECTION (self));

    if (context == NULL)
        return;

    g_clear_pointer (&self->owner_context, g_main_context_unref);
    self->owner_context = g_main_context_ref (context);
}

/**
 * orm_connection_new:
 * @engine: The engine that owns this connection
 * @error: Return location for error
 *
 * Opens a new database connection.
 *
 * Returns: (transfer full) (nullable): A new #OrmConnection, or %NULL on error
 */
OrmConnection *
orm_connection_new (OrmEngine  *engine,
                    GError    **error)
{
    OrmConnection       *self;
    OrmDriver           *driver;
    OrmDriverConnection *driver_conn;

    g_return_val_if_fail (ORM_IS_ENGINE (engine), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    driver = orm_engine_get_driver (engine);
    if (driver == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_NOT_SUPPORTED,
                     "No driver is registered for this database");
        return NULL;
    }

    driver_conn = orm_driver_open (driver, engine, error);
    if (driver_conn == NULL)
        return NULL;

    self = g_object_new (ORM_TYPE_CONNECTION, NULL);
    self->engine = engine;  /* Weak reference */
    self->driver_conn = driver_conn;
    self->dialect_type = orm_engine_get_dialect_type (engine);
    self->is_open = TRUE;

    /* Each backend opens at its own documented default, not a common one. */
    switch (self->dialect_type)
    {
    case ORM_DIALECT_POSTGRES:
        self->isolation_level = ORM_ISOLATION_READ_COMMITTED;
        break;
    case ORM_DIALECT_MYSQL:
        self->isolation_level = ORM_ISOLATION_REPEATABLE_READ;
        break;
    default:
        self->isolation_level = ORM_ISOLATION_SERIALIZABLE;
        break;
    }

    /*
     * A driver with a channel for out-of-band server messages advertises
     * it as a "notice" signal on its own connection type.  Asking the
     * type system rather than the dialect keeps this working for a
     * backend that lives outside the tree.  The driver connection is
     * owned by, and destroyed before, this object, so the handler cannot
     * outlive its data.
     */
    if (g_signal_lookup ("notice", G_OBJECT_TYPE (driver_conn)) != 0)
        g_signal_connect (driver_conn, "notice",
                          G_CALLBACK (orm_connection_on_driver_notice), self);

    orm_connection_set_state (self, ORM_CONNECTION_IDLE);

    return self;
}

/*
 * Closes the backend handle.  Split out because both the synchronous
 * close and the asynchronous one need it, and they reach it from
 * different threads.
 */
static void
orm_connection_close_driver (gpointer data)
{
    OrmConnection       *self = ORM_CONNECTION (data);
    OrmDriverConnection *driver_conn;

    g_mutex_lock (&self->lock);
    driver_conn = self->driver_conn;
    self->driver_conn = NULL;
    g_mutex_unlock (&self->lock);

    if (driver_conn != NULL)
    {
        orm_driver_connection_close (driver_conn);
        g_object_unref (driver_conn);
    }

    self->is_open = FALSE;
    self->in_transaction = FALSE;
}

/**
 * orm_connection_close:
 * @self: An #OrmConnection
 *
 * Closes the connection.  Safe to call more than once.
 *
 * Anything already queued asynchronously on this connection runs first:
 * the worker is drained and stopped before the backend handle is
 * released, so a pending statement is never cut off mid-flight by a
 * close on another thread.
 */
void
orm_connection_close (OrmConnection *self)
{
    OrmWorker *worker;

    g_return_if_fail (ORM_IS_CONNECTION (self));

    worker = (OrmWorker *) g_atomic_pointer_get (&self->worker);

    if (worker != NULL && !orm_worker_is_current (worker))
    {
        /*
         * Clearing the pointer first means a job still in the queue sees
         * no worker and runs its own work inline -- which is right,
         * because it is already on the only thread that matters.
         */
        g_atomic_pointer_set (&self->worker, NULL);
        orm_worker_shutdown (worker);
    }

    orm_connection_close_driver (self);

    orm_connection_set_state (self, ORM_CONNECTION_CLOSED);
}

/**
 * orm_connection_is_open:
 * @self: An #OrmConnection
 *
 * Returns: %TRUE if the connection is open
 */
gboolean
orm_connection_is_open (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    return self->is_open;
}

/*
 * One statement's worth of arguments and answers, handed to the thread
 * the connection is confined to.  It lives on the caller's stack: that
 * thread is blocked for as long as the worker can see it.
 */
typedef struct
{
    OrmConnection    *connection;
    const gchar      *sql;
    GList            *params;
    OrmQueryFlags     flags;
    GError          **error;
    gboolean          ok;
    OrmDriverResult  *result;
} OrmStatementWork;

static void
orm_connection_execute_work (gpointer data)
{
    OrmStatementWork *work = (OrmStatementWork *) data;

    work->ok = orm_driver_connection_execute (work->connection->driver_conn,
                                              work->sql, work->params,
                                              work->error);
}

static void
orm_connection_query_work (gpointer data)
{
    OrmStatementWork *work = (OrmStatementWork *) data;

    work->result = orm_driver_connection_query (work->connection->driver_conn,
                                                work->sql, work->params,
                                                work->flags, work->error);
    work->ok = work->result != NULL;
}

/**
 * orm_connection_execute:
 * @self: An #OrmConnection
 * @sql: SQL statement
 * @error: Return location for error
 *
 * Executes a statement that returns no rows.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_connection_execute (OrmConnection  *self,
                        const gchar    *sql,
                        GError        **error)
{
    return orm_connection_execute_with_params (self, sql, NULL, error);
}

/**
 * orm_connection_execute_with_params:
 * @self: An #OrmConnection
 * @sql: SQL statement with placeholders
 * @params: (element-type OrmValue) (nullable): Parameter values
 * @error: Return location for error
 *
 * Executes a parameterized statement that returns no rows.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_connection_execute_with_params (OrmConnection  *self,
                                    const gchar    *sql,
                                    GList          *params,
                                    GError        **error)
{
    OrmStatementWork work;

    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    g_return_val_if_fail (sql != NULL, FALSE);
    g_return_val_if_fail (self->is_open, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    work.connection = self;
    work.sql = sql;
    work.params = params;
    work.flags = ORM_QUERY_FLAGS_NONE;
    work.error = error;
    work.ok = FALSE;
    work.result = NULL;

    orm_connection_run_confined (self, orm_connection_execute_work, &work);

    return work.ok;
}

/**
 * orm_connection_query:
 * @self: An #OrmConnection
 * @sql: SQL query
 * @error: Return location for error
 *
 * Executes a query and returns its result set.
 *
 * Returns: (transfer full) (nullable): A new #OrmResult, or %NULL on error
 */
OrmResult *
orm_connection_query (OrmConnection  *self,
                      const gchar    *sql,
                      GError        **error)
{
    return orm_connection_query_with_params (self, sql, NULL, error);
}

/**
 * orm_connection_query_with_params:
 * @self: An #OrmConnection
 * @sql: SQL query with placeholders
 * @params: (element-type OrmValue) (nullable): Parameter values
 * @error: Return location for error
 *
 * Executes a parameterized query and returns its result set.
 *
 * Returns: (transfer full) (nullable): A new #OrmResult, or %NULL on error
 */
OrmResult *
orm_connection_query_with_params (OrmConnection  *self,
                                  const gchar    *sql,
                                  GList          *params,
                                  GError        **error)
{
    OrmStatementWork work;

    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    g_return_val_if_fail (sql != NULL, NULL);
    g_return_val_if_fail (self->is_open, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    work.connection = self;
    work.sql = sql;
    work.params = params;
    work.flags = ORM_QUERY_FLAGS_NONE;
    work.error = error;
    work.ok = FALSE;
    work.result = NULL;

    orm_connection_run_confined (self, orm_connection_query_work, &work);

    if (work.result == NULL)
        return NULL;

    return orm_result_new_for_driver (self, work.result);
}

/**
 * orm_connection_begin_transaction:
 * @self: An #OrmConnection
 * @error: Return location for error
 *
 * Begins a new transaction.
 *
 * Returns: (transfer full) (nullable): A new #OrmTransaction, or %NULL on error
 */
OrmTransaction *
orm_connection_begin_transaction (OrmConnection  *self,
                                  GError        **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    g_return_val_if_fail (self->is_open, NULL);
    g_return_val_if_fail (!self->in_transaction, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    return orm_transaction_new (self, error);
}

typedef struct
{
    OrmConnection     *connection;
    OrmIsolationLevel  level;
    gboolean           for_next_transaction;
    GError           **error;
    gboolean           ok;
} OrmIsolationWork;

static void
orm_connection_isolation_work (gpointer data)
{
    OrmIsolationWork *work = (OrmIsolationWork *) data;

    work->ok = orm_driver_connection_set_isolation_level (work->connection->driver_conn,
                                                          work->level,
                                                          work->for_next_transaction,
                                                          work->error);
}

/**
 * orm_connection_set_isolation_level:
 * @self: An #OrmConnection
 * @level: The isolation level to apply
 * @error: Return location for error
 *
 * Sets the transaction isolation level for the whole session, so it
 * governs every transaction started afterwards on this connection.
 *
 * Support varies by backend: SQLite accepts only
 * %ORM_ISOLATION_SERIALIZABLE (its native behaviour, applied as a no-op)
 * and %ORM_ISOLATION_READ_UNCOMMITTED, and fails the rest with
 * %ORM_ERROR_NOT_SUPPORTED rather than quietly giving you weaker
 * guarantees than you asked for.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_connection_set_isolation_level (OrmConnection      *self,
                                    OrmIsolationLevel   level,
                                    GError            **error)
{
    OrmIsolationWork work;

    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    g_return_val_if_fail (self->is_open, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    work.connection = self;
    work.level = level;
    work.for_next_transaction = FALSE;
    work.error = error;
    work.ok = FALSE;

    orm_connection_run_confined (self, orm_connection_isolation_work, &work);

    if (!work.ok)
        return FALSE;

    self->isolation_level = level;
    return TRUE;
}

/**
 * orm_connection_get_isolation_level:
 * @self: An #OrmConnection
 *
 * Gets the isolation level this connection is known to be using.
 *
 * The value is tracked rather than queried: it starts at the backend's
 * documented default and follows every successful
 * orm_connection_set_isolation_level(). A level changed behind the
 * library's back -- by raw SQL, say -- is not reflected here.
 *
 * Returns: The current #OrmIsolationLevel
 */
OrmIsolationLevel
orm_connection_get_isolation_level (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), ORM_ISOLATION_SERIALIZABLE);
    return self->isolation_level;
}

/**
 * orm_connection_begin_transaction_with_isolation:
 * @self: An #OrmConnection
 * @level: The isolation level for this transaction only
 * @error: Return location for error
 *
 * Begins a transaction that runs at @level, leaving the session default
 * untouched.
 *
 * Returns: (transfer full) (nullable): A new #OrmTransaction, or %NULL on error
 */
OrmTransaction *
orm_connection_begin_transaction_with_isolation (OrmConnection      *self,
                                                 OrmIsolationLevel   level,
                                                 GError            **error)
{
    OrmTransaction   *transaction;
    OrmIsolationWork  work;

    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    g_return_val_if_fail (self->is_open, NULL);
    g_return_val_if_fail (!self->in_transaction, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    work.connection = self;
    work.level = level;
    work.for_next_transaction = TRUE;
    work.error = error;
    work.ok = FALSE;

    /*
     * MySQL wants the level set before the transaction opens; PostgreSQL
     * wants it as the transaction's first statement.  SQLite's pragma is
     * connection-scoped and so belongs before BEGIN as well.
     */
    if (self->dialect_type != ORM_DIALECT_POSTGRES)
    {
        orm_connection_run_confined (self, orm_connection_isolation_work, &work);

        if (!work.ok)
            return NULL;

        return orm_transaction_new (self, error);
    }

    transaction = orm_transaction_new (self, error);
    if (transaction == NULL)
        return NULL;

    orm_connection_run_confined (self, orm_connection_isolation_work, &work);

    if (!work.ok)
    {
        /*
         * Roll back rather than hand back a transaction running at the
         * wrong isolation level -- a caller that asked for SERIALIZABLE
         * and silently got READ COMMITTED is the worst outcome here.
         */
        orm_transaction_rollback (transaction, NULL);
        g_object_unref (transaction);
        return NULL;
    }

    return transaction;
}

/**
 * orm_connection_in_transaction:
 * @self: An #OrmConnection
 *
 * Checks if a transaction is active.
 *
 * Returns: %TRUE if in a transaction
 */
gboolean
orm_connection_in_transaction (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    return self->in_transaction;
}

/*
 * The two per-statement counters.  They read state the backend keeps on
 * the connection handle, so they belong on the connection's thread like
 * everything else -- asking sqlite3_changes() from another thread while
 * a statement runs there is undefined, not merely stale.
 */
typedef struct
{
    OrmConnection *connection;
    gint64         last_insert_id;
    gint           changes;
} OrmMetadataWork;

static void
orm_connection_last_insert_id_work (gpointer data)
{
    OrmMetadataWork *work = (OrmMetadataWork *) data;

    work->last_insert_id =
        orm_driver_connection_get_last_insert_id (work->connection->driver_conn);
}

static void
orm_connection_changes_work (gpointer data)
{
    OrmMetadataWork *work = (OrmMetadataWork *) data;

    work->changes =
        orm_driver_connection_get_changes (work->connection->driver_conn);
}

/**
 * orm_connection_get_last_insert_rowid:
 * @self: An #OrmConnection
 *
 * Gets the row id generated by the last INSERT.
 *
 * Not every backend has one: PostgreSQL keeps the value in a sequence
 * rather than on the connection, and returns 0 here.  Use
 * INSERT ... RETURNING there.
 *
 * Returns: The last insert rowid, or 0
 */
gint64
orm_connection_get_last_insert_rowid (OrmConnection *self)
{
    OrmMetadataWork work;

    g_return_val_if_fail (ORM_IS_CONNECTION (self), 0);

    if (self->driver_conn == NULL)
        return 0;

    work.connection = self;
    work.last_insert_id = 0;
    work.changes = 0;

    orm_connection_run_confined (self, orm_connection_last_insert_id_work, &work);

    return work.last_insert_id;
}

/**
 * orm_connection_get_changes:
 * @self: An #OrmConnection
 *
 * Gets the number of rows affected by the last statement.
 *
 * Returns: The number of affected rows
 */
gint
orm_connection_get_changes (OrmConnection *self)
{
    OrmMetadataWork work;

    g_return_val_if_fail (ORM_IS_CONNECTION (self), 0);

    if (self->driver_conn == NULL)
        return 0;

    work.connection = self;
    work.last_insert_id = 0;
    work.changes = 0;

    orm_connection_run_confined (self, orm_connection_changes_work, &work);

    return work.changes;
}

/**
 * orm_connection_get_engine:
 * @self: An #OrmConnection
 *
 * Returns: (transfer none) (nullable): The engine that owns this connection
 */
OrmEngine *
orm_connection_get_engine (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    return self->engine;
}

/*
 * Internal: OrmTransaction tracks whether a transaction is open, since
 * BEGIN and COMMIT go through orm_connection_execute like any other
 * statement and the connection cannot otherwise tell.
 */
void
orm_connection_set_in_transaction (OrmConnection *self,
                                   gboolean       in_transaction)
{
    g_return_if_fail (ORM_IS_CONNECTION (self));
    self->in_transaction = in_transaction;
}

/*
 * Internal: the driver connection, for the layers that need to reach the
 * backend directly (OrmResult's streaming path, the inspector).
 */
OrmDriverConnection *
orm_connection_get_driver_connection (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    return self->driver_conn;
}

/*
 * Internal: the dialect this connection speaks, cached from the engine so
 * callers do not have to reach through a weak reference for it.
 */
OrmDialectType
orm_connection_get_dialect_type (OrmConnection *self)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), ORM_DIALECT_SQLITE);
    return self->dialect_type;
}

/**
 * orm_connection_get_state:
 * @self: An #OrmConnection
 *
 * Gets the connection's current state.
 *
 * Returns: The current #OrmConnectionState
 */
OrmConnectionState
orm_connection_get_state (OrmConnection *self)
{
    OrmConnectionState state;

    g_return_val_if_fail (ORM_IS_CONNECTION (self), ORM_CONNECTION_CLOSED);

    g_mutex_lock (&self->lock);
    state = self->state;
    g_mutex_unlock (&self->lock);

    return state;
}

/* ------------------------------------------------------------------ */
/* Asynchronous operations                                            */
/* ------------------------------------------------------------------ */

/*
 * Asks the backend to abandon whatever it is doing.
 *
 * Called from whichever thread cancelled, not from the worker, which is
 * why the driver connection is taken under the lock and held for the
 * call: a close racing a cancel would otherwise pull the handle out from
 * under interrupt().
 *
 * Returns: %TRUE if the backend was asked to stop
 */
static gboolean
orm_connection_interrupt (OrmConnection  *self,
                          GError        **error)
{
    OrmDriverConnection *driver_conn;
    gboolean             interrupted;

    g_mutex_lock (&self->lock);
    driver_conn = self->driver_conn != NULL ? g_object_ref (self->driver_conn) : NULL;
    g_mutex_unlock (&self->lock);

    if (driver_conn == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                     "Connection is closed");
        return FALSE;
    }

    interrupted = orm_driver_connection_interrupt (driver_conn, error);
    g_object_unref (driver_conn);

    return interrupted;
}

/*
 * A failure that means the connection itself is gone, rather than that
 * one statement was bad.  Nothing queued behind it can succeed either,
 * so the state says so instead of letting each queued operation discover
 * it separately.
 */
static void
orm_connection_note_error (OrmConnection *self,
                           const GError  *error)
{
    if (error == NULL || error->domain != ORM_ERROR)
        return;

    if (error->code == ORM_ERROR_CONNECTION ||
        error->code == ORM_ERROR_CONNECTION_FAILED ||
        error->code == ORM_ERROR_CONNECTION_CLOSED)
        orm_connection_set_state (self, ORM_CONNECTION_CLOSED);
}

static OrmConnectionOp *
orm_connection_op_ref (OrmConnectionOp *op)
{
    g_atomic_int_inc (&op->ref_count);
    return op;
}

static void
orm_connection_op_unref (OrmConnectionOp *op)
{
    if (!g_atomic_int_dec_and_test (&op->ref_count))
        return;

    if (op->data_free != NULL && op->data != NULL)
        op->data_free (op->data);

    g_clear_object (&op->cancellable);
    g_clear_object (&op->task);
    g_clear_object (&op->connection);
    g_free (op);
}

/*
 * Internal: whoever wins this owns the job's completion.
 *
 * A cancelled operation has two threads racing to finish it -- the
 * worker, which is about to hand back a result, and the cancel handler,
 * which wants to hand back %G_IO_ERROR_CANCELLED.  Returning a #GTask
 * twice is fatal, so exactly one of them may.
 *
 * Returns: %TRUE if the caller may return @task
 */
static gboolean
orm_connection_task_answer (GTask *task)
{
    OrmConnectionOp *op;

    op = (OrmConnectionOp *) g_object_get_data (G_OBJECT (task),
                                                ORM_CONNECTION_OP_KEY);
    if (op == NULL)
        return TRUE;

    return g_atomic_int_compare_and_exchange (&op->answered, 0, 1);
}

/*
 * Internal: whether a job should start at all.
 *
 * Cancelling something that is still queued should not run it, and this
 * is where that is noticed -- the worker reaches the job only once
 * everything ahead of it is done, which may be long after the caller
 * gave up on it.
 *
 * Returns: %TRUE if the job should go ahead
 */
gboolean
orm_connection_task_may_run (GTask *task)
{
    GCancellable *cancellable = g_task_get_cancellable (task);

    if (cancellable == NULL || !g_cancellable_is_cancelled (cancellable))
        return TRUE;

    if (orm_connection_task_answer (task))
        g_task_return_error_if_cancelled (task);

    return FALSE;
}

/*
 * Internal: whether a finished job may report its result.
 *
 * %FALSE means the caller must throw that result away: either the cancel
 * handler has already answered the task, or the operation was cancelled
 * while it ran and the backend's own "interrupted" error is noise the
 * caller does not need to see.
 *
 * Returns: %TRUE if the caller may return @task
 */
gboolean
orm_connection_task_may_return (GTask *task)
{
    if (!orm_connection_task_answer (task))
        return FALSE;

    return !g_task_return_error_if_cancelled (task);
}

/*
 * Cancellation, from whichever thread called g_cancellable_cancel().
 */
static void
orm_connection_op_cancelled (GCancellable *cancellable,
                             gpointer      user_data)
{
    OrmConnectionOp   *op = (OrmConnectionOp *) user_data;
    g_autoptr(GError)  error = NULL;

    /*
     * When the backend can be interrupted, the statement fails almost at
     * once and the worker answers the task -- which is the better
     * outcome, because by then the connection really is free.
     */
    if (orm_connection_interrupt (op->connection, &error))
        return;

    /*
     * It cannot be (MySQL has no thread-safe interrupt).  Answering now
     * and letting the statement run itself out is the honest trade: the
     * caller stops waiting immediately, the connection stays busy until
     * the server is done, and the result is discarded when it arrives.
     */
    if (orm_connection_task_answer (op->task))
        g_task_return_new_error (op->task, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                                 "Operation was cancelled");
}

/*
 * The worker-side wrapper every asynchronous operation goes through:
 * report that the connection is busy, run the job, take the cancellation
 * wiring back down, report that it is idle again.
 */
static void
orm_connection_op_run (gpointer  data,
                       GTask    *task)
{
    OrmConnectionOp *op = (OrmConnectionOp *) data;

    orm_connection_set_state (op->connection, ORM_CONNECTION_BUSY);

    op->run (op->data, task);

    /*
     * Disconnecting here rather than in the operation's destructor is
     * deliberate: g_cancellable_disconnect() waits for a handler that is
     * already running, and doing that on the worker is safe, whereas
     * doing it from inside the handler would deadlock.
     */
    if (op->cancellable != NULL && op->cancel_id != 0)
    {
        g_cancellable_disconnect (op->cancellable, op->cancel_id);
        op->cancel_id = 0;
    }

    if (orm_connection_get_state (op->connection) != ORM_CONNECTION_CLOSED)
        orm_connection_set_state (op->connection, ORM_CONNECTION_IDLE);
}

/*
 * Internal: queues @run on this connection's worker, wired for
 * cancellation and bracketed by the state signal.
 *
 * Everything asynchronous in the library goes through here, including
 * the inspector and the row stream, which is what keeps one connection's
 * operations in one queue on one thread.
 */
void
orm_connection_submit_async (OrmConnection    *self,
                             GTask            *task,
                             OrmWorkerJobFunc  run,
                             gpointer          data,
                             GDestroyNotify    data_free)
{
    OrmConnectionOp *op;
    OrmWorker       *worker;

    g_return_if_fail (ORM_IS_CONNECTION (self));
    g_return_if_fail (G_IS_TASK (task));
    g_return_if_fail (run != NULL);

    op = g_new0 (OrmConnectionOp, 1);
    op->ref_count = 1;
    op->connection = g_object_ref (self);
    op->task = g_object_ref (task);
    op->cancellable = g_task_get_cancellable (task);
    op->run = run;
    op->data = data;
    op->data_free = data_free;

    if (op->cancellable != NULL)
        g_object_ref (op->cancellable);

    /*
     * A bare pointer, on purpose: the operation owns the task, so an
     * owning link back would be a cycle neither end ever breaks.
     */
    g_object_set_data (G_OBJECT (task), ORM_CONNECTION_OP_KEY, op);

    if (op->cancellable != NULL)
        op->cancel_id = g_cancellable_connect (op->cancellable,
                                               G_CALLBACK (orm_connection_op_cancelled),
                                               orm_connection_op_ref (op),
                                               (GDestroyNotify) orm_connection_op_unref);

    worker = orm_connection_ensure_worker (self);

    orm_worker_push (worker, orm_connection_op_run, task, op,
                     (GDestroyNotify) orm_connection_op_unref);
}

/*
 * The backend handle, or a set @error explaining that there is none.
 */
static OrmDriverConnection *
orm_connection_require_driver (OrmConnection  *self,
                               GError        **error)
{
    if (self->driver_conn == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION_CLOSED,
                     "Connection is closed");
        return NULL;
    }

    return self->driver_conn;
}

/*
 * A statement queued for the worker.
 *
 * The parameter list is copied because the caller's is (transfer none)
 * and may be freed the moment the _async call returns -- which is the
 * usual thing to do with a list built for one call.
 */
typedef struct
{
    gchar         *sql;
    GList         *params;
    OrmQueryFlags  flags;
} OrmStatementJob;

static GList *
orm_params_copy (GList *params)
{
    GList *copy = NULL;
    GList *l;

    for (l = params; l != NULL; l = l->next)
        copy = g_list_prepend (copy, orm_value_copy ((const OrmValue *) l->data));

    return g_list_reverse (copy);
}

static OrmStatementJob *
orm_statement_job_new (const gchar   *sql,
                       GList         *params,
                       OrmQueryFlags  flags)
{
    OrmStatementJob *job;

    job = g_new0 (OrmStatementJob, 1);
    job->sql = g_strdup (sql);
    job->params = orm_params_copy (params);
    job->flags = flags;

    return job;
}

static void
orm_statement_job_free (gpointer data)
{
    OrmStatementJob *job = (OrmStatementJob *) data;

    g_free (job->sql);
    g_list_free_full (job->params, (GDestroyNotify) orm_value_free);
    g_free (job);
}

static void
orm_connection_execute_job (gpointer  data,
                            GTask    *task)
{
    OrmStatementJob     *job = (OrmStatementJob *) data;
    OrmConnection       *self = ORM_CONNECTION (g_task_get_source_object (task));
    OrmDriverConnection *driver_conn;
    GError              *error = NULL;
    gboolean             ok = FALSE;

    if (!orm_connection_task_may_run (task))
        return;

    driver_conn = orm_connection_require_driver (self, &error);
    if (driver_conn != NULL)
        ok = orm_driver_connection_execute (driver_conn, job->sql, job->params,
                                            &error);

    orm_connection_note_error (self, error);

    if (!orm_connection_task_may_return (task))
    {
        g_clear_error (&error);
        return;
    }

    if (ok)
        g_task_return_boolean (task, TRUE);
    else
        g_task_return_error (task, g_steal_pointer (&error));
}

static void
orm_connection_query_job (gpointer  data,
                          GTask    *task)
{
    OrmStatementJob     *job = (OrmStatementJob *) data;
    OrmConnection       *self = ORM_CONNECTION (g_task_get_source_object (task));
    OrmDriverConnection *driver_conn;
    OrmDriverResult     *driver_result = NULL;
    GError              *error = NULL;

    if (!orm_connection_task_may_run (task))
        return;

    driver_conn = orm_connection_require_driver (self, &error);
    if (driver_conn != NULL)
        driver_result = orm_driver_connection_query (driver_conn, job->sql,
                                                     job->params, job->flags,
                                                     &error);

    orm_connection_note_error (self, error);

    if (!orm_connection_task_may_return (task))
    {
        g_clear_object (&driver_result);
        g_clear_error (&error);
        return;
    }

    if (driver_result == NULL)
    {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    /*
     * The wrapper is built here rather than in the _finish call because
     * a streaming result reads its column metadata from the backend, and
     * that has to happen on the connection's thread like everything else.
     */
    if ((job->flags & ORM_QUERY_FLAGS_STREAMING) != 0)
        g_task_return_pointer (task,
                               orm_row_stream_new_for_driver (self, driver_result),
                               g_object_unref);
    else
        g_task_return_pointer (task,
                               orm_result_new_for_driver (self, driver_result),
                               g_object_unref);
}

static void
orm_connection_close_job (gpointer  data,
                          GTask    *task)
{
    OrmConnection *self = ORM_CONNECTION (g_task_get_source_object (task));

    if (!orm_connection_task_may_run (task))
        return;

    orm_connection_close_driver (self);
    orm_connection_set_state (self, ORM_CONNECTION_CLOSED);

    if (!orm_connection_task_may_return (task))
        return;

    g_task_return_boolean (task, TRUE);
}

/**
 * orm_connection_execute_async:
 * @self: An #OrmConnection
 * @sql: SQL statement with placeholders
 * @params: (element-type OrmValue) (nullable) (transfer none): Parameter values
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the statement has run
 * @user_data: (closure): Data for @callback
 *
 * Executes a statement that returns no rows, on the connection's worker
 * thread.  @params is copied before this returns, so the caller may free
 * it immediately.
 */
void
orm_connection_execute_async (OrmConnection       *self,
                              const gchar         *sql,
                              GList               *params,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (ORM_IS_CONNECTION (self));
    g_return_if_fail (sql != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_connection_execute_async);

    orm_connection_submit_async (self, task, orm_connection_execute_job,
                                 orm_statement_job_new (sql, params,
                                                        ORM_QUERY_FLAGS_NONE),
                                 orm_statement_job_free);
}

/**
 * orm_connection_execute_finish:
 * @self: An #OrmConnection
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_connection_execute_async().
 *
 * Returns: %TRUE on success
 */
gboolean
orm_connection_execute_finish (OrmConnection  *self,
                               GAsyncResult   *result,
                               GError        **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * orm_connection_query_async:
 * @self: An #OrmConnection
 * @sql: SQL query with placeholders
 * @params: (element-type OrmValue) (nullable) (transfer none): Parameter values
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the result set is ready
 * @user_data: (closure): Data for @callback
 *
 * Runs a query on the connection's worker thread.  @params is copied
 * before this returns, so the caller may free it immediately.
 */
void
orm_connection_query_async (OrmConnection       *self,
                            const gchar         *sql,
                            GList               *params,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (ORM_IS_CONNECTION (self));
    g_return_if_fail (sql != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_connection_query_async);

    orm_connection_submit_async (self, task, orm_connection_query_job,
                                 orm_statement_job_new (sql, params,
                                                        ORM_QUERY_FLAGS_NONE),
                                 orm_statement_job_free);
}

/**
 * orm_connection_query_finish:
 * @self: An #OrmConnection
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_connection_query_async().
 *
 * Returns: (transfer full) (nullable): The result set, or %NULL on error
 */
OrmResult *
orm_connection_query_finish (OrmConnection  *self,
                             GAsyncResult   *result,
                             GError        **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return (OrmResult *) g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * orm_connection_query_stream_async:
 * @self: An #OrmConnection
 * @sql: SQL query with placeholders
 * @params: (element-type OrmValue) (nullable) (transfer none): Parameter values
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the stream is ready
 * @user_data: (closure): Data for @callback
 *
 * Runs a query and hands back an #OrmRowStream that fetches rows a batch
 * at a time.  @params is copied before this returns.
 */
void
orm_connection_query_stream_async (OrmConnection       *self,
                                   const gchar         *sql,
                                   GList               *params,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (ORM_IS_CONNECTION (self));
    g_return_if_fail (sql != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_connection_query_stream_async);

    orm_connection_submit_async (self, task, orm_connection_query_job,
                                 orm_statement_job_new (sql, params,
                                                        ORM_QUERY_FLAGS_STREAMING),
                                 orm_statement_job_free);
}

/**
 * orm_connection_query_stream_finish:
 * @self: An #OrmConnection
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_connection_query_stream_async().
 *
 * Returns: (transfer full) (nullable): The row stream, or %NULL on error
 */
OrmRowStream *
orm_connection_query_stream_finish (OrmConnection  *self,
                                    GAsyncResult   *result,
                                    GError        **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return (OrmRowStream *) g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * orm_connection_close_async:
 * @self: An #OrmConnection
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the connection is closed
 * @user_data: (closure): Data for @callback
 *
 * Closes the connection once everything queued ahead of it has run.
 *
 * The worker thread itself outlives this call and is joined when the
 * connection is finalized: stopping a thread from a job running on it is
 * not a thing that ends well.
 */
void
orm_connection_close_async (OrmConnection       *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (ORM_IS_CONNECTION (self));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_connection_close_async);

    orm_connection_submit_async (self, task, orm_connection_close_job,
                                 NULL, NULL);
}

/**
 * orm_connection_close_finish:
 * @self: An #OrmConnection
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_connection_close_async().
 *
 * Returns: %TRUE on success
 */
gboolean
orm_connection_close_finish (OrmConnection  *self,
                             GAsyncResult   *result,
                             GError        **error)
{
    g_return_val_if_fail (ORM_IS_CONNECTION (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}
