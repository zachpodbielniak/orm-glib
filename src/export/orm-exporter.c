/* orm-exporter.c
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

#include "orm-exporter.h"
#include "../core/orm-error.h"

/*
 * OrmExporter - the half of an export that is not formatting.
 *
 * A format supplies three hooks; this file supplies the loop they run
 * inside, and the loop is where the bugs live: an export that stops early
 * because a fetch failed, an export that cannot be cancelled, an export
 * that turns a million cells into a million write(2) calls.  Getting
 * those right once, here, is the whole reason the type exists.
 */

/*
 * Big enough that a wide row is still one write to the destination, small
 * enough to be irrelevant next to the rows themselves.
 */
#define ORM_EXPORTER_BUFFER_SIZE (64 * 1024)

typedef struct
{
    /*
     * Pointer-sized rather than guint64 because GLib's lock-free atomics
     * come in exactly two widths, int and pointer, and this counter is
     * read from a thread other than the one exporting -- a UI polling for
     * progress.  Pointer-sized is the widest of the two.
     */
    gsize rows_written;
} OrmExporterPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (OrmExporter, orm_exporter, G_TYPE_OBJECT)

static void
orm_exporter_class_init (OrmExporterClass *klass)
{
}

static void
orm_exporter_init (OrmExporter *self)
{
}

/*
 * Wraps the caller's stream so the format hooks can write a field at a
 * time without that costing a syscall each.
 */
static GOutputStream *
orm_exporter_wrap_stream (GOutputStream *stream)
{
    GOutputStream *buffered;

    buffered = g_buffered_output_stream_new_sized (stream, ORM_EXPORTER_BUFFER_SIZE);

    /*
     * The caller owns @stream and may well write more to it after the
     * export -- a second result, a trailer, a closing tag -- so closing
     * the buffer must not close what it wraps.
     */
    g_filter_output_stream_set_close_base_stream (G_FILTER_OUTPUT_STREAM (buffered), FALSE);

    return buffered;
}

/*
 * Collects the column names into the NULL-terminated array the
 * @write_begin hook takes.  The strings stay owned by @result.
 */
static GPtrArray *
orm_exporter_result_columns (OrmResult *result,
                             gint      *n_columns)
{
    GPtrArray *names;
    gint       count;
    gint       i;

    count = orm_result_get_column_count (result);
    names = g_ptr_array_new ();

    for (i = 0; i < count; i++)
        g_ptr_array_add (names, (gpointer) orm_result_get_column_name (result, i));

    g_ptr_array_add (names, NULL);

    *n_columns = count;
    return names;
}

/*
 * Same, from a row.  An export of a materialized array has no result to
 * ask, so the first row is the only thing that knows the shape.
 */
static GPtrArray *
orm_exporter_row_columns (OrmRow *row,
                          gint   *n_columns)
{
    GPtrArray *names;
    gint       count;
    gint       i;

    count = (row != NULL) ? orm_row_get_column_count (row) : 0;
    names = g_ptr_array_new ();

    for (i = 0; i < count; i++)
        g_ptr_array_add (names, (gpointer) orm_row_get_column_name (row, i));

    g_ptr_array_add (names, NULL);

    *n_columns = count;
    return names;
}

/**
 * orm_exporter_format_double:
 * @value: The number to format
 * @buffer: (out caller-allocates) (array length=buffer_len): Destination
 * @buffer_len: Size of @buffer; %G_ASCII_DTOSTR_BUF_SIZE always suffices
 *
 * Formats @value the way a text export needs it.
 *
 * The locale half is not a nicety.  A plain printf() in a de_DE or fr_FR
 * locale writes 1.5 as "1,5", which in a comma-delimited CSV turns one
 * column into two: a corrupt file produced by a program that looks
 * correct and passes its tests everywhere the developer ran them.  The
 * g_ascii_* formatters are the ones that ignore the locale.
 *
 * The digit count matters too.  g_ascii_dtostr() always asks for 17
 * significant digits, so a price of 0.1 exports as 0.10000000000000001.
 * Trying shorter forms first and keeping the first that reads back as the
 * same double gives "0.1" without ever rounding a value that genuinely
 * needed the precision.
 *
 * Returns: (transfer none): @buffer
 */
gchar *
orm_exporter_format_double (gdouble  value,
                            gchar   *buffer,
                            gsize    buffer_len)
{
    static const gchar * const formats[] = { "%.15g", "%.16g", "%.17g" };
    gsize i;

    g_return_val_if_fail (buffer != NULL, NULL);
    g_return_val_if_fail (buffer_len > 0, NULL);

    for (i = 0; i < G_N_ELEMENTS (formats); i++)
    {
        g_ascii_formatd (buffer, (gint) buffer_len, formats[i], value);

        if (g_ascii_strtod (buffer, NULL) == value)
            break;
    }

    return buffer;
}

/**
 * orm_exporter_export_result:
 * @self: An #OrmExporter
 * @result: The result set to write
 * @stream: (transfer none): Where to write it
 * @cancellable: (nullable): Optional #GCancellable
 * @error: Return location for error
 *
 * Writes every remaining row of @result to @stream in the subclass's
 * format.
 *
 * Returns: %TRUE on success, %FALSE with @error set
 */
gboolean
orm_exporter_export_result (OrmExporter    *self,
                            OrmResult      *result,
                            GOutputStream  *stream,
                            GCancellable   *cancellable,
                            GError        **error)
{
    OrmExporterPrivate      *priv;
    OrmExporterClass        *klass;
    g_autoptr(GOutputStream) buffered = NULL;
    g_autoptr(GPtrArray)     names = NULL;
    const GError            *result_error;
    OrmRow                  *row;
    gint                     n_columns;

    g_return_val_if_fail (ORM_IS_EXPORTER (self), FALSE);
    g_return_val_if_fail (ORM_IS_RESULT (result), FALSE);
    g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    priv = orm_exporter_get_instance_private (self);
    klass = ORM_EXPORTER_GET_CLASS (self);

    g_return_val_if_fail (klass->write_begin != NULL, FALSE);
    g_return_val_if_fail (klass->write_row != NULL, FALSE);
    g_return_val_if_fail (klass->write_end != NULL, FALSE);

    g_atomic_pointer_set (&priv->rows_written, 0);

    if (g_cancellable_set_error_if_cancelled (cancellable, error))
        return FALSE;

    names = orm_exporter_result_columns (result, &n_columns);
    buffered = orm_exporter_wrap_stream (stream);

    if (!klass->write_begin (self, buffered, n_columns,
                             (const gchar * const *) names->pdata, error))
        goto failed;

    while (orm_result_next (result))
    {
        if (g_cancellable_set_error_if_cancelled (cancellable, error))
            goto failed;

        row = orm_result_get_row (result);
        if (row == NULL)
            continue;

        if (!klass->write_row (self, buffered, row, error))
            goto failed;

        g_atomic_pointer_add (&priv->rows_written, 1);
    }

    /*
     * orm_result_next() answers %FALSE for a result that ended and for
     * one that broke, so without this the export of a query that died
     * halfway through is a short file that looks complete -- the one
     * failure an export must never have, because nothing downstream can
     * detect it.
     */
    result_error = orm_result_get_error (result);
    if (result_error != NULL)
    {
        g_propagate_error (error, g_error_copy (result_error));
        goto failed;
    }

    if (!klass->write_end (self, buffered, error))
        goto failed;

    return g_output_stream_close (buffered, cancellable, error);

failed:
    /* The export has already failed; a close error would only mask it. */
    g_output_stream_close (buffered, NULL, NULL);
    return FALSE;
}

/**
 * orm_exporter_export_rows:
 * @self: An #OrmExporter
 * @rows: (element-type OrmRow): The rows to write
 * @stream: (transfer none): Where to write them
 * @cancellable: (nullable): Optional #GCancellable
 * @error: Return location for error
 *
 * Writes an already-materialized set of rows to @stream.
 *
 * The column names come from the first row, which is the only place they
 * exist once a result has been drained into an array.  An empty @rows
 * therefore has no columns to name, and produces just the format's
 * prologue and epilogue -- an empty JSON array, an empty CSV.
 *
 * Returns: %TRUE on success, %FALSE with @error set
 */
gboolean
orm_exporter_export_rows (OrmExporter    *self,
                          GPtrArray      *rows,
                          GOutputStream  *stream,
                          GCancellable   *cancellable,
                          GError        **error)
{
    OrmExporterPrivate      *priv;
    OrmExporterClass        *klass;
    g_autoptr(GOutputStream) buffered = NULL;
    g_autoptr(GPtrArray)     names = NULL;
    OrmRow                  *row;
    gint                     n_columns;
    guint                    i;

    g_return_val_if_fail (ORM_IS_EXPORTER (self), FALSE);
    g_return_val_if_fail (rows != NULL, FALSE);
    g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    priv = orm_exporter_get_instance_private (self);
    klass = ORM_EXPORTER_GET_CLASS (self);

    g_return_val_if_fail (klass->write_begin != NULL, FALSE);
    g_return_val_if_fail (klass->write_row != NULL, FALSE);
    g_return_val_if_fail (klass->write_end != NULL, FALSE);

    g_atomic_pointer_set (&priv->rows_written, 0);

    if (g_cancellable_set_error_if_cancelled (cancellable, error))
        return FALSE;

    names = orm_exporter_row_columns ((rows->len > 0) ? g_ptr_array_index (rows, 0) : NULL,
                                      &n_columns);
    buffered = orm_exporter_wrap_stream (stream);

    if (!klass->write_begin (self, buffered, n_columns,
                             (const gchar * const *) names->pdata, error))
        goto failed;

    for (i = 0; i < rows->len; i++)
    {
        if (g_cancellable_set_error_if_cancelled (cancellable, error))
            goto failed;

        row = g_ptr_array_index (rows, i);
        if (row == NULL)
            continue;

        if (!klass->write_row (self, buffered, row, error))
            goto failed;

        g_atomic_pointer_add (&priv->rows_written, 1);
    }

    if (!klass->write_end (self, buffered, error))
        goto failed;

    return g_output_stream_close (buffered, cancellable, error);

failed:
    g_output_stream_close (buffered, NULL, NULL);
    return FALSE;
}

/**
 * orm_exporter_get_rows_written:
 * @self: An #OrmExporter
 *
 * Gets how many rows have been written.  Safe to call from another
 * thread while an export runs, which is the point: it is what a progress
 * indicator polls.
 *
 * Returns: The row count of the export in progress, or of the last one
 *   to finish
 */
guint64
orm_exporter_get_rows_written (OrmExporter *self)
{
    OrmExporterPrivate *priv;

    g_return_val_if_fail (ORM_IS_EXPORTER (self), 0);

    priv = orm_exporter_get_instance_private (self);

    return (guint64) g_atomic_pointer_get (&priv->rows_written);
}
