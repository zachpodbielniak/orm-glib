/* orm-exporter.h
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

#ifndef ORM_EXPORTER_H
#define ORM_EXPORTER_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include "../engine/orm-result.h"
#include "../engine/orm-row.h"

G_BEGIN_DECLS

#define ORM_TYPE_EXPORTER (orm_exporter_get_type ())

G_DECLARE_DERIVABLE_TYPE (OrmExporter, orm_exporter, ORM, EXPORTER, GObject)

/*
 * OrmExporterClass:
 * @write_begin: Emit whatever precedes the rows -- a CSV header, a JSON
 *   opening bracket.  Called once, before any row.
 * @write_row: Emit one row.
 * @write_end: Emit whatever follows the rows.  Called once, and only if
 *   @write_begin succeeded.
 *
 * Writes a result set out in some text format.
 *
 * The three hooks are the only thing a format has to supply, because
 * everything else about an export is the same whatever the format:
 * iterating the result, telling end-of-rows apart from a failed fetch,
 * honouring a #GCancellable between rows, buffering so that a per-value
 * write is not a syscall per cell, and counting what has been written so
 * a UI can show progress.  The base class owns all of that and calls down
 * only to format a header, a row, and a trailer.
 *
 * That is why this is an abstract class and not an interface.  An
 * interface can only declare the hooks; the loop, the cancellation check
 * and the buffering would then have to be written again in every
 * implementation, and the second implementation is where they start to
 * disagree -- one exporter checks the result's error, the next forgets
 * and silently writes a truncated file.  Here there is one loop, and a
 * format cannot get it wrong because it never sees it.
 */
struct _OrmExporterClass
{
    GObjectClass parent_class;

    gboolean (*write_begin) (OrmExporter          *self,
                             GOutputStream        *stream,
                             gint                  n_columns,
                             const gchar * const  *column_names,
                             GError              **error);
    gboolean (*write_row)   (OrmExporter          *self,
                             GOutputStream        *stream,
                             OrmRow               *row,
                             GError              **error);
    gboolean (*write_end)   (OrmExporter          *self,
                             GOutputStream        *stream,
                             GError              **error);

    /*< private >*/
    gpointer _reserved[8];
};

/*
 * orm_exporter_export_result:
 * @self: An #OrmExporter
 * @result: The result set to write
 * @stream: (transfer none): Where to write it
 * @cancellable: (nullable): Optional #GCancellable
 * @error: Return location for error
 *
 * Writes every remaining row of @result to @stream.
 *
 * Returns: %TRUE on success, %FALSE with @error set
 */
gboolean orm_exporter_export_result (OrmExporter    *self,
                                     OrmResult      *result,
                                     GOutputStream  *stream,
                                     GCancellable   *cancellable,
                                     GError        **error);

/*
 * orm_exporter_export_rows:
 * @self: An #OrmExporter
 * @rows: (element-type OrmRow): The rows to write
 * @stream: (transfer none): Where to write them
 * @cancellable: (nullable): Optional #GCancellable
 * @error: Return location for error
 *
 * Writes an already-materialized set of rows to @stream.
 *
 * Returns: %TRUE on success, %FALSE with @error set
 */
gboolean orm_exporter_export_rows (OrmExporter    *self,
                                   GPtrArray      *rows,
                                   GOutputStream  *stream,
                                   GCancellable   *cancellable,
                                   GError        **error);

/*
 * orm_exporter_get_rows_written:
 * @self: An #OrmExporter
 *
 * Returns: The number of rows written by the export in progress, or by
 *   the last one to finish
 */
guint64 orm_exporter_get_rows_written (OrmExporter *self);

/*
 * orm_exporter_format_double:
 * @value: The number to format
 * @buffer: (out caller-allocates) (array length=buffer_len): Destination
 * @buffer_len: Size of @buffer; %G_ASCII_DTOSTR_BUF_SIZE always suffices
 *
 * Formats @value for a text export: '.' as the decimal point whatever the
 * locale says, in the fewest digits that still read back as the same
 * double.  Subclasses use it so every format agrees on what a number
 * looks like.
 *
 * Returns: (transfer none): @buffer
 */
gchar * orm_exporter_format_double (gdouble  value,
                                    gchar   *buffer,
                                    gsize    buffer_len);

G_END_DECLS

#endif /* ORM_EXPORTER_H */
