/* orm-csv-exporter.c
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

#include "orm-csv-exporter.h"
#include <string.h>

/*
 * OrmCsvExporter - a result set as delimiter-separated text.
 *
 * CSV has exactly one hard rule and it is the quoting: a field that
 * contains the delimiter, the quote character or a line break must be
 * quoted, and a quote inside a quoted field is written twice.  Anything
 * else is a file that a reader silently splits into the wrong number of
 * columns, which is worse than a file that fails to parse.
 */

#define ORM_CSV_DEFAULT_DELIMITER   ","
#define ORM_CSV_DEFAULT_QUOTE       "\""
#define ORM_CSV_DEFAULT_NULL_STRING ""
#define ORM_CSV_DEFAULT_LINE_ENDING "\n"

struct _OrmCsvExporter
{
    OrmExporter parent_instance;

    gchar    *delimiter;
    gchar    *quote_char;
    gchar    *null_string;
    gchar    *line_ending;
    gboolean  include_header;
    gboolean  force_quotes;
};

G_DEFINE_TYPE (OrmCsvExporter, orm_csv_exporter, ORM_TYPE_EXPORTER)

enum {
    PROP_0,
    PROP_DELIMITER,
    PROP_QUOTE_CHAR,
    PROP_INCLUDE_HEADER,
    PROP_NULL_STRING,
    PROP_LINE_ENDING,
    PROP_FORCE_QUOTES,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

/*
 * The string properties are dereferenced on every field, so a %NULL from
 * a binding that means "unset" has to become the empty string here rather
 * than a crash three layers down.
 */
static void
orm_csv_exporter_set_string (gchar       **field,
                             const gchar  *value)
{
    g_free (*field);
    *field = g_strdup ((value != NULL) ? value : "");
}

/*
 * A field needs quoting when leaving it bare would change where the
 * reader thinks the field ends.
 */
static gboolean
orm_csv_exporter_needs_quotes (OrmCsvExporter *self,
                               const gchar    *field)
{
    if (self->force_quotes)
        return TRUE;

    if (self->delimiter[0] != '\0' && strstr (field, self->delimiter) != NULL)
        return TRUE;

    if (self->quote_char[0] != '\0' && strstr (field, self->quote_char) != NULL)
        return TRUE;

    return strpbrk (field, "\r\n") != NULL;
}

static void
orm_csv_exporter_append_field (OrmCsvExporter *self,
                               GString        *out,
                               const gchar    *field,
                               gboolean        quote)
{
    const gchar *p;
    const gchar *hit;
    gsize        quote_len;

    quote_len = strlen (self->quote_char);

    if (!quote || quote_len == 0)
    {
        g_string_append (out, field);
        return;
    }

    g_string_append (out, self->quote_char);

    /*
     * CSV has no escape character: inside a quoted field the only way to
     * write a quote is to write two, and a reader collapses the pair back.
     */
    for (p = field; (hit = strstr (p, self->quote_char)) != NULL; p = hit + quote_len)
    {
        g_string_append_len (out, p, (gssize) (hit - p));
        g_string_append (out, self->quote_char);
        g_string_append (out, self->quote_char);
    }

    g_string_append (out, p);
    g_string_append (out, self->quote_char);
}

static void
orm_csv_exporter_append_value (OrmCsvExporter *self,
                               GString        *out,
                               OrmValue       *value)
{
    g_autofree gchar *owned = NULL;
    const gchar      *text;
    gchar             number[G_ASCII_DTOSTR_BUF_SIZE];
    GBytes           *bytes;
    GDateTime        *datetime;
    gconstpointer     data;
    gsize             size = 0;

    if (value == NULL || orm_value_is_null (value))
    {
        /*
         * The null string is emitted as written.  A caller who picks
         * "\N" or "NULL" is picking a token their reader recognizes as
         * absent data, and quoting it would make it an ordinary string --
         * unless they asked for everything to be quoted.
         */
        orm_csv_exporter_append_field (self, out, self->null_string, self->force_quotes);
        return;
    }

    switch (orm_value_get_value_type (value))
    {
    case ORM_VALUE_INTEGER:
        owned = g_strdup_printf ("%" G_GINT64_FORMAT, orm_value_get_integer (value));
        text = owned;
        break;

    case ORM_VALUE_FLOAT:
        text = orm_exporter_format_double (orm_value_get_float (value),
                                           number, sizeof (number));
        break;

    case ORM_VALUE_BOOLEAN:
        text = orm_value_get_boolean (value) ? "true" : "false";
        break;

    case ORM_VALUE_BLOB:
        /*
         * Base64 rather than the raw bytes, because a blob can contain
         * the delimiter, a NUL or a line break, and a text format has no
         * way to carry those.
         */
        bytes = orm_value_get_blob (value);
        data = (bytes != NULL) ? g_bytes_get_data (bytes, &size) : NULL;
        owned = g_base64_encode (data, size);
        text = owned;
        break;

    case ORM_VALUE_DATETIME:
        datetime = orm_value_get_datetime (value);
        owned = (datetime != NULL) ? g_date_time_format_iso8601 (datetime) : NULL;
        text = (owned != NULL) ? owned : "";
        break;

    case ORM_VALUE_STRING:
    default:
        text = orm_value_get_string (value);
        if (text == NULL)
            text = "";
        break;
    }

    orm_csv_exporter_append_field (self, out, text,
                                   orm_csv_exporter_needs_quotes (self, text));
}

static gboolean
orm_csv_exporter_write_begin (OrmExporter          *exporter,
                              GOutputStream        *stream,
                              gint                  n_columns,
                              const gchar * const  *column_names,
                              GError              **error)
{
    OrmCsvExporter     *self = ORM_CSV_EXPORTER (exporter);
    g_autoptr(GString)  out = NULL;
    const gchar        *name;
    gint                i;

    if (!self->include_header || n_columns <= 0)
        return TRUE;

    out = g_string_new (NULL);

    for (i = 0; i < n_columns; i++)
    {
        if (i > 0)
            g_string_append (out, self->delimiter);

        name = (column_names[i] != NULL) ? column_names[i] : "";
        orm_csv_exporter_append_field (self, out, name,
                                       orm_csv_exporter_needs_quotes (self, name));
    }

    g_string_append (out, self->line_ending);

    return g_output_stream_write_all (stream, out->str, out->len, NULL, NULL, error);
}

static gboolean
orm_csv_exporter_write_row (OrmExporter    *exporter,
                            GOutputStream  *stream,
                            OrmRow         *row,
                            GError        **error)
{
    OrmCsvExporter     *self = ORM_CSV_EXPORTER (exporter);
    g_autoptr(GString)  out = NULL;
    gint                n_columns;
    gint                i;

    out = g_string_new (NULL);
    n_columns = orm_row_get_column_count (row);

    for (i = 0; i < n_columns; i++)
    {
        if (i > 0)
            g_string_append (out, self->delimiter);

        orm_csv_exporter_append_value (self, out, orm_row_get_value (row, i));
    }

    g_string_append (out, self->line_ending);

    return g_output_stream_write_all (stream, out->str, out->len, NULL, NULL, error);
}

/*
 * CSV has no trailer: every row already ended with the line ending.
 */
static gboolean
orm_csv_exporter_write_end (OrmExporter    *exporter,
                            GOutputStream  *stream,
                            GError        **error)
{
    return TRUE;
}

static void
orm_csv_exporter_finalize (GObject *object)
{
    OrmCsvExporter *self = ORM_CSV_EXPORTER (object);

    g_free (self->delimiter);
    g_free (self->quote_char);
    g_free (self->null_string);
    g_free (self->line_ending);

    G_OBJECT_CLASS (orm_csv_exporter_parent_class)->finalize (object);
}

static void
orm_csv_exporter_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
    OrmCsvExporter *self = ORM_CSV_EXPORTER (object);

    switch (prop_id)
    {
    case PROP_DELIMITER:
        g_value_set_string (value, self->delimiter);
        break;
    case PROP_QUOTE_CHAR:
        g_value_set_string (value, self->quote_char);
        break;
    case PROP_INCLUDE_HEADER:
        g_value_set_boolean (value, self->include_header);
        break;
    case PROP_NULL_STRING:
        g_value_set_string (value, self->null_string);
        break;
    case PROP_LINE_ENDING:
        g_value_set_string (value, self->line_ending);
        break;
    case PROP_FORCE_QUOTES:
        g_value_set_boolean (value, self->force_quotes);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_csv_exporter_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    OrmCsvExporter *self = ORM_CSV_EXPORTER (object);

    switch (prop_id)
    {
    case PROP_DELIMITER:
        orm_csv_exporter_set_string (&self->delimiter, g_value_get_string (value));
        break;
    case PROP_QUOTE_CHAR:
        orm_csv_exporter_set_string (&self->quote_char, g_value_get_string (value));
        break;
    case PROP_INCLUDE_HEADER:
        self->include_header = g_value_get_boolean (value);
        break;
    case PROP_NULL_STRING:
        orm_csv_exporter_set_string (&self->null_string, g_value_get_string (value));
        break;
    case PROP_LINE_ENDING:
        orm_csv_exporter_set_string (&self->line_ending, g_value_get_string (value));
        break;
    case PROP_FORCE_QUOTES:
        self->force_quotes = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_csv_exporter_class_init (OrmCsvExporterClass *klass)
{
    GObjectClass     *object_class = G_OBJECT_CLASS (klass);
    OrmExporterClass *exporter_class = ORM_EXPORTER_CLASS (klass);

    object_class->finalize = orm_csv_exporter_finalize;
    object_class->get_property = orm_csv_exporter_get_property;
    object_class->set_property = orm_csv_exporter_set_property;

    exporter_class->write_begin = orm_csv_exporter_write_begin;
    exporter_class->write_row = orm_csv_exporter_write_row;
    exporter_class->write_end = orm_csv_exporter_write_end;

    /**
     * OrmCsvExporter:delimiter:
     *
     * The string between fields.  A tab makes the output TSV.
     */
    properties[PROP_DELIMITER] =
        g_param_spec_string ("delimiter",
                             "Delimiter",
                             "The string written between fields",
                             ORM_CSV_DEFAULT_DELIMITER,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    /**
     * OrmCsvExporter:quote-char:
     *
     * The string that wraps a field needing quotes, and which is doubled
     * when it appears inside one.
     */
    properties[PROP_QUOTE_CHAR] =
        g_param_spec_string ("quote-char",
                             "Quote character",
                             "The string used to quote fields",
                             ORM_CSV_DEFAULT_QUOTE,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    /**
     * OrmCsvExporter:include-header:
     *
     * Whether to write the column names as the first line.
     */
    properties[PROP_INCLUDE_HEADER] =
        g_param_spec_boolean ("include-header",
                              "Include header",
                              "Whether to write a header line of column names",
                              TRUE,
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS);

    /**
     * OrmCsvExporter:null-string:
     *
     * What a NULL becomes.  Empty by default, which is what spreadsheets
     * expect; a sentinel such as "\N" is what makes NULL distinguishable
     * from an empty string on the way back in.
     */
    properties[PROP_NULL_STRING] =
        g_param_spec_string ("null-string",
                             "NULL string",
                             "The text written for a NULL value",
                             ORM_CSV_DEFAULT_NULL_STRING,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    /**
     * OrmCsvExporter:line-ending:
     *
     * The string that ends each line.  RFC 4180 says CRLF; LF is what
     * every tool outside Windows actually wants, so it is the default.
     */
    properties[PROP_LINE_ENDING] =
        g_param_spec_string ("line-ending",
                             "Line ending",
                             "The string written at the end of each line",
                             ORM_CSV_DEFAULT_LINE_ENDING,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    /**
     * OrmCsvExporter:force-quotes:
     *
     * Whether to quote every field rather than only the ones that need
     * it.  Some readers infer types from whether a field was quoted, and
     * this is how you tell them everything is text.
     */
    properties[PROP_FORCE_QUOTES] =
        g_param_spec_boolean ("force-quotes",
                              "Force quotes",
                              "Whether to quote every field",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
orm_csv_exporter_init (OrmCsvExporter *self)
{
    self->delimiter = g_strdup (ORM_CSV_DEFAULT_DELIMITER);
    self->quote_char = g_strdup (ORM_CSV_DEFAULT_QUOTE);
    self->null_string = g_strdup (ORM_CSV_DEFAULT_NULL_STRING);
    self->line_ending = g_strdup (ORM_CSV_DEFAULT_LINE_ENDING);
    self->include_header = TRUE;
    self->force_quotes = FALSE;
}

/**
 * orm_csv_exporter_new:
 *
 * Creates a CSV exporter with RFC 4180 defaults: comma separated, quotes
 * doubled, a header line, LF endings.
 *
 * Returns: (transfer full): A new #OrmCsvExporter
 */
OrmCsvExporter *
orm_csv_exporter_new (void)
{
    return g_object_new (ORM_TYPE_CSV_EXPORTER, NULL);
}
