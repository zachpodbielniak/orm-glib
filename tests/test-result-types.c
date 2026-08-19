/* test-result-types.c
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
 * Column type metadata, over SQLite.
 *
 * The property that matters is that the answer describes the COLUMN, not
 * the row you happen to be sitting on. A per-row answer is easy to get
 * and useless in exactly the case where a caller needs it most: a NULL
 * cell in an integer column still has to report integer, or a UI cannot
 * decide how to align it and an editor cannot decide how to parse what
 * you type into it.
 */

#include <glib.h>
#include <glib-object.h>

#include "test-fixtures.h"

static OrmResult *
run (TestDbFixture *fixture,
     const gchar   *sql)
{
    g_autoptr(GError) error = NULL;
    OrmResult        *result;

    result = orm_connection_query (fixture->connection, sql, &error);
    g_assert_no_error (error);
    g_assert_nonnull (result);

    return result;
}

static void
test_result_types_declared (TestDbFixture *fixture,
                            gconstpointer  user_data)
{
    g_autoptr(OrmResult) result = NULL;
    g_autoptr(GError)    error = NULL;

    g_assert_true (orm_connection_execute (
        fixture->connection,
        "CREATE TABLE typed ("
        "  id INTEGER PRIMARY KEY,"
        "  name VARCHAR(80),"
        "  body TEXT,"
        "  score REAL,"
        "  ratio DOUBLE,"
        "  active BOOLEAN,"
        "  created DATETIME,"
        "  payload BLOB)",
        &error));
    g_assert_no_error (error);

    result = run (fixture, "SELECT id, name, body, score, ratio, active, created, payload FROM typed");

    g_assert_cmpint (orm_result_get_column_count (result), ==, 8);

    g_assert_cmpint (orm_result_get_column_type (result, 0), ==, ORM_VALUE_INTEGER);
    g_assert_cmpint (orm_result_get_column_type (result, 1), ==, ORM_VALUE_STRING);
    g_assert_cmpint (orm_result_get_column_type (result, 2), ==, ORM_VALUE_STRING);
    g_assert_cmpint (orm_result_get_column_type (result, 3), ==, ORM_VALUE_FLOAT);
    g_assert_cmpint (orm_result_get_column_type (result, 4), ==, ORM_VALUE_FLOAT);
    g_assert_cmpint (orm_result_get_column_type (result, 5), ==, ORM_VALUE_BOOLEAN);
    g_assert_cmpint (orm_result_get_column_type (result, 6), ==, ORM_VALUE_DATETIME);
    g_assert_cmpint (orm_result_get_column_type (result, 7), ==, ORM_VALUE_BLOB);

    /* The backend's own spelling comes back verbatim. */
    g_assert_cmpstr (orm_result_get_column_type_name (result, 1), ==, "VARCHAR(80)");
    g_assert_cmpstr (orm_result_get_column_type_name (result, 3), ==, "REAL");
}

/*
 * The whole point: type metadata is available before a single row has
 * been read, and on an empty table there are none to read.
 */
static void
test_result_types_without_rows (TestDbFixture *fixture,
                                gconstpointer  user_data)
{
    g_autoptr(OrmResult) result = NULL;
    g_autoptr(GError)    error = NULL;

    g_assert_true (orm_connection_execute (
        fixture->connection,
        "CREATE TABLE empty_t (id INTEGER, label TEXT)", &error));
    g_assert_no_error (error);

    result = run (fixture, "SELECT id, label FROM empty_t");

    g_assert_cmpint (orm_result_get_column_type (result, 0), ==, ORM_VALUE_INTEGER);
    g_assert_cmpint (orm_result_get_column_type (result, 1), ==, ORM_VALUE_STRING);

    /* Confirm there really were no rows. */
    g_assert_false (orm_result_next (result));
}

/*
 * A NULL in an integer column must not turn the column into "unknown".
 * Reading the type off the value would do exactly that.
 */
static void
test_result_types_null_row_keeps_column_type (TestDbFixture *fixture,
                                              gconstpointer  user_data)
{
    g_autoptr(OrmResult) result = NULL;
    g_autoptr(GError)    error = NULL;
    OrmRow              *row;
    OrmValue            *value;

    g_assert_true (orm_connection_execute (
        fixture->connection,
        "CREATE TABLE nullable_t (n INTEGER)", &error));
    g_assert_no_error (error);
    g_assert_true (orm_connection_execute (
        fixture->connection, "INSERT INTO nullable_t (n) VALUES (NULL)", &error));
    g_assert_no_error (error);

    result = run (fixture, "SELECT n FROM nullable_t");

    g_assert_true (orm_result_next (result));
    row = orm_result_get_row (result);
    value = orm_row_get_value (row, 0);

    /* The value is NULL... */
    g_assert_cmpint (orm_value_get_value_type (value), ==, ORM_VALUE_NULL);
    /* ...and the column is still an integer column. */
    g_assert_cmpint (orm_result_get_column_type (result, 0), ==, ORM_VALUE_INTEGER);
}

/*
 * A computed column has no declared type.  Reporting ORM_VALUE_NULL is
 * the honest answer, and better than a guess that changes with the data.
 */
static void
test_result_types_computed_column (TestDbFixture *fixture,
                                   gconstpointer  user_data)
{
    g_autoptr(OrmResult) result = NULL;

    result = run (fixture, "SELECT COUNT(*) AS n FROM sqlite_master");

    g_assert_cmpint (orm_result_get_column_type (result, 0), ==, ORM_VALUE_NULL);
    g_assert_null (orm_result_get_column_type_name (result, 0));

    /* The column still has a name, and the query still works. */
    g_assert_cmpstr (orm_result_get_column_name (result, 0), ==, "n");
    g_assert_true (orm_result_next (result));
}

/*
 * Out-of-range indices must not read past the field array.
 */
static void
test_result_types_out_of_range (TestDbFixture *fixture,
                                gconstpointer  user_data)
{
    g_autoptr(OrmResult) result = NULL;

    result = run (fixture, "SELECT 1 AS one");

    g_assert_cmpint (orm_result_get_column_type (result, -1), ==, ORM_VALUE_NULL);
    g_assert_cmpint (orm_result_get_column_type (result, 99), ==, ORM_VALUE_NULL);
    g_assert_null (orm_result_get_column_type_name (result, 99));
}

gint
main (gint    argc,
      gchar **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add ("/result-types/declared", TestDbFixture, NULL,
                test_db_fixture_setup, test_result_types_declared,
                test_db_fixture_teardown);
    g_test_add ("/result-types/without-rows", TestDbFixture, NULL,
                test_db_fixture_setup, test_result_types_without_rows,
                test_db_fixture_teardown);
    g_test_add ("/result-types/null-row-keeps-column-type", TestDbFixture, NULL,
                test_db_fixture_setup,
                test_result_types_null_row_keeps_column_type,
                test_db_fixture_teardown);
    g_test_add ("/result-types/computed-column", TestDbFixture, NULL,
                test_db_fixture_setup, test_result_types_computed_column,
                test_db_fixture_teardown);
    g_test_add ("/result-types/out-of-range", TestDbFixture, NULL,
                test_db_fixture_setup, test_result_types_out_of_range,
                test_db_fixture_teardown);

    return g_test_run ();
}
