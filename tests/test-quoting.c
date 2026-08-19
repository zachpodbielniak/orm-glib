/* test-quoting.c
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
 * Identifier and string quoting.
 *
 * A table or column name is not always something the programmer typed:
 * a schema browser reads names out of a catalog, and a migration reads
 * them out of a file. If quoting does not escape the quote character
 * itself, a name containing one closes its own quoting and everything
 * after it is parsed as SQL -- so `x" ; DROP TABLE users --` is not a
 * name, it is a statement.
 *
 * These tests exist because the identifier path did not escape while the
 * string-literal path always did.
 */

#include <glib.h>
#include <glib-object.h>

#define ORM_INSIDE
#include "orm.h"
#undef ORM_INSIDE

/*
 * Asserts that @quoted is @raw wrapped in @qc with every internal @qc
 * doubled -- i.e. that unwrapping it yields exactly @raw again.
 */
static void
assert_round_trips (const gchar *quoted,
                    const gchar *raw,
                    gchar        qc)
{
    GString     *unwrapped;
    const gchar *p;
    gsize        len;

    g_assert_nonnull (quoted);
    len = strlen (quoted);

    g_assert_cmpint (len, >=, 2);
    g_assert_cmpint (quoted[0], ==, qc);
    g_assert_cmpint (quoted[len - 1], ==, qc);

    unwrapped = g_string_new (NULL);

    for (p = quoted + 1; p < quoted + len - 1; p++)
    {
        if (*p == qc)
        {
            /* Must be a doubled pair, never a lone quote. */
            g_assert_cmpint (*(p + 1), ==, qc);
            p++;
        }
        g_string_append_c (unwrapped, *p);
    }

    g_assert_cmpstr (unwrapped->str, ==, raw);
    g_string_free (unwrapped, TRUE);
}

static void
test_quoting_identifier_plain (void)
{
    g_autoptr(GObject) obj = NULL;
    OrmDialect        *dialect;
    g_autofree gchar  *quoted = NULL;

    dialect = orm_dialect_for_type (ORM_DIALECT_SQLITE);
    obj = G_OBJECT (dialect);

    quoted = orm_dialect_quote_identifier (dialect, "users");
    g_assert_cmpstr (quoted, ==, "\"users\"");
}

/*
 * The injection case.  Before the quote character was doubled, this
 * produced "x";DROP TABLE users;--" -- a closed identifier followed by
 * live SQL.
 */
static void
test_quoting_identifier_embedded_quote (void)
{
    g_autoptr(GObject) obj = NULL;
    OrmDialect        *dialect;
    g_autofree gchar  *quoted = NULL;
    const gchar       *hostile = "x\";DROP TABLE users;--";

    dialect = orm_dialect_for_type (ORM_DIALECT_SQLITE);
    obj = G_OBJECT (dialect);

    quoted = orm_dialect_quote_identifier (dialect, hostile);

    /*
     * The round-trip check is the real assertion: it walks the quoted
     * form and fails on any quote character that is not part of a
     * doubled pair, which is exactly the condition that would let the
     * payload out.
     */
    assert_round_trips (quoted, hostile, '"');
}

static void
test_quoting_identifier_only_quotes (void)
{
    g_autoptr(GObject) obj = NULL;
    OrmDialect        *dialect;
    g_autofree gchar  *quoted = NULL;

    dialect = orm_dialect_for_type (ORM_DIALECT_SQLITE);
    obj = G_OBJECT (dialect);

    quoted = orm_dialect_quote_identifier (dialect, "\"\"");
    assert_round_trips (quoted, "\"\"", '"');
}

static void
test_quoting_identifier_empty (void)
{
    g_autoptr(GObject) obj = NULL;
    OrmDialect        *dialect;
    g_autofree gchar  *quoted = NULL;

    dialect = orm_dialect_for_type (ORM_DIALECT_SQLITE);
    obj = G_OBJECT (dialect);

    quoted = orm_dialect_quote_identifier (dialect, "");
    g_assert_cmpstr (quoted, ==, "\"\"");
}

#ifdef ORM_ENABLE_MYSQL
/*
 * MySQL quotes with backticks and overrides the default implementation,
 * so it needs its own escaping and its own test.
 */
static void
test_quoting_identifier_mysql_backtick (void)
{
    g_autoptr(GObject) obj = NULL;
    OrmDialect        *dialect;
    g_autofree gchar  *plain = NULL;
    g_autofree gchar  *hostile = NULL;

    dialect = orm_dialect_for_type (ORM_DIALECT_MYSQL);
    obj = G_OBJECT (dialect);

    plain = orm_dialect_quote_identifier (dialect, "users");
    g_assert_cmpstr (plain, ==, "`users`");

    hostile = orm_dialect_quote_identifier (dialect, "x`;DROP TABLE users;--");
    assert_round_trips (hostile, "x`;DROP TABLE users;--", '`');
}
#endif

/*
 * String literals were always escaped correctly; this keeps it that way.
 */
static void
test_quoting_string_embedded_quote (void)
{
    g_autoptr(GObject) obj = NULL;
    OrmDialect        *dialect;
    g_autofree gchar  *quoted = NULL;

    dialect = orm_dialect_for_type (ORM_DIALECT_SQLITE);
    obj = G_OBJECT (dialect);

    quoted = orm_dialect_quote_string (dialect, "O'Brien');DROP TABLE t;--");
    assert_round_trips (quoted, "O'Brien');DROP TABLE t;--", '\'');
}

/*
 * The end-to-end property: a table whose name contains a quote can be
 * created, written and read through quoted identifiers, and nothing in
 * the name is ever executed.
 */
static void
test_quoting_hostile_table_name_round_trip (void)
{
    g_autoptr(OrmEngine)     engine = NULL;
    g_autoptr(OrmConnection) connection = NULL;
    g_autoptr(GError)        error = NULL;
    g_autofree gchar        *quoted = NULL;
    g_autofree gchar        *create = NULL;
    g_autofree gchar        *insert = NULL;
    g_autofree gchar        *select = NULL;
    g_autoptr(OrmResult)     result = NULL;
    g_autoptr(OrmValue)      value = NULL;
    OrmDialect              *dialect;
    const gchar             *hostile = "ev\"il\";DROP TABLE canary;--";

    engine = orm_engine_new ("sqlite:///:memory:", &error);
    g_assert_no_error (error);

    connection = orm_engine_connect (engine, &error);
    g_assert_no_error (error);

    /* A table that must still be here at the end. */
    g_assert_true (orm_connection_execute (
        connection, "CREATE TABLE canary (id INTEGER)", &error));
    g_assert_no_error (error);

    dialect = orm_engine_get_dialect (engine);
    quoted = orm_dialect_quote_identifier (dialect, hostile);

    create = g_strdup_printf ("CREATE TABLE %s (n INTEGER)", quoted);
    g_assert_true (orm_connection_execute (connection, create, &error));
    g_assert_no_error (error);

    insert = g_strdup_printf ("INSERT INTO %s (n) VALUES (42)", quoted);
    g_assert_true (orm_connection_execute (connection, insert, &error));
    g_assert_no_error (error);

    select = g_strdup_printf ("SELECT n FROM %s", quoted);
    result = orm_connection_query (connection, select, &error);
    g_assert_no_error (error);
    value = orm_result_get_scalar (result);
    g_assert_cmpint (orm_value_get_integer (value), ==, 42);

    /* The payload in the name did not run. */
    g_clear_object (&result);
    result = orm_connection_query (connection, "SELECT COUNT(*) FROM canary", &error);
    g_assert_no_error (error);
    g_assert_nonnull (result);
}

gint
main (gint    argc,
      gchar **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/quoting/identifier/plain", test_quoting_identifier_plain);
    g_test_add_func ("/quoting/identifier/embedded-quote",
                     test_quoting_identifier_embedded_quote);
    g_test_add_func ("/quoting/identifier/only-quotes",
                     test_quoting_identifier_only_quotes);
    g_test_add_func ("/quoting/identifier/empty", test_quoting_identifier_empty);
#ifdef ORM_ENABLE_MYSQL
    g_test_add_func ("/quoting/identifier/mysql-backtick",
                     test_quoting_identifier_mysql_backtick);
#endif
    g_test_add_func ("/quoting/string/embedded-quote",
                     test_quoting_string_embedded_quote);
    g_test_add_func ("/quoting/hostile-table-name-round-trip",
                     test_quoting_hostile_table_name_round_trip);

    return g_test_run ();
}
