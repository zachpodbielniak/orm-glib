/* test-inspector.c
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
 * Schema introspection over SQLite.
 *
 * The fixture schema is built to exercise the cases that are easy to get
 * subtly wrong and impossible to notice without them: a composite
 * primary key declared in an order that differs from table order, a
 * composite foreign key, a foreign key with an implicit target, a view,
 * and a multi-column unique index. A schema of two flat tables would
 * pass with several of those broken.
 *
 * The PostgreSQL and MySQL inspectors are covered by the live-backend
 * suite, since their catalogs cannot be exercised without a server.
 */

#include <glib.h>
#include <glib-object.h>

#include "test-fixtures.h"

typedef struct
{
    OrmEngine     *engine;
    OrmConnection *connection;
    OrmInspector  *inspector;
} InspectFixture;

static void
inspect_exec (InspectFixture *fixture,
              const gchar    *sql)
{
    g_autoptr(GError) error = NULL;

    g_assert_true (orm_connection_execute (fixture->connection, sql, &error));
    g_assert_no_error (error);
}

static void
inspect_fixture_setup (InspectFixture *fixture,
                       gconstpointer   user_data)
{
    g_autoptr(GError) error = NULL;

    fixture->engine = orm_engine_new ("sqlite:///:memory:", &error);
    g_assert_no_error (error);

    fixture->connection = orm_engine_connect (fixture->engine, &error);
    g_assert_no_error (error);

    inspect_exec (fixture,
                  "CREATE TABLE authors ("
                  "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                  "  name VARCHAR(80) NOT NULL,"
                  "  bio TEXT,"
                  "  rating REAL DEFAULT 0.0)");

    /*
     * The primary key is declared (region, code) while the columns are
     * declared (code, region) -- so key order and table order disagree,
     * which is the whole point of having this table.
     */
    inspect_exec (fixture,
                  "CREATE TABLE regions ("
                  "  code VARCHAR(8) NOT NULL,"
                  "  region VARCHAR(8) NOT NULL,"
                  "  label TEXT,"
                  "  PRIMARY KEY (region, code))");

    /* A composite foreign key into that composite primary key. */
    inspect_exec (fixture,
                  "CREATE TABLE offices ("
                  "  id INTEGER PRIMARY KEY,"
                  "  region VARCHAR(8),"
                  "  code VARCHAR(8),"
                  "  FOREIGN KEY (region, code)"
                  "    REFERENCES regions (region, code) ON DELETE CASCADE)");

    /* A single-column foreign key with no explicit target column. */
    inspect_exec (fixture,
                  "CREATE TABLE books ("
                  "  id INTEGER PRIMARY KEY,"
                  "  author_id INTEGER REFERENCES authors,"
                  "  title TEXT NOT NULL)");

    inspect_exec (fixture,
                  "CREATE UNIQUE INDEX idx_books_author_title"
                  "  ON books (author_id, title)");
    inspect_exec (fixture, "CREATE INDEX idx_books_title ON books (title)");

    inspect_exec (fixture,
                  "CREATE VIEW recent_books AS SELECT id, title FROM books");

    fixture->inspector = orm_inspector_new (fixture->connection, &error);
    g_assert_no_error (error);
    g_assert_nonnull (fixture->inspector);
}

static void
inspect_fixture_teardown (InspectFixture *fixture,
                          gconstpointer   user_data)
{
    g_clear_object (&fixture->inspector);
    g_clear_object (&fixture->connection);
    g_clear_object (&fixture->engine);
}

/* Finds a named record in an array of OrmColumnInfo. */
static OrmColumnInfo *
find_column (GPtrArray   *columns,
             const gchar *name)
{
    guint i;

    for (i = 0; i < columns->len; i++)
    {
        OrmColumnInfo *info = g_ptr_array_index (columns, i);

        if (g_strcmp0 (orm_column_info_get_name (info), name) == 0)
            return info;
    }

    return NULL;
}

static gboolean
strv_contains (gchar      **strv,
               const gchar *needle)
{
    return strv != NULL && g_strv_contains ((const gchar * const *) strv, needle);
}

static void
test_inspector_list_relations (InspectFixture *fixture,
                               gconstpointer   user_data)
{
    g_autoptr(GPtrArray) relations = NULL;
    g_autoptr(GError)    error = NULL;
    g_auto(GStrv)        tables = NULL;
    g_auto(GStrv)        views = NULL;

    relations = orm_inspector_list_relations (fixture->inspector, NULL, &error);
    g_assert_no_error (error);
    g_assert_nonnull (relations);

    /* Four tables and one view; sqlite_* internals must not appear. */
    g_assert_cmpuint (relations->len, ==, 5);

    tables = orm_inspector_list_tables (fixture->inspector, NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpuint (g_strv_length (tables), ==, 4);
    g_assert_true (strv_contains (tables, "authors"));
    g_assert_true (strv_contains (tables, "books"));
    g_assert_false (strv_contains (tables, "recent_books"));

    views = orm_inspector_list_views (fixture->inspector, NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpuint (g_strv_length (views), ==, 1);
    g_assert_cmpstr (views[0], ==, "recent_books");
}

static void
test_inspector_has_table (InspectFixture *fixture,
                          gconstpointer   user_data)
{
    g_autoptr(GError) error = NULL;

    g_assert_true (orm_inspector_has_table (fixture->inspector, "authors",
                                            NULL, &error));
    g_assert_no_error (error);
    g_assert_false (orm_inspector_has_table (fixture->inspector, "nope",
                                             NULL, &error));
    g_assert_no_error (error);
}

static void
test_inspector_columns (InspectFixture *fixture,
                        gconstpointer   user_data)
{
    g_autoptr(GPtrArray) columns = NULL;
    g_autoptr(GError)    error = NULL;
    OrmColumnInfo       *id;
    OrmColumnInfo       *name;
    OrmColumnInfo       *bio;
    OrmColumnInfo       *rating;

    columns = orm_inspector_get_columns (fixture->inspector, "authors", NULL,
                                         &error);
    g_assert_no_error (error);
    g_assert_cmpuint (columns->len, ==, 4);

    id = find_column (columns, "id");
    g_assert_nonnull (id);
    g_assert_cmpint (orm_column_info_get_ordinal (id), ==, 0);
    g_assert_true (orm_column_info_get_primary_key (id));
    g_assert_true (orm_column_info_get_autoincrement (id));
    g_assert_cmpint (orm_column_info_get_value_type (id), ==, ORM_VALUE_INTEGER);

    name = find_column (columns, "name");
    g_assert_nonnull (name);
    g_assert_false (orm_column_info_get_nullable (name));
    g_assert_false (orm_column_info_get_primary_key (name));
    g_assert_cmpstr (orm_column_info_get_type_name (name), ==, "VARCHAR(80)");
    g_assert_cmpint (orm_column_info_get_value_type (name), ==, ORM_VALUE_STRING);

    bio = find_column (columns, "bio");
    g_assert_nonnull (bio);
    g_assert_true (orm_column_info_get_nullable (bio));
    g_assert_null (orm_column_info_get_default_value (bio));

    rating = find_column (columns, "rating");
    g_assert_nonnull (rating);
    g_assert_cmpint (orm_column_info_get_value_type (rating), ==, ORM_VALUE_FLOAT);
    g_assert_nonnull (orm_column_info_get_default_value (rating));
}

/*
 * Key order, not table order.  regions declares its columns
 * (code, region) and its key (region, code); reporting the key in table
 * order would silently produce a WHERE clause that matches the wrong row.
 */
static void
test_inspector_primary_key_order (InspectFixture *fixture,
                                  gconstpointer   user_data)
{
    g_auto(GStrv)     pk = NULL;
    g_autoptr(GError) error = NULL;

    pk = orm_inspector_get_primary_key (fixture->inspector, "regions", NULL,
                                        &error);
    g_assert_no_error (error);
    g_assert_cmpuint (g_strv_length (pk), ==, 2);
    g_assert_cmpstr (pk[0], ==, "region");
    g_assert_cmpstr (pk[1], ==, "code");
}

/*
 * A table with no primary key must say so plainly: an editor has to
 * refuse to update rows it cannot uniquely name.
 */
static void
test_inspector_primary_key_absent (InspectFixture *fixture,
                                   gconstpointer   user_data)
{
    g_auto(GStrv)     pk = NULL;
    g_autoptr(GError) error = NULL;

    inspect_exec (fixture, "CREATE TABLE keyless (a INTEGER, b TEXT)");

    pk = orm_inspector_get_primary_key (fixture->inspector, "keyless", NULL,
                                        &error);
    g_assert_no_error (error);
    g_assert_nonnull (pk);
    g_assert_cmpuint (g_strv_length (pk), ==, 0);
}

static void
test_inspector_indexes (InspectFixture *fixture,
                        gconstpointer   user_data)
{
    g_autoptr(GPtrArray) indexes = NULL;
    g_autoptr(GError)    error = NULL;
    OrmIndexInfo        *composite = NULL;
    guint                i;

    indexes = orm_inspector_get_indexes (fixture->inspector, "books", NULL,
                                         &error);
    g_assert_no_error (error);
    g_assert_cmpuint (indexes->len, >=, 2);

    for (i = 0; i < indexes->len; i++)
    {
        OrmIndexInfo *info = g_ptr_array_index (indexes, i);

        if (g_strcmp0 (orm_index_info_get_name (info),
                       "idx_books_author_title") == 0)
            composite = info;
    }

    g_assert_nonnull (composite);
    g_assert_true (orm_index_info_get_unique (composite));

    /* Column order is part of what an index is. */
    g_assert_cmpstr (orm_index_info_get_columns (composite)[0], ==, "author_id");
    g_assert_cmpstr (orm_index_info_get_columns (composite)[1], ==, "title");
    g_assert_null (orm_index_info_get_columns (composite)[2]);
}

/*
 * The two foreign keys on offices and books between them cover composite
 * grouping, action mapping, and the implicit target column.
 */
static void
test_inspector_foreign_keys_composite (InspectFixture *fixture,
                                       gconstpointer   user_data)
{
    g_autoptr(GPtrArray) fks = NULL;
    g_autoptr(GError)    error = NULL;
    OrmForeignKeyInfo   *fk;

    fks = orm_inspector_get_foreign_keys (fixture->inspector, "offices", NULL,
                                          &error);
    g_assert_no_error (error);
    g_assert_cmpuint (fks->len, ==, 1);

    fk = g_ptr_array_index (fks, 0);
    g_assert_cmpstr (orm_foreign_key_info_get_ref_table (fk), ==, "regions");
    g_assert_cmpint (orm_foreign_key_info_get_on_delete (fk), ==, ORM_FK_CASCADE);

    /* Both column lists carry two entries, paired positionally. */
    g_assert_cmpuint (g_strv_length ((gchar **) orm_foreign_key_info_get_columns (fk)),
                      ==, 2);
    g_assert_cmpuint (g_strv_length ((gchar **) orm_foreign_key_info_get_ref_columns (fk)),
                      ==, 2);
}

static void
test_inspector_foreign_key_implicit_target (InspectFixture *fixture,
                                            gconstpointer   user_data)
{
    g_autoptr(GPtrArray) fks = NULL;
    g_autoptr(GError)    error = NULL;
    OrmForeignKeyInfo   *fk;

    fks = orm_inspector_get_foreign_keys (fixture->inspector, "books", NULL,
                                          &error);
    g_assert_no_error (error);
    g_assert_cmpuint (fks->len, ==, 1);

    fk = g_ptr_array_index (fks, 0);
    g_assert_cmpstr (orm_foreign_key_info_get_ref_table (fk), ==, "authors");
    g_assert_cmpstr (orm_foreign_key_info_get_columns (fk)[0], ==, "author_id");

    /*
     * "REFERENCES authors" names no column, meaning the target's primary
     * key -- which the inspector has to look up rather than report NULL.
     */
    g_assert_cmpstr (orm_foreign_key_info_get_ref_columns (fk)[0], ==, "id");
}

static void
test_inspector_row_count (InspectFixture *fixture,
                          gconstpointer   user_data)
{
    g_autoptr(GError) error = NULL;
    gboolean          is_estimate = TRUE;
    gint64            count;

    inspect_exec (fixture,
                  "INSERT INTO authors (name) VALUES ('a'), ('b'), ('c')");

    count = orm_inspector_estimate_row_count (fixture->inspector, "authors",
                                              NULL, &is_estimate, &error);
    g_assert_no_error (error);
    g_assert_cmpint (count, ==, 3);

    /* SQLite keeps no statistics, so the count is exact. */
    g_assert_false (is_estimate);
}

/*
 * A name containing a quote character must be readable like any other,
 * and must not execute.
 */
static void
test_inspector_hostile_table_name (InspectFixture *fixture,
                                   gconstpointer   user_data)
{
    g_autoptr(GPtrArray) columns = NULL;
    g_autoptr(GError)    error = NULL;

    inspect_exec (fixture, "CREATE TABLE \"ev\"\"il\" (n INTEGER)");

    g_assert_true (orm_inspector_has_table (fixture->inspector, "ev\"il",
                                            NULL, &error));
    g_assert_no_error (error);

    columns = orm_inspector_get_columns (fixture->inspector, "ev\"il", NULL,
                                         &error);
    g_assert_no_error (error);
    g_assert_cmpuint (columns->len, ==, 1);
    g_assert_cmpstr (orm_column_info_get_name (g_ptr_array_index (columns, 0)),
                     ==, "n");
}

static void
test_inspector_missing_table (InspectFixture *fixture,
                              gconstpointer   user_data)
{
    g_autoptr(GPtrArray) columns = NULL;
    g_autoptr(GError)    error = NULL;

    /* No such table: an empty column list, not a crash. */
    columns = orm_inspector_get_columns (fixture->inspector, "does_not_exist",
                                         NULL, &error);
    if (columns != NULL)
        g_assert_cmpuint (columns->len, ==, 0);
}

static void
test_inspector_list_schemas (InspectFixture *fixture,
                             gconstpointer   user_data)
{
    g_auto(GStrv)     schemas = NULL;
    g_autoptr(GError) error = NULL;

    schemas = orm_inspector_list_schemas (fixture->inspector, &error);
    g_assert_no_error (error);
    g_assert_nonnull (schemas);
    g_assert_true (strv_contains (schemas, "main"));
}

#define ADD(path, fn) \
    g_test_add (path, InspectFixture, NULL, inspect_fixture_setup, fn, \
                inspect_fixture_teardown)

gint
main (gint    argc,
      gchar **argv)
{
    g_test_init (&argc, &argv, NULL);

    ADD ("/inspector/list-relations", test_inspector_list_relations);
    ADD ("/inspector/has-table", test_inspector_has_table);
    ADD ("/inspector/columns", test_inspector_columns);
    ADD ("/inspector/primary-key/order", test_inspector_primary_key_order);
    ADD ("/inspector/primary-key/absent", test_inspector_primary_key_absent);
    ADD ("/inspector/indexes", test_inspector_indexes);
    ADD ("/inspector/foreign-keys/composite",
         test_inspector_foreign_keys_composite);
    ADD ("/inspector/foreign-keys/implicit-target",
         test_inspector_foreign_key_implicit_target);
    ADD ("/inspector/row-count", test_inspector_row_count);
    ADD ("/inspector/hostile-table-name", test_inspector_hostile_table_name);
    ADD ("/inspector/missing-table", test_inspector_missing_table);
    ADD ("/inspector/list-schemas", test_inspector_list_schemas);

    return g_test_run ();
}
