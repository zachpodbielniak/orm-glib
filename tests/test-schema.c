/* test-schema.c
 *
 * Copyright 2025 Zach Pobiel
 *
 * This file is part of orm-glib.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include <glib-object.h>

#define ORM_INSIDE
#include "orm.h"
#undef ORM_INSIDE

/* ============================================================================
 * OrmMetadata Tests
 * ============================================================================ */

static void
test_metadata_new (void)
{
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();

    g_assert_nonnull (metadata);
    g_assert_true (ORM_IS_METADATA (metadata));
}

static void
test_metadata_add_table (void)
{
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();
    g_autoptr(OrmTable) table = orm_table_new ("users", metadata);

    orm_metadata_add_table (metadata, table);

    OrmTable *found = orm_metadata_get_table (metadata, "users");
    g_assert_nonnull (found);
    g_assert_true (found == table);
}

static void
test_metadata_get_nonexistent_table (void)
{
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();

    OrmTable *table = orm_metadata_get_table (metadata, "nonexistent");
    g_assert_null (table);
}

static void
test_metadata_list_tables (void)
{
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();
    g_autoptr(OrmTable) table1 = orm_table_new ("users", metadata);
    g_autoptr(OrmTable) table2 = orm_table_new ("posts", metadata);

    orm_metadata_add_table (metadata, table1);
    orm_metadata_add_table (metadata, table2);

    /* Note: orm_metadata_get_tables returns internal list (transfer none) */
    GList *tables = orm_metadata_get_tables (metadata);
    g_assert_cmpuint (g_list_length (tables), ==, 2);
    /* Don't free - it's the internal list */
}

/* ============================================================================
 * OrmTable Tests
 * ============================================================================ */

static void
test_table_new (void)
{
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();
    g_autoptr(OrmTable) table = orm_table_new ("users", metadata);

    g_assert_nonnull (table);
    g_assert_true (ORM_IS_TABLE (table));
    g_assert_cmpstr (orm_table_get_name (table), ==, "users");
}

static void
test_table_add_column (void)
{
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();
    g_autoptr(OrmTable) table = orm_table_new ("users", metadata);
    g_autoptr(OrmInteger) int_type = orm_integer_new ();
    g_autoptr(OrmColumn) column = orm_column_new ("id", ORM_SQL_TYPE (int_type));

    orm_table_add_column (table, column);

    OrmColumn *found = orm_table_get_column (table, "id");
    g_assert_nonnull (found);
    g_assert_cmpstr (orm_column_get_name (found), ==, "id");
}

static void
test_table_list_columns (void)
{
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();
    g_autoptr(OrmTable) table = orm_table_new ("users", metadata);

    g_autoptr(OrmInteger) int_type = orm_integer_new ();
    g_autoptr(OrmString) str_type = orm_string_new (100);

    g_autoptr(OrmColumn) col1 = orm_column_new ("id", ORM_SQL_TYPE (int_type));
    g_autoptr(OrmColumn) col2 = orm_column_new ("name", ORM_SQL_TYPE (str_type));

    orm_table_add_column (table, col1);
    orm_table_add_column (table, col2);

    /* Note: returns internal list (transfer none) */
    GList *columns = orm_table_get_columns (table);
    g_assert_cmpuint (g_list_length (columns), ==, 2);
}

static void
test_table_get_nonexistent_column (void)
{
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();
    g_autoptr(OrmTable) table = orm_table_new ("users", metadata);

    OrmColumn *column = orm_table_get_column (table, "nonexistent");
    g_assert_null (column);
}

/* ============================================================================
 * OrmColumn Tests
 * ============================================================================ */

static void
test_column_new (void)
{
    g_autoptr(OrmInteger) int_type = orm_integer_new ();
    g_autoptr(OrmColumn) column = orm_column_new ("id", ORM_SQL_TYPE (int_type));

    g_assert_nonnull (column);
    g_assert_true (ORM_IS_COLUMN (column));
    g_assert_cmpstr (orm_column_get_name (column), ==, "id");
}

static void
test_column_nullable (void)
{
    g_autoptr(OrmInteger) int_type = orm_integer_new ();
    g_autoptr(OrmColumn) column = orm_column_new ("id", ORM_SQL_TYPE (int_type));

    orm_column_set_nullable (column, TRUE);
    g_assert_true (orm_column_get_nullable (column));

    orm_column_set_nullable (column, FALSE);
    g_assert_false (orm_column_get_nullable (column));
}

static void
test_column_default_value (void)
{
    g_autoptr(OrmInteger) int_type = orm_integer_new ();
    g_autoptr(OrmColumn) column = orm_column_new ("status", ORM_SQL_TYPE (int_type));

    /* Default is stored as a string (SQL literal) */
    orm_column_set_default (column, "1");

    const gchar *result = orm_column_get_default (column);
    g_assert_nonnull (result);
    g_assert_cmpstr (result, ==, "1");
}

static void
test_column_unique (void)
{
    g_autoptr(OrmString) str_type = orm_string_new (100);
    g_autoptr(OrmColumn) column = orm_column_new ("email", ORM_SQL_TYPE (str_type));

    orm_column_set_unique (column, TRUE);
    g_assert_true (orm_column_get_unique (column));
}

/* ============================================================================
 * OrmPrimaryKey Tests
 * ============================================================================ */

static void
test_primary_key_single (void)
{
    g_autoptr(OrmPrimaryKey) pk = orm_primary_key_new ("pk_users");

    g_assert_nonnull (pk);
    g_assert_true (ORM_IS_PRIMARY_KEY (pk));
    g_assert_cmpstr (orm_primary_key_get_name (pk), ==, "pk_users");
}

static void
test_primary_key_add_column (void)
{
    g_autoptr(OrmPrimaryKey) pk = orm_primary_key_new ("pk_users");

    orm_primary_key_add_column (pk, "id");

    /* Note: returns internal list (transfer none) */
    GList *columns = orm_primary_key_get_columns (pk);
    g_assert_cmpuint (g_list_length (columns), ==, 1);
    g_assert_cmpstr ((gchar *)columns->data, ==, "id");
}

static void
test_primary_key_composite (void)
{
    g_autoptr(OrmPrimaryKey) pk = orm_primary_key_new ("pk_user_roles");

    orm_primary_key_add_column (pk, "user_id");
    orm_primary_key_add_column (pk, "role_id");

    /* Note: returns internal list (transfer none) */
    GList *columns = orm_primary_key_get_columns (pk);
    g_assert_cmpuint (g_list_length (columns), ==, 2);
}

/* ============================================================================
 * OrmForeignKey Tests
 * ============================================================================ */

static void
test_foreign_key_new (void)
{
    /* Foreign key requires both name and referenced table */
    g_autoptr(OrmForeignKey) fk = orm_foreign_key_new ("fk_posts_user", "users");

    g_assert_nonnull (fk);
    g_assert_true (ORM_IS_FOREIGN_KEY (fk));
    g_assert_cmpstr (orm_foreign_key_get_name (fk), ==, "fk_posts_user");
    g_assert_cmpstr (orm_foreign_key_get_ref_table (fk), ==, "users");
}

static void
test_foreign_key_references (void)
{
    /* Referenced table is specified in constructor */
    g_autoptr(OrmForeignKey) fk = orm_foreign_key_new ("fk_posts_user", "users");

    /* add_column takes local column and referenced column */
    orm_foreign_key_add_column (fk, "user_id", "id");

    g_assert_cmpstr (orm_foreign_key_get_ref_table (fk), ==, "users");

    /* Note: returns internal list (transfer none) */
    GList *ref_cols = orm_foreign_key_get_ref_columns (fk);
    g_assert_cmpuint (g_list_length (ref_cols), ==, 1);
    g_assert_cmpstr ((gchar *)ref_cols->data, ==, "id");

    GList *local_cols = orm_foreign_key_get_local_columns (fk);
    g_assert_cmpuint (g_list_length (local_cols), ==, 1);
    g_assert_cmpstr ((gchar *)local_cols->data, ==, "user_id");
}

static void
test_foreign_key_actions (void)
{
    g_autoptr(OrmForeignKey) fk = orm_foreign_key_new ("fk_posts_user", "users");

    orm_foreign_key_set_on_delete (fk, ORM_FK_CASCADE);
    orm_foreign_key_set_on_update (fk, ORM_FK_SET_NULL);

    g_assert_cmpint (orm_foreign_key_get_on_delete (fk), ==, ORM_FK_CASCADE);
    g_assert_cmpint (orm_foreign_key_get_on_update (fk), ==, ORM_FK_SET_NULL);
}

/* ============================================================================
 * OrmIndex Tests
 * ============================================================================ */

static void
test_index_new (void)
{
    g_autoptr(OrmIndex) index = orm_index_new ("idx_users_email");

    g_assert_nonnull (index);
    g_assert_true (ORM_IS_INDEX (index));
    g_assert_cmpstr (orm_index_get_name (index), ==, "idx_users_email");
}

static void
test_index_add_column (void)
{
    g_autoptr(OrmIndex) index = orm_index_new ("idx_users_email");

    orm_index_add_column (index, "email");

    /* Note: returns internal list (transfer none) */
    GList *columns = orm_index_get_columns (index);
    g_assert_cmpuint (g_list_length (columns), ==, 1);
}

static void
test_index_unique (void)
{
    g_autoptr(OrmIndex) index = orm_index_new ("idx_users_email");

    orm_index_set_unique (index, TRUE);
    g_assert_true (orm_index_get_unique (index));
}

static void
test_index_composite (void)
{
    g_autoptr(OrmIndex) index = orm_index_new ("idx_posts_user_created");

    orm_index_add_column (index, "user_id");
    orm_index_add_column (index, "created_at");

    /* Note: returns internal list (transfer none) */
    GList *columns = orm_index_get_columns (index);
    g_assert_cmpuint (g_list_length (columns), ==, 2);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Metadata tests */
    g_test_add_func ("/schema/metadata/new", test_metadata_new);
    g_test_add_func ("/schema/metadata/add-table", test_metadata_add_table);
    g_test_add_func ("/schema/metadata/get-nonexistent-table", test_metadata_get_nonexistent_table);
    g_test_add_func ("/schema/metadata/list-tables", test_metadata_list_tables);

    /* Table tests */
    g_test_add_func ("/schema/table/new", test_table_new);
    g_test_add_func ("/schema/table/add-column", test_table_add_column);
    g_test_add_func ("/schema/table/list-columns", test_table_list_columns);
    g_test_add_func ("/schema/table/get-nonexistent-column", test_table_get_nonexistent_column);

    /* Column tests */
    g_test_add_func ("/schema/column/new", test_column_new);
    g_test_add_func ("/schema/column/nullable", test_column_nullable);
    g_test_add_func ("/schema/column/default-value", test_column_default_value);
    g_test_add_func ("/schema/column/unique", test_column_unique);

    /* Primary key tests */
    g_test_add_func ("/schema/primary-key/single", test_primary_key_single);
    g_test_add_func ("/schema/primary-key/add-column", test_primary_key_add_column);
    g_test_add_func ("/schema/primary-key/composite", test_primary_key_composite);

    /* Foreign key tests */
    g_test_add_func ("/schema/foreign-key/new", test_foreign_key_new);
    g_test_add_func ("/schema/foreign-key/references", test_foreign_key_references);
    g_test_add_func ("/schema/foreign-key/actions", test_foreign_key_actions);

    /* Index tests */
    g_test_add_func ("/schema/index/new", test_index_new);
    g_test_add_func ("/schema/index/add-column", test_index_add_column);
    g_test_add_func ("/schema/index/unique", test_index_unique);
    g_test_add_func ("/schema/index/composite", test_index_composite);

    return g_test_run ();
}
