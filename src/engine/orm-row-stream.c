/* orm-row-stream.c
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

#include "orm-row-stream.h"
#include "orm-connection.h"
#include "orm-engine-private.h"
#include "../core/orm-error.h"
#include "../driver/orm-driver-result.h"

/*
 * OrmRowStream - rows a batch at a time, fetched off the caller's thread.
 *
 * The cursor underneath is the same OrmDriverResult that OrmResult wraps;
 * the difference is where it is stepped. OrmResult steps it wherever the
 * caller happens to be, which for SQLite means the caller waits for the
 * database on its own thread. A stream steps it on the connection's
 * worker and hands back a GPtrArray when the batch is ready.
 *
 * Column metadata is snapshotted at construction, on the worker, for two
 * reasons: it cannot change for the life of a result set, and reading it
 * from the backend later would mean reaching into a handle another
 * thread may be stepping.
 *
 * The connection is referenced, not borrowed. A stream that outlives its
 * connection would be stepping a statement whose database has been
 * closed, and the pull model makes that easy to arrange by accident --
 * the caller who drops the connection between two fetches is not doing
 * anything obviously wrong.
 */

typedef struct
{
    gchar        *name;
    gchar        *type_name;
    OrmValueType  value_type;
} OrmRowStreamColumn;

struct _OrmRowStream
{
    GObject parent_instance;

    OrmConnection   *connection;    /* Owned */
    OrmDriverResult *driver_result;

    GArray          *columns;
    gboolean         at_end;
    gboolean         is_closed;
};

G_DEFINE_FINAL_TYPE (OrmRowStream, orm_row_stream, G_TYPE_OBJECT)

static void
orm_row_stream_column_clear (gpointer data)
{
    OrmRowStreamColumn *column = (OrmRowStreamColumn *) data;

    g_free (column->name);
    g_free (column->type_name);
}

/*
 * Releases the backend cursor.  Runs wherever the connection's work runs.
 */
static void
orm_row_stream_close_cursor (gpointer data)
{
    OrmRowStream *self = ORM_ROW_STREAM (data);

    if (self->driver_result != NULL)
    {
        orm_driver_result_close (self->driver_result);
        g_clear_object (&self->driver_result);
    }

    self->is_closed = TRUE;
    self->at_end = TRUE;
}

static void
orm_row_stream_finalize (GObject *object)
{
    OrmRowStream *self = ORM_ROW_STREAM (object);

    /*
     * Through the connection rather than directly: a stream dropped
     * while the worker is busy would otherwise finalize a statement the
     * worker is in the middle of stepping.
     */
    if (self->connection != NULL)
        orm_connection_run_confined (self->connection,
                                     orm_row_stream_close_cursor, self);
    else
        orm_row_stream_close_cursor (self);

    g_clear_pointer (&self->columns, g_array_unref);
    g_clear_object (&self->connection);

    G_OBJECT_CLASS (orm_row_stream_parent_class)->finalize (object);
}

static void
orm_row_stream_class_init (OrmRowStreamClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_row_stream_finalize;
}

static void
orm_row_stream_init (OrmRowStream *self)
{
    self->connection = NULL;
    self->driver_result = NULL;
    self->columns = NULL;
    self->at_end = FALSE;
    self->is_closed = FALSE;
}

/*
 * Internal: wraps a driver result as a stream, capturing its column
 * metadata.  Called on the connection's worker thread, by the query job
 * that produced @driver_result.
 *
 * Returns: (transfer full): A new #OrmRowStream taking ownership of
 *   @driver_result
 */
OrmRowStream *
orm_row_stream_new_for_driver (OrmConnection   *connection,
                               OrmDriverResult *driver_result)
{
    OrmRowStream *self;
    gint          count;
    gint          i;

    g_return_val_if_fail (ORM_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (ORM_IS_DRIVER_RESULT (driver_result), NULL);

    self = g_object_new (ORM_TYPE_ROW_STREAM, NULL);
    self->connection = g_object_ref (connection);
    self->driver_result = driver_result;

    count = orm_driver_result_get_column_count (driver_result);

    self->columns = g_array_sized_new (FALSE, TRUE, sizeof (OrmRowStreamColumn),
                                       count > 0 ? count : 1);
    g_array_set_clear_func (self->columns, orm_row_stream_column_clear);

    for (i = 0; i < count; i++)
    {
        OrmRowStreamColumn column;

        column.name = g_strdup (orm_driver_result_get_column_name (driver_result, i));
        column.type_name = g_strdup (orm_driver_result_get_column_type_name (driver_result, i));
        column.value_type = orm_driver_result_get_column_value_type (driver_result, i);

        g_array_append_val (self->columns, column);
    }

    return self;
}

/**
 * orm_row_stream_get_column_count:
 * @self: An #OrmRowStream
 *
 * Returns: The number of columns
 */
gint
orm_row_stream_get_column_count (OrmRowStream *self)
{
    g_return_val_if_fail (ORM_IS_ROW_STREAM (self), 0);

    return (gint) self->columns->len;
}

/**
 * orm_row_stream_get_column_name:
 * @self: An #OrmRowStream
 * @index: Zero-based column index
 *
 * Returns: (transfer none) (nullable): The column name
 */
const gchar *
orm_row_stream_get_column_name (OrmRowStream *self,
                                gint          index)
{
    g_return_val_if_fail (ORM_IS_ROW_STREAM (self), NULL);

    if (index < 0 || (guint) index >= self->columns->len)
        return NULL;

    return g_array_index (self->columns, OrmRowStreamColumn, index).name;
}

/**
 * orm_row_stream_get_column_type:
 * @self: An #OrmRowStream
 * @index: Zero-based column index
 *
 * Returns: The #OrmValueType the column's values decode to
 */
OrmValueType
orm_row_stream_get_column_type (OrmRowStream *self,
                                gint          index)
{
    g_return_val_if_fail (ORM_IS_ROW_STREAM (self), ORM_VALUE_NULL);

    if (index < 0 || (guint) index >= self->columns->len)
        return ORM_VALUE_NULL;

    return g_array_index (self->columns, OrmRowStreamColumn, index).value_type;
}

/**
 * orm_row_stream_get_column_type_name:
 * @self: An #OrmRowStream
 * @index: Zero-based column index
 *
 * Returns: (transfer none) (nullable): The backend's own name for the
 *   column's declared type, or %NULL
 */
const gchar *
orm_row_stream_get_column_type_name (OrmRowStream *self,
                                     gint          index)
{
    g_return_val_if_fail (ORM_IS_ROW_STREAM (self), NULL);

    if (index < 0 || (guint) index >= self->columns->len)
        return NULL;

    return g_array_index (self->columns, OrmRowStreamColumn, index).type_name;
}

/**
 * orm_row_stream_is_at_end:
 * @self: An #OrmRowStream
 *
 * Returns: %TRUE when the rows are exhausted
 */
gboolean
orm_row_stream_is_at_end (OrmRowStream *self)
{
    g_return_val_if_fail (ORM_IS_ROW_STREAM (self), TRUE);

    return self->at_end;
}

/*
 * Fetches one batch on the worker thread.
 */
static void
orm_row_stream_fetch_job (gpointer  data,
                          GTask    *task)
{
    OrmRowStream *self = ORM_ROW_STREAM (g_task_get_source_object (task));
    guint         n_rows = GPOINTER_TO_UINT (data);
    GPtrArray    *rows;
    GError       *error = NULL;
    guint         i;

    if (!orm_connection_task_may_run (task))
        return;

    rows = g_ptr_array_new_with_free_func (g_object_unref);

    for (i = 0; i < n_rows && !self->at_end && self->driver_result != NULL; i++)
    {
        OrmRow *row = orm_driver_result_fetch_row (self->driver_result, &error);

        if (row == NULL)
        {
            /*
             * A NULL row with no error is the end of the results, which
             * is not a failure -- the caller learns it from a batch that
             * came back short.
             */
            self->at_end = TRUE;
            break;
        }

        g_ptr_array_add (rows, row);
    }

    if (!orm_connection_task_may_return (task))
    {
        g_ptr_array_unref (rows);
        g_clear_error (&error);
        return;
    }

    if (error != NULL)
    {
        g_ptr_array_unref (rows);
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    g_task_return_pointer (task, rows, (GDestroyNotify) g_ptr_array_unref);
}

/**
 * orm_row_stream_fetch_async:
 * @self: An #OrmRowStream
 * @n_rows: The most rows to fetch
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the batch is ready
 * @user_data: (closure): Data for @callback
 *
 * Fetches up to @n_rows rows on the connection's worker thread.
 */
void
orm_row_stream_fetch_async (OrmRowStream        *self,
                            guint                n_rows,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (ORM_IS_ROW_STREAM (self));
    g_return_if_fail (n_rows > 0);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_row_stream_fetch_async);

    if (self->is_closed)
    {
        g_task_return_new_error (task, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                                 "The row stream is closed");
        return;
    }

    orm_connection_submit_async (self->connection, task,
                                 orm_row_stream_fetch_job,
                                 GUINT_TO_POINTER (n_rows), NULL);
}

/**
 * orm_row_stream_fetch_finish:
 * @self: An #OrmRowStream
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_row_stream_fetch_async().
 *
 * Returns: (transfer full) (element-type OrmRow) (nullable): The rows, or
 *   %NULL on error.  An empty array means the results are exhausted.
 */
GPtrArray *
orm_row_stream_fetch_finish (OrmRowStream  *self,
                             GAsyncResult  *result,
                             GError       **error)
{
    g_return_val_if_fail (ORM_IS_ROW_STREAM (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return (GPtrArray *) g_task_propagate_pointer (G_TASK (result), error);
}

static void
orm_row_stream_close_job (gpointer  data,
                          GTask    *task)
{
    OrmRowStream *self = ORM_ROW_STREAM (g_task_get_source_object (task));

    if (!orm_connection_task_may_run (task))
        return;

    orm_row_stream_close_cursor (self);

    if (!orm_connection_task_may_return (task))
        return;

    g_task_return_boolean (task, TRUE);
}

/**
 * orm_row_stream_close_async:
 * @self: An #OrmRowStream
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async) (nullable): Called when the cursor is released
 * @user_data: (closure): Data for @callback
 *
 * Releases the backend cursor, freeing the connection for other work.
 */
void
orm_row_stream_close_async (OrmRowStream        *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (ORM_IS_ROW_STREAM (self));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, orm_row_stream_close_async);

    orm_connection_submit_async (self->connection, task,
                                 orm_row_stream_close_job, NULL, NULL);
}

/**
 * orm_row_stream_close_finish:
 * @self: An #OrmRowStream
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_row_stream_close_async().
 *
 * Returns: %TRUE on success
 */
gboolean
orm_row_stream_close_finish (OrmRowStream  *self,
                             GAsyncResult  *result,
                             GError       **error)
{
    g_return_val_if_fail (ORM_IS_ROW_STREAM (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}
