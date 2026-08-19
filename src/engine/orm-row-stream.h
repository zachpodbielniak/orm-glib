/* orm-row-stream.h
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

#ifndef ORM_ROW_STREAM_H
#define ORM_ROW_STREAM_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include "orm-row.h"
#include "../core/orm-enums.h"

G_BEGIN_DECLS

#define ORM_TYPE_ROW_STREAM (orm_row_stream_get_type ())

G_DECLARE_FINAL_TYPE (OrmRowStream, orm_row_stream, ORM, ROW_STREAM, GObject)

/*
 * OrmRowStream:
 *
 * Rows arriving a batch at a time, off the calling thread.
 *
 * This is the asynchronous counterpart of #OrmResult, and it is a pull:
 * nothing is fetched until orm_row_stream_fetch_async() asks for it.
 * That is deliberate.  A push API -- a "row" signal, say -- would give
 * the caller no way to say "not so fast", and a slow consumer would
 * either buffer without bound or stall the connection with no way to
 * tell which.  Asking for the next batch when ready is backpressure that
 * needs no protocol.
 *
 * A stream holds its connection busy until it is drained or closed.
 * Every other operation queued on that connection waits behind it, so a
 * stream that is opened and forgotten wedges the connection until it is
 * finalized.  Close it when done reading early.
 *
 * Column metadata is captured when the stream is created, so the
 * accessors are cheap and answer the same thing before the first fetch
 * as after the last.
 */

/*
 * orm_row_stream_get_column_count:
 * @self: An #OrmRowStream
 *
 * Returns: The number of columns
 */
gint orm_row_stream_get_column_count (OrmRowStream *self);

/*
 * orm_row_stream_get_column_name:
 * @self: An #OrmRowStream
 * @index: Zero-based column index
 *
 * Returns: (transfer none) (nullable): The column name
 */
const gchar * orm_row_stream_get_column_name (OrmRowStream *self,
                                              gint          index);

/*
 * orm_row_stream_get_column_type:
 * @self: An #OrmRowStream
 * @index: Zero-based column index
 *
 * Gets the value type a column's values decode to, from the column's
 * declared type rather than from any particular row.
 *
 * Returns: The #OrmValueType, or %ORM_VALUE_NULL when the backend
 *   cannot say
 */
OrmValueType orm_row_stream_get_column_type (OrmRowStream *self,
                                             gint          index);

/*
 * orm_row_stream_get_column_type_name:
 * @self: An #OrmRowStream
 * @index: Zero-based column index
 *
 * Returns: (transfer none) (nullable): The backend's own name for the
 *   column's declared type, or %NULL when it has none to give
 */
const gchar * orm_row_stream_get_column_type_name (OrmRowStream *self,
                                                   gint          index);

/*
 * orm_row_stream_fetch_async:
 * @self: An #OrmRowStream
 * @n_rows: The most rows to fetch
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the batch is ready
 * @user_data: (closure): Data for @callback
 *
 * Fetches up to @n_rows rows on the connection's worker thread.
 */
void orm_row_stream_fetch_async (OrmRowStream        *self,
                                 guint                n_rows,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data);

/*
 * orm_row_stream_fetch_finish:
 * @self: An #OrmRowStream
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_row_stream_fetch_async().
 *
 * An empty array is the end of the results, never %NULL: %NULL means the
 * fetch failed and @error says why.  Conflating the two is how a caller
 * ends up treating a broken query as an empty table.
 *
 * Returns: (transfer full) (element-type OrmRow) (nullable): The rows, at
 *   most @n_rows of them, or %NULL on error
 */
GPtrArray * orm_row_stream_fetch_finish (OrmRowStream  *self,
                                         GAsyncResult  *result,
                                         GError       **error);

/*
 * orm_row_stream_is_at_end:
 * @self: An #OrmRowStream
 *
 * Checks whether the rows are exhausted.  True once a fetch has come
 * back with fewer rows than it was asked for.
 *
 * Returns: %TRUE if there are no more rows
 */
gboolean orm_row_stream_is_at_end (OrmRowStream *self);

/*
 * orm_row_stream_close_async:
 * @self: An #OrmRowStream
 * @cancellable: (nullable): A #GCancellable
 * @callback: (scope async): Called when the cursor is released
 * @user_data: (closure): Data for @callback
 *
 * Releases the backend cursor, freeing the connection for other work.
 * Safe to call on a stream that is already closed or drained.
 */
void orm_row_stream_close_async (OrmRowStream        *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data);

/*
 * orm_row_stream_close_finish:
 * @self: An #OrmRowStream
 * @result: The #GAsyncResult
 * @error: Return location for error
 *
 * Finishes orm_row_stream_close_async().
 *
 * Returns: %TRUE on success
 */
gboolean orm_row_stream_close_finish (OrmRowStream  *self,
                                      GAsyncResult  *result,
                                      GError       **error);

G_END_DECLS

#endif /* ORM_ROW_STREAM_H */
