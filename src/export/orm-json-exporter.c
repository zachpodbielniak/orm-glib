/* orm-json-exporter.c
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

#include "orm-json-exporter.h"

/*
 * OrmJsonExporter - a result set as JSON, emitted by hand.
 *
 * By hand because the alternative is a whole DOM: a JSON library would
 * have every row of a million-row export resident as nodes before a byte
 * reached the stream, which defeats the streaming the exporter base class
 * exists to provide.  A document this shape -- an array of flat objects
 * -- is small enough to serialize correctly directly, provided the string
 * escaping is right, and that is the one part of it worth testing hard.
 */

#define ORM_JSON_DEFAULT_INDENT (2)

struct _OrmJsonExporter
{
    OrmExporter parent_instance;

    OrmJsonLayout layout;
    gboolean      pretty;
    guint         indent;
    gboolean      include_columns;

    /*
     * Per-export state.  @wrapped is decided once in write_begin rather
     * than read from the properties each time, so a property changed
     * mid-export cannot produce a document whose opening and closing
     * brackets disagree.
     */
    gboolean      wrapped;
    gboolean      any_rows;
};

G_DEFINE_TYPE (OrmJsonExporter, orm_json_exporter, ORM_TYPE_EXPORTER)

enum {
    PROP_0,
    PROP_LAYOUT,
    PROP_PRETTY,
    PROP_INDENT,
    PROP_INCLUDE_COLUMNS,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

/*
 * A line break and @level levels of indent, or nothing at all when the
 * output is compact.  Every structural position in the document goes
 * through this, which is what keeps the two modes from drifting apart.
 */
static void
orm_json_exporter_newline (OrmJsonExporter *self,
                           GString         *out,
                           guint            level)
{
    if (!self->pretty)
        return;

    g_string_append_c (out, '\n');
    g_string_append_printf (out, "%*s", (gint) (level * self->indent), "");
}

/*
 * The space after a colon, in pretty mode only.
 */
static void
orm_json_exporter_space (OrmJsonExporter *self,
                         GString         *out)
{
    if (self->pretty)
        g_string_append_c (out, ' ');
}

/*
 * The indent level the row elements sit at: one deeper than the array
 * that holds them, which is itself nested inside an object when the
 * column names are wrapped alongside.
 */
static guint
orm_json_exporter_rows_level (OrmJsonExporter *self)
{
    return self->wrapped ? 1 : 0;
}

static void
orm_json_exporter_append_string (GString     *out,
                                 const gchar *str)
{
    const gchar *p;
    guchar       c;

    g_string_append_c (out, '"');

    for (p = (str != NULL) ? str : ""; *p != '\0'; p++)
    {
        c = (guchar) *p;

        switch (c)
        {
        case '"':
            g_string_append (out, "\\\"");
            break;
        case '\\':
            g_string_append (out, "\\\\");
            break;
        case '\b':
            g_string_append (out, "\\b");
            break;
        case '\f':
            g_string_append (out, "\\f");
            break;
        case '\n':
            g_string_append (out, "\\n");
            break;
        case '\r':
            g_string_append (out, "\\r");
            break;
        case '\t':
            g_string_append (out, "\\t");
            break;
        default:
            /*
             * Every control character below 0x20 is forbidden unescaped,
             * not just the ones with a short form -- a stray 0x01 out of
             * a text column would otherwise make the whole document
             * unparseable.  Bytes at or above 0x80 pass through: they are
             * UTF-8 continuation bytes, which JSON takes verbatim.
             */
            if (c < 0x20)
                g_string_append_printf (out, "\\u%04x", c);
            else
                g_string_append_c (out, (gchar) c);
            break;
        }
    }

    g_string_append_c (out, '"');
}

/*
 * JSON numbers are finite by definition.  NaN and the infinities have no
 * spelling in the grammar, and emitting the C library's "nan" or "inf"
 * produces a file that every conforming parser rejects -- so they become
 * null, which at least round-trips as "no usable number here".
 *
 * Tested without <math.h>'s C99 macros, which -std=gnu89 does not
 * guarantee: a NaN is the only value unequal to itself, and an infinity
 * the only one outside the finite range.
 */
static gboolean
orm_json_exporter_is_finite (gdouble value)
{
    return !(value != value) && value <= G_MAXDOUBLE && value >= -G_MAXDOUBLE;
}

static void
orm_json_exporter_append_value (OrmJsonExporter *self,
                                GString         *out,
                                OrmValue        *value)
{
    g_autofree gchar *owned = NULL;
    gchar             number[G_ASCII_DTOSTR_BUF_SIZE];
    gdouble           real;
    GBytes           *bytes;
    GDateTime        *datetime;
    gconstpointer     data;
    gsize             size = 0;

    if (value == NULL || orm_value_is_null (value))
    {
        g_string_append (out, "null");
        return;
    }

    switch (orm_value_get_value_type (value))
    {
    case ORM_VALUE_INTEGER:
        g_string_append_printf (out, "%" G_GINT64_FORMAT, orm_value_get_integer (value));
        break;

    case ORM_VALUE_FLOAT:
        real = orm_value_get_float (value);
        if (!orm_json_exporter_is_finite (real))
            g_string_append (out, "null");
        else
            g_string_append (out, orm_exporter_format_double (real, number, sizeof (number)));
        break;

    case ORM_VALUE_BOOLEAN:
        g_string_append (out, orm_value_get_boolean (value) ? "true" : "false");
        break;

    case ORM_VALUE_BLOB:
        bytes = orm_value_get_blob (value);
        data = (bytes != NULL) ? g_bytes_get_data (bytes, &size) : NULL;
        owned = g_base64_encode (data, size);
        orm_json_exporter_append_string (out, owned);
        break;

    case ORM_VALUE_DATETIME:
        datetime = orm_value_get_datetime (value);
        owned = (datetime != NULL) ? g_date_time_format_iso8601 (datetime) : NULL;
        if (owned != NULL)
            orm_json_exporter_append_string (out, owned);
        else
            g_string_append (out, "null");
        break;

    case ORM_VALUE_STRING:
    default:
        orm_json_exporter_append_string (out, orm_value_get_string (value));
        break;
    }
}

static gboolean
orm_json_exporter_write_begin (OrmExporter          *exporter,
                               GOutputStream        *stream,
                               gint                  n_columns,
                               const gchar * const  *column_names,
                               GError              **error)
{
    OrmJsonExporter    *self = ORM_JSON_EXPORTER (exporter);
    g_autoptr(GString)  out = NULL;
    gint                i;

    self->wrapped = (self->layout == ORM_JSON_LAYOUT_ARRAY_OF_ARRAYS) &&
                    self->include_columns;
    self->any_rows = FALSE;

    out = g_string_new (NULL);

    if (self->wrapped)
    {
        /*
         * Positional rows are unreadable without the column list, so the
         * wrapped form carries it once at the top rather than leaving the
         * consumer to know it out of band.
         */
        g_string_append_c (out, '{');
        orm_json_exporter_newline (self, out, 1);
        g_string_append (out, "\"columns\":");
        orm_json_exporter_space (self, out);
        g_string_append_c (out, '[');

        for (i = 0; i < n_columns; i++)
        {
            if (i > 0)
                g_string_append_c (out, ',');

            orm_json_exporter_newline (self, out, 2);
            orm_json_exporter_append_string (out, column_names[i]);
        }

        if (n_columns > 0)
            orm_json_exporter_newline (self, out, 1);

        g_string_append_c (out, ']');
        g_string_append_c (out, ',');
        orm_json_exporter_newline (self, out, 1);
        g_string_append (out, "\"rows\":");
        orm_json_exporter_space (self, out);
    }

    g_string_append_c (out, '[');

    return g_output_stream_write_all (stream, out->str, out->len, NULL, NULL, error);
}

static gboolean
orm_json_exporter_write_row (OrmExporter    *exporter,
                             GOutputStream  *stream,
                             OrmRow         *row,
                             GError        **error)
{
    OrmJsonExporter    *self = ORM_JSON_EXPORTER (exporter);
    g_autoptr(GString)  out = NULL;
    const gchar        *name;
    guint               level;
    gint                n_columns;
    gint                i;

    out = g_string_new (NULL);
    level = orm_json_exporter_rows_level (self);
    n_columns = orm_row_get_column_count (row);

    if (self->any_rows)
        g_string_append_c (out, ',');
    self->any_rows = TRUE;

    orm_json_exporter_newline (self, out, level + 1);

    if (self->layout == ORM_JSON_LAYOUT_ARRAY_OF_OBJECTS)
    {
        g_string_append_c (out, '{');

        for (i = 0; i < n_columns; i++)
        {
            if (i > 0)
                g_string_append_c (out, ',');

            orm_json_exporter_newline (self, out, level + 2);

            name = orm_row_get_column_name (row, i);
            orm_json_exporter_append_string (out, name);
            g_string_append_c (out, ':');
            orm_json_exporter_space (self, out);
            orm_json_exporter_append_value (self, out, orm_row_get_value (row, i));
        }

        if (n_columns > 0)
            orm_json_exporter_newline (self, out, level + 1);

        g_string_append_c (out, '}');
    }
    else
    {
        g_string_append_c (out, '[');

        for (i = 0; i < n_columns; i++)
        {
            if (i > 0)
                g_string_append_c (out, ',');

            orm_json_exporter_newline (self, out, level + 2);
            orm_json_exporter_append_value (self, out, orm_row_get_value (row, i));
        }

        if (n_columns > 0)
            orm_json_exporter_newline (self, out, level + 1);

        g_string_append_c (out, ']');
    }

    return g_output_stream_write_all (stream, out->str, out->len, NULL, NULL, error);
}

static gboolean
orm_json_exporter_write_end (OrmExporter    *exporter,
                             GOutputStream  *stream,
                             GError        **error)
{
    OrmJsonExporter    *self = ORM_JSON_EXPORTER (exporter);
    g_autoptr(GString)  out = NULL;

    out = g_string_new (NULL);

    /*
     * An export that wrote nothing closes on the same line it opened, so
     * an empty result is "[]" rather than a bracket pair straddling a
     * blank line.
     */
    if (self->any_rows)
        orm_json_exporter_newline (self, out, orm_json_exporter_rows_level (self));

    g_string_append_c (out, ']');

    if (self->wrapped)
    {
        orm_json_exporter_newline (self, out, 0);
        g_string_append_c (out, '}');
    }

    return g_output_stream_write_all (stream, out->str, out->len, NULL, NULL, error);
}

static void
orm_json_exporter_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
    OrmJsonExporter *self = ORM_JSON_EXPORTER (object);

    switch (prop_id)
    {
    case PROP_LAYOUT:
        g_value_set_enum (value, self->layout);
        break;
    case PROP_PRETTY:
        g_value_set_boolean (value, self->pretty);
        break;
    case PROP_INDENT:
        g_value_set_uint (value, self->indent);
        break;
    case PROP_INCLUDE_COLUMNS:
        g_value_set_boolean (value, self->include_columns);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_json_exporter_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
    OrmJsonExporter *self = ORM_JSON_EXPORTER (object);

    switch (prop_id)
    {
    case PROP_LAYOUT:
        self->layout = g_value_get_enum (value);
        break;
    case PROP_PRETTY:
        self->pretty = g_value_get_boolean (value);
        break;
    case PROP_INDENT:
        self->indent = g_value_get_uint (value);
        break;
    case PROP_INCLUDE_COLUMNS:
        self->include_columns = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
orm_json_exporter_class_init (OrmJsonExporterClass *klass)
{
    GObjectClass     *object_class = G_OBJECT_CLASS (klass);
    OrmExporterClass *exporter_class = ORM_EXPORTER_CLASS (klass);

    object_class->get_property = orm_json_exporter_get_property;
    object_class->set_property = orm_json_exporter_set_property;

    exporter_class->write_begin = orm_json_exporter_write_begin;
    exporter_class->write_row = orm_json_exporter_write_row;
    exporter_class->write_end = orm_json_exporter_write_end;

    /**
     * OrmJsonExporter:layout:
     *
     * Whether each row is an object keyed by column name or an array in
     * column order.
     */
    properties[PROP_LAYOUT] =
        g_param_spec_enum ("layout",
                           "Layout",
                           "How rows are arranged in the document",
                           ORM_TYPE_JSON_LAYOUT,
                           ORM_JSON_LAYOUT_ARRAY_OF_OBJECTS,
                           G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS);

    /**
     * OrmJsonExporter:pretty:
     *
     * Whether to break lines and indent.  Off by default: the compact
     * form is what goes over a wire, and it is smaller by a third.
     */
    properties[PROP_PRETTY] =
        g_param_spec_boolean ("pretty",
                              "Pretty",
                              "Whether to indent the output across lines",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS);

    /**
     * OrmJsonExporter:indent:
     *
     * Spaces per level when #OrmJsonExporter:pretty is set.
     */
    properties[PROP_INDENT] =
        g_param_spec_uint ("indent",
                           "Indent",
                           "Spaces per nesting level when pretty printing",
                           0, 32, ORM_JSON_DEFAULT_INDENT,
                           G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS);

    /**
     * OrmJsonExporter:include-columns:
     *
     * Whether an %ORM_JSON_LAYOUT_ARRAY_OF_ARRAYS export wraps its rows
     * as `{"columns": [...], "rows": [...]}` rather than emitting the
     * bare array.  Ignored for the object layout, where every row already
     * names its columns.
     */
    properties[PROP_INCLUDE_COLUMNS] =
        g_param_spec_boolean ("include-columns",
                              "Include columns",
                              "Whether positional rows carry a column list",
                              TRUE,
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
orm_json_exporter_init (OrmJsonExporter *self)
{
    self->layout = ORM_JSON_LAYOUT_ARRAY_OF_OBJECTS;
    self->pretty = FALSE;
    self->indent = ORM_JSON_DEFAULT_INDENT;
    self->include_columns = TRUE;
    self->wrapped = FALSE;
    self->any_rows = FALSE;
}

/**
 * orm_json_exporter_new:
 *
 * Creates a JSON exporter emitting a compact array of objects, one per
 * row, keyed by column name.
 *
 * Returns: (transfer full): A new #OrmJsonExporter
 */
OrmJsonExporter *
orm_json_exporter_new (void)
{
    return g_object_new (ORM_TYPE_JSON_EXPORTER, NULL);
}
