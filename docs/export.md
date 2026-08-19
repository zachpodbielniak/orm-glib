# Export

An **exporter** writes a result set out as text. `OrmValue` could already
turn one value into a string; this is the layer that turns a whole query
into a file a spreadsheet or a JSON parser will accept.

```c
g_autoptr(OrmResult) result = orm_connection_query (conn, "SELECT * FROM users", &error);
g_autoptr(GFile) file = g_file_new_for_path ("users.csv");
g_autoptr(GFileOutputStream) stream = g_file_replace (file, NULL, FALSE,
                                                      G_FILE_CREATE_NONE, NULL, &error);
g_autoptr(OrmCsvExporter) csv = orm_csv_exporter_new ();

orm_exporter_export_result (ORM_EXPORTER (csv), result,
                            G_OUTPUT_STREAM (stream), NULL, &error);
```

## OrmExporter is an abstract class, not an interface

`OrmExporter` gives a subclass three hooks:

```c
gboolean (*write_begin) (OrmExporter *self, GOutputStream *stream, gint n_columns,
                         const gchar * const *column_names, GError **error);
gboolean (*write_row)   (OrmExporter *self, GOutputStream *stream, OrmRow *row, GError **error);
gboolean (*write_end)   (OrmExporter *self, GOutputStream *stream, GError **error);
```

and keeps everything else for itself: the iteration, the check that told
end-of-rows from a failed fetch, the `GCancellable` poll between rows, the
`GBufferedOutputStream` wrapper, and the rows-written counter.

That split is the reason it is a class. An interface can only declare the
three hooks, which would leave the loop to be written again in every
format — and the second copy is where they start to disagree. One
exporter remembers to ask `orm_result_get_error()`; the next forgets, and
silently writes a file that is short but looks finished. Here there is one
loop, and a format cannot get it wrong because it never sees it.

Both public entry points share that loop:

| Function | Source | Notes |
|---|---|---|
| `orm_exporter_export_result` | `OrmResult` | Streams; checks the result's error afterwards |
| `orm_exporter_export_rows` | `GPtrArray` of `OrmRow` | Column names come from the first row |

`orm_exporter_get_rows_written()` is atomic and safe to poll from another
thread while an export runs — it is what a progress bar reads. Each
export resets it.

### The truncation trap

`orm_result_next()` returns `FALSE` both when the rows are exhausted and
when the fetch failed. An exporter that stops at the `FALSE` and reports
success produces a file that is missing rows and carries no sign of it —
the worst possible outcome, because nothing downstream can detect it. So
after the loop:

```c
result_error = orm_result_get_error (result);
if (result_error != NULL)
    /* propagate: the export failed, however much of it was written */;
```

A pre-cancelled `GCancellable` fails before anything is written;
cancelling mid-export fails at the next row boundary. Either way the
return is `FALSE` with `G_IO_ERROR_CANCELLED`.

## OrmCsvExporter

RFC 4180 by default: comma separated, quotes doubled, a header line, LF
endings.

| Property | Type | Default | Meaning |
|---|---|---|---|
| `delimiter` | string | `,` | Between fields; a tab makes it TSV |
| `quote-char` | string | `"` | Wraps a quoted field, doubled inside one |
| `include-header` | boolean | `TRUE` | Write the column names first |
| `null-string` | string | `""` | What a NULL becomes |
| `line-ending` | string | `\n` | `\r\n` for Windows consumers |
| `force-quotes` | boolean | `FALSE` | Quote every field, not just the ones needing it |

**Quoting.** A field is quoted when it contains the delimiter, the quote
character, CR or LF — or always, under `force-quotes`. CSV has no escape
character, so a quote inside a quoted field is written **twice**:

```
He said "hi"     →     "He said ""hi"""
Smith, John      →     "Smith, John"
```

**NULL.** The null string is written as-is, unquoted (unless
`force-quotes`). That is deliberate: a caller who sets `\N` or `NULL` is
choosing a token their reader recognizes as absent data, and quoting it
would make it an ordinary string indistinguishable from one.

**Values.** Blobs become Base64, datetimes ISO 8601, booleans
`true`/`false`.

**The locale trap.** Floats are formatted with the `g_ascii_*` family, not
`printf`. In a `de_DE` or `fr_FR` locale `printf("%g", 1.5)` writes
`1,5` — which in a comma-delimited file is a column boundary. The result
is a corrupt file produced by a program that looks correct and passes
every test run in an English locale. `tests/test-export.c` sets a
comma-decimal locale and asserts on the bytes.

## OrmJsonExporter

Emitted by hand into a `GString`, with no JSON library: a DOM-building
library would hold every row of a large export in memory before a byte
reached the stream, which is exactly what the streaming base class exists
to avoid.

| Property | Type | Default | Meaning |
|---|---|---|---|
| `layout` | `OrmJsonLayout` | `ARRAY_OF_OBJECTS` | Object per row, or array per row |
| `pretty` | boolean | `FALSE` | Break lines and indent |
| `indent` | uint | `2` | Spaces per level when pretty |
| `include-columns` | boolean | `TRUE` | Wrap positional rows with their column list |

```c
/* ARRAY_OF_OBJECTS */
[{"id":1,"name":"Alice"},{"id":2,"name":null}]

/* ARRAY_OF_ARRAYS, include-columns TRUE */
{"columns":["id","name"],"rows":[[1,"Alice"],[2,null]]}

/* ARRAY_OF_ARRAYS, include-columns FALSE */
[[1,"Alice"],[2,null]]
```

`include-columns` is ignored for the object layout, where every row
already names its columns.

**Escaping.** `\"` `\\` `\b` `\f` `\n` `\r` `\t`, and `\uXXXX` for *every*
control character below `0x20` — not only the ones with a short form. A
stray `0x01` out of a text column makes the whole document unparseable,
and it is invisible to any test that only checks the surrounding text.
Bytes at or above `0x80` pass through unchanged: they are UTF-8, which
JSON takes verbatim.

**NaN and infinity become `null`.** They are not in the JSON grammar, and
emitting the C library's `nan` or `inf` tokens produces a file every
conforming parser rejects. `null` at least round-trips as "no usable
number here".

**Other values.** NULL is `null`, blobs are Base64 strings, datetimes are
ISO 8601 strings, floats go through the same locale-independent formatter
as CSV.

## Adding a format

Subclass `OrmExporter`, implement the three hooks, write to the
`GOutputStream` you are handed (it is already buffered). The class vtable
carries `gpointer _reserved[8]`, so hooks can be added later without
breaking an out-of-tree exporter's ABI.
