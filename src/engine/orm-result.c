/* orm-result.c
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

#include "orm-result.h"
#include "orm-connection.h"
#include "orm-row.h"
#include "../core/orm-error.h"
#include "../driver/orm-driver-result.h"
#include "orm-engine-private.h"

/*
 * OrmResult - a query's result set.
 *
 * A facade over OrmDriverResult, which is a pull cursor: fetch_row hands
 * back the next row or %NULL. The public API here is a step-then-read
 * pair instead -- orm_result_next() advances and orm_result_get_row()
 * reads -- so this type holds the row between the two calls.
 *
 * The step/read split cannot report an error, since next() returns only
 * a gboolean and false has always meant "no more rows". A failed fetch
 * is therefore stashed and surfaced by orm_result_get_error(), which
 * lets a caller that cares tell a broken query from an empty one without
 * breaking one that does not.
 *
 * A result can also be buffered (orm_result_buffer(), used by the pool's
 * one-shot queries): every row is read up front on the connection's
 * thread, the column metadata is copied, the cursor is closed and the
 * connection is let go.  From then on the result reads from memory and
 * holds nothing of the connection's.
 *
 * While a cursor is open the connection tracks it, and a pool taking the
 * connection back closes it (orm_result_invalidate()); reading on then
 * reports an error rather than rows from a stale snapshot.
 */

/* Column metadata, copied for a buffered result */
typedef struct
{
    gchar        *name;
    gchar        *type_name;
    OrmValueType  value_type;
} OrmResultColumn;

struct _OrmResult
{
    GObject parent_instance;

    /*
     * Referenced, because reading a result reaches back into the
     * connection: SQLite steps a statement that belongs to the database
     * handle, and every fetch has to happen on the thread that
     * connection is confined to.  A result outliving its connection
     * would be stepping a closed database.
     */
    OrmConnection   *connection;
    OrmDriverResult *driver_result;
    OrmRow          *current_row;
    GError          *error;
    gboolean         is_closed;
    gboolean         exhausted;

    /* Buffered results only; both NULL otherwise */
    GQueue          *rows;          /* OrmRow *, owned */
    GArray          *columns;       /* OrmResultColumn */
};

G_DEFINE_TYPE (OrmResult, orm_result, G_TYPE_OBJECT)

static void
orm_result_column_clear (gpointer data)
{
    OrmResultColumn *column = (OrmResultColumn *) data;

    g_free (column->name);
    g_free (column->type_name);
}

static void
orm_result_free_rows (GQueue *rows)
{
    g_queue_free_full (rows, g_object_unref);
}

static void
orm_result_finalize (GObject *object)
{
    OrmResult *self = ORM_RESULT (object);

    orm_result_close (self);

    g_clear_object (&self->current_row);
    g_clear_object (&self->connection);
    g_clear_error (&self->error);
    g_clear_pointer (&self->rows, orm_result_free_rows);
    g_clear_pointer (&self->columns, g_array_unref);

    G_OBJECT_CLASS (orm_result_parent_class)->finalize (object);
}

static void
orm_result_class_init (OrmResultClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_result_finalize;
}

static void
orm_result_init (OrmResult *self)
{
    self->connection = NULL;
    self->driver_result = NULL;
    self->current_row = NULL;
    self->error = NULL;
    self->is_closed = FALSE;
    self->exhausted = FALSE;
    self->rows = NULL;
    self->columns = NULL;
}

static void orm_result_close_work (gpointer data);

/*
 * Called by the connection when a pool takes it back with this result's
 * cursor still open.  Closes the cursor where the connection's work runs
 * and records why, so the next orm_result_next() fails visibly instead of
 * quietly reporting the end of the rows.
 */
static void
orm_result_invalidate (gpointer data)
{
    OrmResult *self = ORM_RESULT (data);

    orm_connection_run_confined (self->connection, orm_result_close_work, self);

    g_clear_object (&self->current_row);
    self->exhausted = TRUE;

    if (self->error == NULL)
        self->error = g_error_new_literal (ORM_ERROR, ORM_ERROR_CONNECTION_CLOSED,
                                           "Result is no longer readable: its "
                                           "connection was returned to a pool");
}

/*
 * Internal: wraps a driver result.  Called by OrmConnection, which is the
 * only thing that has a driver result to hand over.
 *
 * Returns: (transfer full): A new #OrmResult taking ownership of @driver_result
 */
OrmResult *
orm_result_new_for_driver (OrmConnection   *connection,
                           OrmDriverResult *driver_result)
{
    OrmResult *self;

    g_return_val_if_fail (ORM_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (ORM_IS_DRIVER_RESULT (driver_result), NULL);

    self = g_object_new (ORM_TYPE_RESULT, NULL);
    self->connection = g_object_ref (connection);
    self->driver_result = driver_result;

    orm_connection_track_cursor (connection, G_OBJECT (self), orm_result_invalidate);

    return self;
}

/*
 * One step of the cursor, run on the connection's thread.
 */
typedef struct
{
    OrmResult *result;
    OrmRow    *row;
} OrmResultFetchWork;

static void
orm_result_fetch_work (gpointer data)
{
    OrmResultFetchWork *work = (OrmResultFetchWork *) data;
    OrmResult          *self = work->result;

    /* A pool may have closed the cursor since the caller checked */
    if (self->driver_result == NULL)
        return;

    work->row = orm_driver_result_fetch_row (self->driver_result, &self->error);
}

/**
 * orm_result_next:
 * @self: An #OrmResult
 *
 * Advances to the next row.
 *
 * A %FALSE return means either that the rows are exhausted or that the
 * fetch failed; orm_result_get_error() distinguishes the two.
 *
 * Returns: %TRUE if there is another row
 */
gboolean
orm_result_next (OrmResult *self)
{
    OrmResultFetchWork  work;
    OrmRow             *row;

    g_return_val_if_fail (ORM_IS_RESULT (self), FALSE);
    g_return_val_if_fail (!self->is_closed, FALSE);

    g_clear_object (&self->current_row);

    if (self->exhausted)
        return FALSE;

    /* Buffered: rows are already here */
    if (self->rows != NULL)
    {
        self->current_row = (OrmRow *) g_queue_pop_head (self->rows);
        if (self->current_row == NULL)
            self->exhausted = TRUE;
        return self->current_row != NULL;
    }

    if (self->driver_result == NULL)
        return FALSE;

    /*
     * Stepping goes through the connection, so a result being read here
     * cannot collide with a statement the connection's worker is running
     * for someone else.  With no worker this is a direct call, exactly
     * as it always was.
     */
    work.result = self;
    work.row = NULL;

    orm_connection_run_confined (self->connection, orm_result_fetch_work, &work);

    row = work.row;

    if (row == NULL)
    {
        /* Either end of results or a failure; self->error says which. */
        self->exhausted = TRUE;
        return FALSE;
    }

    self->current_row = row;
    return TRUE;
}

/**
 * orm_result_get_row:
 * @self: An #OrmResult
 *
 * Gets the row orm_result_next() advanced to.
 *
 * Returns: (transfer none) (nullable): The current row
 */
OrmRow *
orm_result_get_row (OrmResult *self)
{
    g_return_val_if_fail (ORM_IS_RESULT (self), NULL);
    return self->current_row;
}

/**
 * orm_result_get_error:
 * @self: An #OrmResult
 *
 * Gets the error that ended iteration, if one did.
 *
 * Iteration stops on both a genuine end of results and a failed fetch,
 * and orm_result_next() cannot tell them apart on its own. This is how a
 * caller checks which happened.
 *
 * Returns: (transfer none) (nullable): The error, or %NULL if none occurred
 */
const GError *
orm_result_get_error (OrmResult *self)
{
    g_return_val_if_fail (ORM_IS_RESULT (self), NULL);
    return self->error;
}

/**
 * orm_result_get_column_count:
 * @self: An #OrmResult
 *
 * Returns: The number of columns
 */
gint
orm_result_get_column_count (OrmResult *self)
{
    g_return_val_if_fail (ORM_IS_RESULT (self), 0);

    if (self->columns != NULL)
        return (gint) self->columns->len;

    if (self->driver_result == NULL)
        return 0;

    return orm_driver_result_get_column_count (self->driver_result);
}

/**
 * orm_result_get_column_type:
 * @self: An #OrmResult
 * @index: Zero-based column index
 *
 * Gets the value type a column's values decode to, taken from the
 * column's declared type rather than from any particular row.  That
 * distinction is the whole point: a per-row answer reports
 * %ORM_VALUE_NULL for a NULL integer, which tells a caller nothing about
 * the column and is exactly when it most needs to know.
 *
 * Returns: The #OrmValueType, or %ORM_VALUE_NULL when the backend cannot say
 */
OrmValueType
orm_result_get_column_type (OrmResult *self,
                            gint       index)
{
    g_return_val_if_fail (ORM_IS_RESULT (self), ORM_VALUE_NULL);

    if (self->columns != NULL)
    {
        if (index < 0 || (guint) index >= self->columns->len)
            return ORM_VALUE_NULL;
        return g_array_index (self->columns, OrmResultColumn, index).value_type;
    }

    if (self->driver_result == NULL)
        return ORM_VALUE_NULL;

    return orm_driver_result_get_column_value_type (self->driver_result, index);
}

/**
 * orm_result_get_column_type_name:
 * @self: An #OrmResult
 * @index: Zero-based column index
 *
 * Gets the backend's own name for a column's declared type, such as
 * "VARCHAR(255)" or "BIGINT".
 *
 * Returns: (transfer none) (nullable): The type name, or %NULL
 */
const gchar *
orm_result_get_column_type_name (OrmResult *self,
                                 gint       index)
{
    g_return_val_if_fail (ORM_IS_RESULT (self), NULL);

    if (self->columns != NULL)
    {
        if (index < 0 || (guint) index >= self->columns->len)
            return NULL;
        return g_array_index (self->columns, OrmResultColumn, index).type_name;
    }

    if (self->driver_result == NULL)
        return NULL;

    return orm_driver_result_get_column_type_name (self->driver_result, index);
}

/**
 * orm_result_get_column_name:
 * @self: An #OrmResult
 * @index: Zero-based column index
 *
 * Returns: (transfer none) (nullable): The column name
 */
const gchar *
orm_result_get_column_name (OrmResult *self,
                            gint       index)
{
    g_return_val_if_fail (ORM_IS_RESULT (self), NULL);

    if (self->columns != NULL)
    {
        if (index < 0 || (guint) index >= self->columns->len)
            return NULL;
        return g_array_index (self->columns, OrmResultColumn, index).name;
    }

    if (self->driver_result == NULL)
        return NULL;

    return orm_driver_result_get_column_name (self->driver_result, index);
}

/**
 * orm_result_get_all:
 * @self: An #OrmResult
 *
 * Reads every remaining row.
 *
 * Returns: (transfer full) (element-type OrmRow): The rows
 */
GList *
orm_result_get_all (OrmResult *self)
{
    GList *rows = NULL;

    g_return_val_if_fail (ORM_IS_RESULT (self), NULL);

    while (orm_result_next (self))
        rows = g_list_prepend (rows, g_object_ref (orm_result_get_row (self)));

    return g_list_reverse (rows);
}

/**
 * orm_result_get_first:
 * @self: An #OrmResult
 *
 * Reads the first row.
 *
 * Returns: (transfer full) (nullable): The first row, or %NULL if there is none
 */
OrmRow *
orm_result_get_first (OrmResult *self)
{
    g_return_val_if_fail (ORM_IS_RESULT (self), NULL);

    if (!orm_result_next (self))
        return NULL;

    return g_object_ref (orm_result_get_row (self));
}

/**
 * orm_result_get_scalar:
 * @self: An #OrmResult
 *
 * Reads the first column of the first row -- the shape a COUNT(*) or a
 * single-value lookup comes back in.
 *
 * Returns: (transfer full) (nullable): The value, or %NULL if there is none
 */
OrmValue *
orm_result_get_scalar (OrmResult *self)
{
    g_autoptr(OrmRow) row = NULL;
    OrmValue         *value;

    g_return_val_if_fail (ORM_IS_RESULT (self), NULL);

    row = orm_result_get_first (self);
    if (row == NULL)
        return NULL;

    value = orm_row_get_value (row, 0);
    if (value == NULL)
        return NULL;

    return orm_value_copy (value);
}

/**
 * orm_result_close:
 * @self: An #OrmResult
 *
 * Releases the cursor.  Safe to call more than once.
 */
static void
orm_result_close_work (gpointer data)
{
    OrmResult *self = ORM_RESULT (data);

    if (self->driver_result != NULL)
    {
        orm_driver_result_close (self->driver_result);
        g_clear_object (&self->driver_result);

        if (self->connection != NULL)
            orm_connection_untrack_cursor (self->connection, self);
    }
}

void
orm_result_close (OrmResult *self)
{
    g_return_if_fail (ORM_IS_RESULT (self));

    /*
     * Releasing the cursor is backend work like any other -- finalizing
     * a SQLite statement on one thread while the worker steps another on
     * the same database is precisely what the confinement rule exists to
     * prevent.
     */
    /*
     * With no cursor left -- buffered, invalidated by a pool, or closed
     * already -- there is nothing to hand the worker.  Skipping it
     * matters: a result freed after its connection went back to a pool
     * would otherwise queue behind the next borrower's statements.
     */
    if (self->connection != NULL && self->driver_result != NULL)
        orm_connection_run_confined (self->connection, orm_result_close_work, self);
    else
        orm_result_close_work (self);

    g_clear_pointer (&self->rows, orm_result_free_rows);
    self->is_closed = TRUE;
    self->exhausted = TRUE;
}

/*
 * The confined half of orm_result_buffer(): copy the column metadata,
 * read every row, close the cursor.
 */
typedef struct
{
    OrmResult *result;
    GQueue    *rows;
    GArray    *columns;
    GError    *error;
} OrmResultBufferWork;

static void
orm_result_buffer_work (gpointer data)
{
    OrmResultBufferWork *work = (OrmResultBufferWork *) data;
    OrmResult           *self = work->result;
    OrmResultColumn      column;
    OrmRow              *row;
    gint                 count;
    gint                 i;

    if (self->driver_result == NULL)
    {
        g_set_error_literal (&work->error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                             "Result is closed");
        return;
    }

    count = orm_driver_result_get_column_count (self->driver_result);
    work->columns = g_array_sized_new (FALSE, TRUE, sizeof (OrmResultColumn),
                                       (guint) MAX (count, 0));
    g_array_set_clear_func (work->columns, orm_result_column_clear);

    for (i = 0; i < count; i++)
    {
        column.name = g_strdup (orm_driver_result_get_column_name (self->driver_result, i));
        column.type_name = g_strdup (orm_driver_result_get_column_type_name (self->driver_result, i));
        column.value_type = orm_driver_result_get_column_value_type (self->driver_result, i);
        g_array_append_val (work->columns, column);
    }

    work->rows = g_queue_new ();
    while ((row = orm_driver_result_fetch_row (self->driver_result, &work->error)) != NULL)
        g_queue_push_tail (work->rows, row);

    orm_result_close_work (self);
}

/*
 * Internal: reads the whole result into memory and lets go of the
 * connection.  See orm-engine-private.h.
 */
gboolean
orm_result_buffer (OrmResult  *self,
                   GError    **error)
{
    OrmResultBufferWork work;

    g_return_val_if_fail (ORM_IS_RESULT (self), FALSE);
    g_return_val_if_fail (self->rows == NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    work.result = self;
    work.rows = NULL;
    work.columns = NULL;
    work.error = NULL;

    orm_connection_run_confined (self->connection, orm_result_buffer_work, &work);

    if (work.error != NULL)
    {
        g_clear_pointer (&work.rows, orm_result_free_rows);
        g_clear_pointer (&work.columns, g_array_unref);
        self->is_closed = TRUE;
        self->exhausted = TRUE;
        g_propagate_error (error, work.error);
        return FALSE;
    }

    self->rows = work.rows;
    self->columns = work.columns;
    g_clear_object (&self->connection);

    return TRUE;
}
