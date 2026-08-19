/* test-export.c
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

/*
 * The CSV and JSON exporters, asserted on exact bytes.
 *
 * Byte-exact is the only useful assertion here. An export is read by
 * something that is not this program -- a spreadsheet, a JSON parser, a
 * shell pipeline -- and every interesting failure is a file that still
 * looks plausible: a quote that ends a field early, a decimal comma that
 * splits a column in two, a control character that makes a parser stop.
 * Checking that the output "contains Alice" would pass on all of them.
 */

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <locale.h>

#include "test-fixtures.h"

/* ---------------------------------------------------------------- */
/* Helpers                                                           */
/* ---------------------------------------------------------------- */

/*
 * Builds a row directly, which is far less machinery than a query for
 * the formatting cases -- most of them are about a value a real table
 * would make awkward to produce on demand.
 *
 * Takes ownership of @values, matching orm_row_new().
 */
static OrmRow *
test_row_new (const gchar * const *names,
              OrmValue           **values,
              guint                n_columns)
{
    GPtrArray *name_array;
    GPtrArray *value_array;
    guint      i;

    name_array = g_ptr_array_new_with_free_func (g_free);
    value_array = g_ptr_array_new_with_free_func ((GDestroyNotify) orm_value_free);

    for (i = 0; i < n_columns; i++)
    {
        g_ptr_array_add (name_array, g_strdup (names[i]));
        g_ptr_array_add (value_array, values[i]);
    }

    return orm_row_new (name_array, value_array);
}

static GPtrArray *
test_rows_new (void)
{
    return g_ptr_array_new_with_free_func (g_object_unref);
}

/*
 * A single-row array of one string column, which is the shape almost
 * every escaping test wants.
 */
static GPtrArray *
test_rows_one_string (const gchar *column,
                      const gchar *value)
{
    const gchar *names[1];
    OrmValue    *values[1];
    GPtrArray   *rows;

    names[0] = column;
    values[0] = orm_value_new_string (value);

    rows = test_rows_new ();
    g_ptr_array_add (rows, test_row_new (names, values, 1));

    return rows;
}

/*
 * Reads back everything the exporter wrote.  A GMemoryOutputStream is
 * what makes byte-exact assertions possible without a temporary file.
 */
static gchar *
test_stream_contents (GOutputStream *stream)
{
    GMemoryOutputStream *memory = G_MEMORY_OUTPUT_STREAM (stream);
    gconstpointer        data;
    gsize                size;

    data = g_memory_output_stream_get_data (memory);
    size = g_memory_output_stream_get_data_size (memory);

    if (data == NULL)
        return g_strdup ("");

    return g_strndup (data, size);
}

static gchar *
test_export_rows (OrmExporter *exporter,
                  GPtrArray   *rows)
{
    g_autoptr(GOutputStream) stream = NULL;
    g_autoptr(GError)        error = NULL;

    stream = g_memory_output_stream_new_resizable ();

    g_assert_true (orm_exporter_export_rows (exporter, rows, stream, NULL, &error));
    g_assert_no_error (error);

    return test_stream_contents (stream);
}

/* ---------------------------------------------------------------- */
/* CSV                                                               */
/* ---------------------------------------------------------------- */

static GPtrArray *
test_csv_sample_rows (void)
{
    const gchar *names[2];
    OrmValue    *values[2];
    GPtrArray   *rows;

    names[0] = "id";
    names[1] = "name";

    rows = test_rows_new ();

    values[0] = orm_value_new_integer (1);
    values[1] = orm_value_new_string ("Alice");
    g_ptr_array_add (rows, test_row_new (names, values, 2));

    values[0] = orm_value_new_integer (2);
    values[1] = orm_value_new_string ("Bob");
    g_ptr_array_add (rows, test_row_new (names, values, 2));

    return rows;
}

static void
test_csv_header (void)
{
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_csv_sample_rows ();
    g_autofree gchar         *out = NULL;

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "id,name\n1,Alice\n2,Bob\n");
}

static void
test_csv_no_header (void)
{
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_csv_sample_rows ();
    g_autofree gchar         *out = NULL;

    g_object_set (exporter, "include-header", FALSE, NULL);
    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "1,Alice\n2,Bob\n");
}

/*
 * The delimiter inside a field is the failure everyone has seen: left
 * bare it silently turns one column into two.
 */
static void
test_csv_quote_delimiter (void)
{
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_rows_one_string ("name", "Smith, John");
    g_autofree gchar         *out = NULL;

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "name\n\"Smith, John\"\n");
}

/*
 * CSV has no backslash: a quote inside a quoted field is written twice.
 */
static void
test_csv_quote_doubled (void)
{
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_rows_one_string ("quip", "He said \"hi\"");
    g_autofree gchar         *out = NULL;

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "quip\n\"He said \"\"hi\"\"\"\n");
}

static void
test_csv_quote_newline (void)
{
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_rows_one_string ("note", "line one\nline two");
    g_autofree gchar         *out = NULL;

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "note\n\"line one\nline two\"\n");
}

static void
test_csv_null_default (void)
{
    const gchar *names[2];
    OrmValue    *values[2];
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_rows_new ();
    g_autofree gchar         *out = NULL;

    names[0] = "id";
    names[1] = "name";
    values[0] = orm_value_new_integer (7);
    values[1] = orm_value_new_null ();
    g_ptr_array_add (rows, test_row_new (names, values, 2));

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "id,name\n7,\n");
}

/*
 * A sentinel is the only way a CSV distinguishes NULL from an empty
 * string, so it must go out unquoted -- quoted, it is just a string.
 */
static void
test_csv_null_custom (void)
{
    const gchar *names[2];
    OrmValue    *values[2];
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_rows_new ();
    g_autofree gchar         *out = NULL;

    names[0] = "id";
    names[1] = "name";
    values[0] = orm_value_new_integer (7);
    values[1] = orm_value_new_null ();
    g_ptr_array_add (rows, test_row_new (names, values, 2));

    g_object_set (exporter, "null-string", "\\N", NULL);
    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "id,name\n7,\\N\n");
}

static void
test_csv_force_quotes (void)
{
    const gchar *names[2];
    OrmValue    *values[2];
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_rows_new ();
    g_autofree gchar         *out = NULL;

    names[0] = "id";
    names[1] = "name";
    values[0] = orm_value_new_integer (1);
    values[1] = orm_value_new_null ();
    g_ptr_array_add (rows, test_row_new (names, values, 2));

    g_object_set (exporter, "force-quotes", TRUE, NULL);
    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    /* Header and NULL included: force means force. */
    g_assert_cmpstr (out, ==, "\"id\",\"name\"\n\"1\",\"\"\n");
}

static void
test_csv_custom_delimiter (void)
{
    const gchar *names[2];
    OrmValue    *values[2];
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_rows_new ();
    g_autofree gchar         *out = NULL;

    names[0] = "id";
    names[1] = "name";
    values[0] = orm_value_new_integer (1);
    values[1] = orm_value_new_string ("a,b");
    g_ptr_array_add (rows, test_row_new (names, values, 2));

    g_object_set (exporter,
                  "delimiter", "\t",
                  "line-ending", "\r\n",
                  NULL);
    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    /* A comma is no longer special once the delimiter is a tab. */
    g_assert_cmpstr (out, ==, "id\tname\r\n1\ta,b\r\n");
}

static void
test_csv_blob_base64 (void)
{
    const gchar *names[1];
    OrmValue    *values[1];
    g_autoptr(GBytes)         bytes = g_bytes_new ("hi", 2);
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_rows_new ();
    g_autofree gchar         *out = NULL;

    names[0] = "payload";
    values[0] = orm_value_new_blob (bytes);
    g_ptr_array_add (rows, test_row_new (names, values, 1));

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "payload\naGk=\n");
}

static void
test_csv_datetime_iso8601 (void)
{
    const gchar *names[1];
    OrmValue    *values[1];
    g_autoptr(GDateTime)      when = g_date_time_new_utc (2025, 3, 4, 5, 6, 7);
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_rows_new ();
    g_autofree gchar         *out = NULL;

    names[0] = "created";
    values[0] = orm_value_new_datetime (when);
    g_ptr_array_add (rows, test_row_new (names, values, 1));

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "created\n2025-03-04T05:06:07Z\n");
}

/*
 * A float must not pick up the locale's decimal comma: in a comma
 * delimited file that is a column boundary.
 */
static void
test_csv_float_decimal_point (void)
{
    const gchar *names[1];
    OrmValue    *values[1];
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_rows_new ();
    g_autofree gchar         *out = NULL;

    names[0] = "price";
    values[0] = orm_value_new_float (0.1);
    g_ptr_array_add (rows, test_row_new (names, values, 1));

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "price\n0.1\n");
}

/*
 * The same float under a locale that writes 1,5 for one and a half.  A
 * plain printf() here emits "0,1", which a comma-delimited reader takes
 * as two fields -- a corrupt file from a program that passed every test
 * run in an English locale.
 */
static void
test_csv_locale_decimal_point (void)
{
    static const gchar * const candidates[] = { "de_DE.UTF-8", "de_DE.utf8", "de_DE" };
    const gchar *names[1];
    OrmValue    *values[1];
    g_autoptr(OrmCsvExporter) exporter = NULL;
    g_autoptr(GPtrArray)      rows = NULL;
    g_autofree gchar         *saved = NULL;
    g_autofree gchar         *out = NULL;
    const gchar              *applied = NULL;
    gsize                     i;

    saved = g_strdup (setlocale (LC_NUMERIC, NULL));

    for (i = 0; i < G_N_ELEMENTS (candidates) && applied == NULL; i++)
        applied = setlocale (LC_NUMERIC, candidates[i]);

    if (applied == NULL)
    {
        g_test_skip ("no comma-decimal locale installed");
        return;
    }

    exporter = orm_csv_exporter_new ();
    rows = test_rows_new ();
    names[0] = "price";
    values[0] = orm_value_new_float (0.1);
    g_ptr_array_add (rows, test_row_new (names, values, 1));

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    /* Restore before asserting, so a failure does not leak the locale. */
    setlocale (LC_NUMERIC, (saved != NULL) ? saved : "C");

    g_assert_cmpstr (out, ==, "price\n0.1\n");
}

static void
test_csv_no_rows (void)
{
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_rows_new ();
    g_autofree gchar         *out = NULL;

    /* No rows means no columns to name either, so the file is empty. */
    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "");
}

/* ---------------------------------------------------------------- */
/* JSON                                                              */
/* ---------------------------------------------------------------- */

static void
test_json_array_of_objects (void)
{
    g_autoptr(OrmJsonExporter) exporter = orm_json_exporter_new ();
    g_autoptr(GPtrArray)       rows = test_csv_sample_rows ();
    g_autofree gchar          *out = NULL;

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==,
                     "[{\"id\":1,\"name\":\"Alice\"},{\"id\":2,\"name\":\"Bob\"}]");
}

static void
test_json_array_of_arrays (void)
{
    g_autoptr(OrmJsonExporter) exporter = orm_json_exporter_new ();
    g_autoptr(GPtrArray)       rows = test_csv_sample_rows ();
    g_autofree gchar          *out = NULL;

    g_object_set (exporter, "layout", ORM_JSON_LAYOUT_ARRAY_OF_ARRAYS, NULL);
    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    /* Positional rows are meaningless without the column list. */
    g_assert_cmpstr (out, ==,
                     "{\"columns\":[\"id\",\"name\"],"
                     "\"rows\":[[1,\"Alice\"],[2,\"Bob\"]]}");
}

static void
test_json_array_of_arrays_bare (void)
{
    g_autoptr(OrmJsonExporter) exporter = orm_json_exporter_new ();
    g_autoptr(GPtrArray)       rows = test_csv_sample_rows ();
    g_autofree gchar          *out = NULL;

    g_object_set (exporter,
                  "layout", ORM_JSON_LAYOUT_ARRAY_OF_ARRAYS,
                  "include-columns", FALSE,
                  NULL);
    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "[[1,\"Alice\"],[2,\"Bob\"]]");
}

static void
test_json_pretty (void)
{
    g_autoptr(OrmJsonExporter) exporter = orm_json_exporter_new ();
    g_autoptr(GPtrArray)       rows = test_csv_sample_rows ();
    g_autofree gchar          *out = NULL;

    g_object_set (exporter, "pretty", TRUE, NULL);
    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==,
                     "[\n"
                     "  {\n"
                     "    \"id\": 1,\n"
                     "    \"name\": \"Alice\"\n"
                     "  },\n"
                     "  {\n"
                     "    \"id\": 2,\n"
                     "    \"name\": \"Bob\"\n"
                     "  }\n"
                     "]");
}

static void
test_json_pretty_wrapped (void)
{
    g_autoptr(OrmJsonExporter) exporter = orm_json_exporter_new ();
    g_autoptr(GPtrArray)       rows = test_rows_one_string ("name", "Alice");
    g_autofree gchar          *out = NULL;

    g_object_set (exporter,
                  "layout", ORM_JSON_LAYOUT_ARRAY_OF_ARRAYS,
                  "pretty", TRUE,
                  "indent", 4,
                  NULL);
    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==,
                     "{\n"
                     "    \"columns\": [\n"
                     "        \"name\"\n"
                     "    ],\n"
                     "    \"rows\": [\n"
                     "        [\n"
                     "            \"Alice\"\n"
                     "        ]\n"
                     "    ]\n"
                     "}");
}

/*
 * The escaping is the whole correctness of a hand-rolled emitter.  A raw
 * control byte -- which a text column can absolutely hold -- makes the
 * document unparseable, and it is invisible in any assertion that only
 * looks for the surrounding text.
 */
static void
test_json_string_escaping (void)
{
    g_autoptr(OrmJsonExporter) exporter = orm_json_exporter_new ();
    g_autoptr(GPtrArray)       rows = NULL;
    g_autofree gchar          *out = NULL;

    rows = test_rows_one_string ("s", "q\"b\\s\x01n\nt\tr\rf\fb\b");
    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==,
                     "[{\"s\":\"q\\\"b\\\\s\\u0001n\\nt\\tr\\rf\\fb\\b\"}]");
}

static void
test_json_null (void)
{
    const gchar *names[2];
    OrmValue    *values[2];
    g_autoptr(OrmJsonExporter) exporter = orm_json_exporter_new ();
    g_autoptr(GPtrArray)       rows = test_rows_new ();
    g_autofree gchar          *out = NULL;

    names[0] = "id";
    names[1] = "name";
    values[0] = orm_value_new_integer (1);
    values[1] = orm_value_new_null ();
    g_ptr_array_add (rows, test_row_new (names, values, 2));

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "[{\"id\":1,\"name\":null}]");
}

/*
 * NaN and the infinities have no JSON spelling.  Emitting the C library's
 * tokens for them produces a file no conforming parser will read.
 */
static void
test_json_non_finite (void)
{
    const gchar *names[3];
    OrmValue    *values[3];
    g_autoptr(OrmJsonExporter) exporter = orm_json_exporter_new ();
    g_autoptr(GPtrArray)       rows = test_rows_new ();
    g_autofree gchar          *out = NULL;

    names[0] = "nan";
    names[1] = "inf";
    names[2] = "ninf";
    values[0] = orm_value_new_float (g_ascii_strtod ("nan", NULL));
    values[1] = orm_value_new_float (g_ascii_strtod ("inf", NULL));
    values[2] = orm_value_new_float (g_ascii_strtod ("-inf", NULL));
    g_ptr_array_add (rows, test_row_new (names, values, 3));

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "[{\"nan\":null,\"inf\":null,\"ninf\":null}]");
}

static void
test_json_finite_float (void)
{
    const gchar *names[1];
    OrmValue    *values[1];
    g_autoptr(OrmJsonExporter) exporter = orm_json_exporter_new ();
    g_autoptr(GPtrArray)       rows = test_rows_new ();
    g_autofree gchar          *out = NULL;

    names[0] = "price";
    values[0] = orm_value_new_float (0.1);
    g_ptr_array_add (rows, test_row_new (names, values, 1));

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "[{\"price\":0.1}]");
}

static void
test_json_blob_and_datetime (void)
{
    const gchar *names[2];
    OrmValue    *values[2];
    g_autoptr(GBytes)          bytes = g_bytes_new ("hi", 2);
    g_autoptr(GDateTime)       when = g_date_time_new_utc (2025, 3, 4, 5, 6, 7);
    g_autoptr(OrmJsonExporter) exporter = orm_json_exporter_new ();
    g_autoptr(GPtrArray)       rows = test_rows_new ();
    g_autofree gchar          *out = NULL;

    names[0] = "payload";
    names[1] = "created";
    values[0] = orm_value_new_blob (bytes);
    values[1] = orm_value_new_datetime (when);
    g_ptr_array_add (rows, test_row_new (names, values, 2));

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==,
                     "[{\"payload\":\"aGk=\",\"created\":\"2025-03-04T05:06:07Z\"}]");
}

static void
test_json_no_rows (void)
{
    g_autoptr(OrmJsonExporter) exporter = orm_json_exporter_new ();
    g_autoptr(GPtrArray)       rows = test_rows_new ();
    g_autofree gchar          *out = NULL;

    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "[]");
}

static void
test_json_no_rows_wrapped (void)
{
    g_autoptr(OrmJsonExporter) exporter = orm_json_exporter_new ();
    g_autoptr(GPtrArray)       rows = test_rows_new ();
    g_autofree gchar          *out = NULL;

    g_object_set (exporter, "layout", ORM_JSON_LAYOUT_ARRAY_OF_ARRAYS, NULL);
    out = test_export_rows (ORM_EXPORTER (exporter), rows);

    g_assert_cmpstr (out, ==, "{\"columns\":[],\"rows\":[]}");
}

/* ---------------------------------------------------------------- */
/* Base class behaviour                                              */
/* ---------------------------------------------------------------- */

static void
test_export_cancelled (void)
{
    g_autoptr(OrmCsvExporter)  csv = orm_csv_exporter_new ();
    g_autoptr(OrmJsonExporter) json = orm_json_exporter_new ();
    g_autoptr(GCancellable)    cancellable = g_cancellable_new ();
    g_autoptr(GOutputStream)   stream = g_memory_output_stream_new_resizable ();
    g_autoptr(GPtrArray)       rows = test_csv_sample_rows ();
    g_autoptr(GError)          error = NULL;
    g_autofree gchar          *out = NULL;

    g_cancellable_cancel (cancellable);

    g_assert_false (orm_exporter_export_rows (ORM_EXPORTER (csv), rows, stream,
                                              cancellable, &error));
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_clear_error (&error);

    g_assert_false (orm_exporter_export_rows (ORM_EXPORTER (json), rows, stream,
                                              cancellable, &error));
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

    /* A cancelled export must not have left a half-written document. */
    out = test_stream_contents (stream);
    g_assert_cmpstr (out, ==, "");
}

static void
test_export_rows_written (void)
{
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GPtrArray)      rows = test_csv_sample_rows ();
    g_autoptr(GPtrArray)      empty = test_rows_new ();
    g_autofree gchar         *out = NULL;

    g_assert_cmpuint (orm_exporter_get_rows_written (ORM_EXPORTER (exporter)), ==, 0);

    out = test_export_rows (ORM_EXPORTER (exporter), rows);
    g_assert_cmpuint (orm_exporter_get_rows_written (ORM_EXPORTER (exporter)), ==, 2);

    /* Each export starts the count again rather than accumulating. */
    g_free (test_export_rows (ORM_EXPORTER (exporter), empty));
    g_assert_cmpuint (orm_exporter_get_rows_written (ORM_EXPORTER (exporter)), ==, 0);
}

/* ---------------------------------------------------------------- */
/* End to end, over a real query                                     */
/* ---------------------------------------------------------------- */

static gchar *
test_export_query (TestDbFixture *fixture,
                   OrmExporter   *exporter,
                   const gchar   *sql)
{
    g_autoptr(OrmResult)     result = NULL;
    g_autoptr(GOutputStream) stream = NULL;
    g_autoptr(GError)        error = NULL;

    result = orm_connection_query (fixture->connection, sql, &error);
    g_assert_no_error (error);
    g_assert_nonnull (result);

    stream = g_memory_output_stream_new_resizable ();

    g_assert_true (orm_exporter_export_result (exporter, result, stream, NULL, &error));
    g_assert_no_error (error);

    return test_stream_contents (stream);
}

static void
test_e2e_csv (TestDbFixture *fixture,
              gconstpointer  user_data)
{
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(GError)         error = NULL;
    g_autofree gchar         *out = NULL;

    g_assert_true (orm_connection_execute (
        fixture->connection,
        "CREATE TABLE t (id INTEGER, label TEXT)", &error));
    g_assert_no_error (error);
    g_assert_true (orm_connection_execute (
        fixture->connection,
        "INSERT INTO t VALUES (1, 'a,b'), (2, NULL)", &error));
    g_assert_no_error (error);

    out = test_export_query (fixture, ORM_EXPORTER (exporter),
                             "SELECT id, label FROM t ORDER BY id");

    g_assert_cmpstr (out, ==, "id,label\n1,\"a,b\"\n2,\n");
    g_assert_cmpuint (orm_exporter_get_rows_written (ORM_EXPORTER (exporter)), ==, 2);
}

static void
test_e2e_json (TestDbFixture *fixture,
               gconstpointer  user_data)
{
    g_autoptr(OrmJsonExporter) exporter = orm_json_exporter_new ();
    g_autoptr(GError)          error = NULL;
    g_autofree gchar          *out = NULL;

    g_assert_true (orm_connection_execute (
        fixture->connection,
        "CREATE TABLE t (id INTEGER, label TEXT)", &error));
    g_assert_no_error (error);
    g_assert_true (orm_connection_execute (
        fixture->connection,
        "INSERT INTO t VALUES (1, 'a\"b'), (2, NULL)", &error));
    g_assert_no_error (error);

    out = test_export_query (fixture, ORM_EXPORTER (exporter),
                             "SELECT id, label FROM t ORDER BY id");

    g_assert_cmpstr (out, ==,
                     "[{\"id\":1,\"label\":\"a\\\"b\"},{\"id\":2,\"label\":null}]");
}

/*
 * A result with no rows still knows its columns, so the CSV keeps its
 * header and the JSON is a well-formed empty array.
 */
static void
test_e2e_empty (TestDbFixture *fixture,
                gconstpointer  user_data)
{
    g_autoptr(OrmCsvExporter)  csv = orm_csv_exporter_new ();
    g_autoptr(OrmJsonExporter) json = orm_json_exporter_new ();
    g_autoptr(GError)          error = NULL;
    g_autofree gchar          *csv_out = NULL;
    g_autofree gchar          *json_out = NULL;

    g_assert_true (orm_connection_execute (
        fixture->connection,
        "CREATE TABLE t (id INTEGER, label TEXT)", &error));
    g_assert_no_error (error);

    csv_out = test_export_query (fixture, ORM_EXPORTER (csv),
                                 "SELECT id, label FROM t");
    g_assert_cmpstr (csv_out, ==, "id,label\n");

    json_out = test_export_query (fixture, ORM_EXPORTER (json),
                                  "SELECT id, label FROM t");
    g_assert_cmpstr (json_out, ==, "[]");
}

/*
 * The failure an export must never hide.  orm_result_next() answers
 * %FALSE for a result that ended and for one that broke, so an exporter
 * that does not ask orm_result_get_error() afterwards writes a short file
 * and reports success -- and nothing downstream can tell that apart from
 * a query that really did return two rows.
 */
static void
test_e2e_result_error (TestDbFixture *fixture,
                       gconstpointer  user_data)
{
    g_autoptr(OrmCsvExporter) exporter = orm_csv_exporter_new ();
    g_autoptr(OrmResult)      result = NULL;
    g_autoptr(GOutputStream)  stream = NULL;
    g_autoptr(GError)         error = NULL;

    /*
     * abs() of the most negative integer overflows, and SQLite raises it
     * on the step that would produce the second row: one good row, then a
     * failure, mid-iteration.
     */
    result = orm_connection_query (
        fixture->connection,
        "SELECT 1 AS n UNION ALL SELECT abs(-9223372036854775807 - 1)", &error);
    g_assert_no_error (error);
    g_assert_nonnull (result);

    stream = g_memory_output_stream_new_resizable ();

    g_assert_false (orm_exporter_export_result (ORM_EXPORTER (exporter), result,
                                                stream, NULL, &error));
    g_assert_error (error, ORM_ERROR, ORM_ERROR_EXECUTE);
    g_assert_cmpuint (orm_exporter_get_rows_written (ORM_EXPORTER (exporter)), ==, 1);
}

int
main (int    argc,
      char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/export/csv/header", test_csv_header);
    g_test_add_func ("/export/csv/no-header", test_csv_no_header);
    g_test_add_func ("/export/csv/quote-delimiter", test_csv_quote_delimiter);
    g_test_add_func ("/export/csv/quote-doubled", test_csv_quote_doubled);
    g_test_add_func ("/export/csv/quote-newline", test_csv_quote_newline);
    g_test_add_func ("/export/csv/null-default", test_csv_null_default);
    g_test_add_func ("/export/csv/null-custom", test_csv_null_custom);
    g_test_add_func ("/export/csv/force-quotes", test_csv_force_quotes);
    g_test_add_func ("/export/csv/custom-delimiter", test_csv_custom_delimiter);
    g_test_add_func ("/export/csv/blob-base64", test_csv_blob_base64);
    g_test_add_func ("/export/csv/datetime-iso8601", test_csv_datetime_iso8601);
    g_test_add_func ("/export/csv/float-decimal-point", test_csv_float_decimal_point);
    g_test_add_func ("/export/csv/locale-decimal-point", test_csv_locale_decimal_point);
    g_test_add_func ("/export/csv/no-rows", test_csv_no_rows);

    g_test_add_func ("/export/json/array-of-objects", test_json_array_of_objects);
    g_test_add_func ("/export/json/array-of-arrays", test_json_array_of_arrays);
    g_test_add_func ("/export/json/array-of-arrays-bare", test_json_array_of_arrays_bare);
    g_test_add_func ("/export/json/pretty", test_json_pretty);
    g_test_add_func ("/export/json/pretty-wrapped", test_json_pretty_wrapped);
    g_test_add_func ("/export/json/string-escaping", test_json_string_escaping);
    g_test_add_func ("/export/json/null", test_json_null);
    g_test_add_func ("/export/json/non-finite", test_json_non_finite);
    g_test_add_func ("/export/json/finite-float", test_json_finite_float);
    g_test_add_func ("/export/json/blob-and-datetime", test_json_blob_and_datetime);
    g_test_add_func ("/export/json/no-rows", test_json_no_rows);
    g_test_add_func ("/export/json/no-rows-wrapped", test_json_no_rows_wrapped);

    g_test_add_func ("/export/cancelled", test_export_cancelled);
    g_test_add_func ("/export/rows-written", test_export_rows_written);

    TEST_ADD_FIXTURE ("/export/e2e/csv", test_e2e_csv);
    TEST_ADD_FIXTURE ("/export/e2e/json", test_e2e_json);
    TEST_ADD_FIXTURE ("/export/e2e/empty", test_e2e_empty);
    TEST_ADD_FIXTURE ("/export/e2e/result-error", test_e2e_result_error);

    return g_test_run ();
}
