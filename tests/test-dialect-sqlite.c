/* test-dialect-sqlite.c
 *
 * Copyright 2025 Zach Pobiel
 *
 * This file is part of orm-glib.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include <glib-object.h>
#include <string.h>

#define ORM_INSIDE
#include "orm.h"
#undef ORM_INSIDE

#include "test-fixtures.h"

#ifdef ORM_ENABLE_SQLITE

/* ============================================================================
 * SQLite Dialect Tests
 * ============================================================================ */

static void
test_sqlite_dialect_type (void)
{
    g_autoptr(OrmSqliteDialect) dialect = orm_sqlite_dialect_new ();

    g_assert_nonnull (dialect);
    g_assert_true (ORM_IS_SQLITE_DIALECT (dialect));
    g_assert_true (ORM_IS_DIALECT (dialect));
}

static void
test_sqlite_dialect_name (void)
{
    g_autoptr(OrmSqliteDialect) dialect = orm_sqlite_dialect_new ();

    const gchar *name = orm_dialect_get_name (ORM_DIALECT (dialect));
    g_assert_cmpstr (name, ==, "SQLite");
}

static void
test_sqlite_paramstyle (void)
{
    g_autoptr(OrmSqliteDialect) dialect = orm_sqlite_dialect_new ();

    const gchar *style = orm_dialect_get_parameter_style (ORM_DIALECT (dialect));
    g_assert_cmpstr (style, ==, "?");
}

static void
test_sqlite_dialect_type_enum (void)
{
    g_autoptr(OrmSqliteDialect) dialect = orm_sqlite_dialect_new ();

    OrmDialectType type = orm_dialect_get_dialect_type (ORM_DIALECT (dialect));
    g_assert_cmpint (type, ==, ORM_DIALECT_SQLITE);
}

static void
test_sqlite_supports_autoincrement (void)
{
    g_autoptr(OrmSqliteDialect) dialect = orm_sqlite_dialect_new ();

    gboolean supports = orm_dialect_supports_autoincrement (ORM_DIALECT (dialect));
    g_assert_true (supports);
}

/* ============================================================================
 * SQLite Type Compiler Tests
 * ============================================================================ */

static void
test_sqlite_type_compiler_integer (void)
{
    g_autoptr(OrmSqliteTypeCompiler) compiler = orm_sqlite_type_compiler_new ();
    g_autoptr(OrmInteger) type = orm_integer_new ();

    g_autofree gchar *sql = orm_type_compiler_compile_type (ORM_TYPE_COMPILER (compiler),
                                                             ORM_SQL_TYPE (type));

    g_assert_cmpstr (sql, ==, "INTEGER");
}

static void
test_sqlite_type_compiler_string (void)
{
    g_autoptr(OrmSqliteTypeCompiler) compiler = orm_sqlite_type_compiler_new ();
    g_autoptr(OrmString) type = orm_string_new (255);

    g_autofree gchar *sql = orm_type_compiler_compile_type (ORM_TYPE_COMPILER (compiler),
                                                             ORM_SQL_TYPE (type));

    g_assert_nonnull (sql);
    /* SQLite may use VARCHAR(255) or TEXT */
    g_assert_true (strstr (sql, "VARCHAR") != NULL || g_strcmp0 (sql, "TEXT") == 0);
}

static void
test_sqlite_type_compiler_text (void)
{
    g_autoptr(OrmSqliteTypeCompiler) compiler = orm_sqlite_type_compiler_new ();
    g_autoptr(OrmText) type = orm_text_new ();

    g_autofree gchar *sql = orm_type_compiler_compile_type (ORM_TYPE_COMPILER (compiler),
                                                             ORM_SQL_TYPE (type));

    g_assert_cmpstr (sql, ==, "TEXT");
}

static void
test_sqlite_type_compiler_boolean (void)
{
    g_autoptr(OrmSqliteTypeCompiler) compiler = orm_sqlite_type_compiler_new ();
    g_autoptr(OrmBooleanType) type = orm_boolean_type_new ();

    g_autofree gchar *sql = orm_type_compiler_compile_type (ORM_TYPE_COMPILER (compiler),
                                                             ORM_SQL_TYPE (type));

    /* SQLite uses INTEGER for boolean */
    g_assert_cmpstr (sql, ==, "INTEGER");
}

static void
test_sqlite_type_compiler_float (void)
{
    g_autoptr(OrmSqliteTypeCompiler) compiler = orm_sqlite_type_compiler_new ();
    g_autoptr(OrmFloatType) type = orm_float_type_new ();

    g_autofree gchar *sql = orm_type_compiler_compile_type (ORM_TYPE_COMPILER (compiler),
                                                             ORM_SQL_TYPE (type));

    g_assert_cmpstr (sql, ==, "REAL");
}

static void
test_sqlite_type_compiler_datetime (void)
{
    g_autoptr(OrmSqliteTypeCompiler) compiler = orm_sqlite_type_compiler_new ();
    g_autoptr(OrmDateTimeType) type = orm_datetime_type_new ();

    g_autofree gchar *sql = orm_type_compiler_compile_type (ORM_TYPE_COMPILER (compiler),
                                                             ORM_SQL_TYPE (type));

    /* SQLite uses TEXT for datetime */
    g_assert_cmpstr (sql, ==, "TEXT");
}

static void
test_sqlite_type_compiler_blob (void)
{
    g_autoptr(OrmSqliteTypeCompiler) compiler = orm_sqlite_type_compiler_new ();
    g_autoptr(OrmBlobType) type = orm_blob_type_new ();

    g_autofree gchar *sql = orm_type_compiler_compile_type (ORM_TYPE_COMPILER (compiler),
                                                             ORM_SQL_TYPE (type));

    g_assert_cmpstr (sql, ==, "BLOB");
}

/* ============================================================================
 * SQLite DDL Compiler Tests
 * ============================================================================ */

static void
test_sqlite_ddl_create_table (void)
{
    g_autoptr(OrmSqliteDdlCompiler) compiler = orm_sqlite_ddl_compiler_new ();
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();
    g_autoptr(OrmTable) table = orm_table_new ("users", metadata);

    g_autoptr(OrmInteger) int_type = orm_integer_new ();
    g_autoptr(OrmString) str_type = orm_string_new (100);

    g_autoptr(OrmColumn) id_col = orm_column_new ("id", ORM_SQL_TYPE (int_type));
    g_autoptr(OrmColumn) name_col = orm_column_new ("name", ORM_SQL_TYPE (str_type));

    orm_column_set_nullable (name_col, FALSE);
    orm_table_add_column (table, id_col);
    orm_table_add_column (table, name_col);

    g_autofree gchar *sql = orm_ddl_compiler_compile_create_table (ORM_DDL_COMPILER (compiler),
                                                                    table, FALSE);

    g_assert_nonnull (sql);
    g_assert_true (strstr (sql, "CREATE TABLE") != NULL);
    g_assert_nonnull (strstr (sql, "users"));
    g_assert_nonnull (strstr (sql, "id"));
    g_assert_nonnull (strstr (sql, "name"));
}

static void
test_sqlite_ddl_create_table_if_not_exists (void)
{
    g_autoptr(OrmSqliteDdlCompiler) compiler = orm_sqlite_ddl_compiler_new ();
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();
    g_autoptr(OrmTable) table = orm_table_new ("users", metadata);

    g_autoptr(OrmInteger) int_type = orm_integer_new ();
    g_autoptr(OrmColumn) id_col = orm_column_new ("id", ORM_SQL_TYPE (int_type));
    orm_table_add_column (table, id_col);

    g_autofree gchar *sql = orm_ddl_compiler_compile_create_table (ORM_DDL_COMPILER (compiler),
                                                                    table, TRUE);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "IF NOT EXISTS"));
}

static void
test_sqlite_ddl_create_table_with_pk (void)
{
    g_autoptr(OrmSqliteDdlCompiler) compiler = orm_sqlite_ddl_compiler_new ();
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();
    g_autoptr(OrmTable) table = orm_table_new ("users", metadata);

    g_autoptr(OrmInteger) int_type = orm_integer_new ();
    g_autoptr(OrmColumn) id_col = orm_column_new ("id", ORM_SQL_TYPE (int_type));

    /* Set column's primary key flag for inline PRIMARY KEY generation */
    orm_column_set_primary_key (id_col, TRUE);
    orm_table_add_column (table, id_col);

    g_autoptr(OrmPrimaryKey) pk = orm_primary_key_new ("pk_users");
    orm_primary_key_add_column (pk, "id");
    orm_table_set_primary_key (table, pk);

    g_autofree gchar *sql = orm_ddl_compiler_compile_create_table (ORM_DDL_COMPILER (compiler),
                                                                    table, FALSE);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "PRIMARY KEY"));
}

static void
test_sqlite_ddl_drop_table (void)
{
    g_autoptr(OrmSqliteDdlCompiler) compiler = orm_sqlite_ddl_compiler_new ();
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();
    g_autoptr(OrmTable) table = orm_table_new ("users", metadata);

    g_autofree gchar *sql = orm_ddl_compiler_compile_drop_table (ORM_DDL_COMPILER (compiler),
                                                                  table, FALSE, FALSE);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "DROP TABLE"));
    g_assert_nonnull (strstr (sql, "users"));
}

static void
test_sqlite_ddl_drop_table_if_exists (void)
{
    g_autoptr(OrmSqliteDdlCompiler) compiler = orm_sqlite_ddl_compiler_new ();
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();
    g_autoptr(OrmTable) table = orm_table_new ("users", metadata);

    g_autofree gchar *sql = orm_ddl_compiler_compile_drop_table (ORM_DDL_COMPILER (compiler),
                                                                  table, TRUE, FALSE);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "IF EXISTS"));
}

static void
test_sqlite_ddl_create_index (void)
{
    g_autoptr(OrmSqliteDdlCompiler) compiler = orm_sqlite_ddl_compiler_new ();
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();
    g_autoptr(OrmTable) table = orm_table_new ("users", metadata);
    g_autoptr(OrmIndex) index = orm_index_new ("idx_users_email");

    orm_index_add_column (index, "email");

    g_autofree gchar *sql = orm_ddl_compiler_compile_create_index (ORM_DDL_COMPILER (compiler),
                                                                    index, table, FALSE);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "CREATE INDEX"));
    g_assert_nonnull (strstr (sql, "idx_users_email"));
}

static void
test_sqlite_ddl_create_unique_index (void)
{
    g_autoptr(OrmSqliteDdlCompiler) compiler = orm_sqlite_ddl_compiler_new ();
    g_autoptr(OrmMetadata) metadata = orm_metadata_new ();
    g_autoptr(OrmTable) table = orm_table_new ("users", metadata);
    g_autoptr(OrmIndex) index = orm_index_new ("idx_users_email");

    orm_index_add_column (index, "email");
    orm_index_set_unique (index, TRUE);

    g_autofree gchar *sql = orm_ddl_compiler_compile_create_index (ORM_DDL_COMPILER (compiler),
                                                                    index, table, FALSE);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "CREATE UNIQUE INDEX"));
}

#endif /* ORM_ENABLE_SQLITE */

/* ============================================================================
 * Main
 * ============================================================================ */

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

#ifdef ORM_ENABLE_SQLITE
    /* Dialect tests */
    g_test_add_func ("/dialect/sqlite/type", test_sqlite_dialect_type);
    g_test_add_func ("/dialect/sqlite/name", test_sqlite_dialect_name);
    g_test_add_func ("/dialect/sqlite/paramstyle", test_sqlite_paramstyle);
    g_test_add_func ("/dialect/sqlite/type-enum", test_sqlite_dialect_type_enum);
    g_test_add_func ("/dialect/sqlite/supports-autoincrement", test_sqlite_supports_autoincrement);

    /* Type compiler tests */
    g_test_add_func ("/dialect/sqlite/type-compiler/integer", test_sqlite_type_compiler_integer);
    g_test_add_func ("/dialect/sqlite/type-compiler/string", test_sqlite_type_compiler_string);
    g_test_add_func ("/dialect/sqlite/type-compiler/text", test_sqlite_type_compiler_text);
    g_test_add_func ("/dialect/sqlite/type-compiler/boolean", test_sqlite_type_compiler_boolean);
    g_test_add_func ("/dialect/sqlite/type-compiler/float", test_sqlite_type_compiler_float);
    g_test_add_func ("/dialect/sqlite/type-compiler/datetime", test_sqlite_type_compiler_datetime);
    g_test_add_func ("/dialect/sqlite/type-compiler/blob", test_sqlite_type_compiler_blob);

    /* DDL compiler tests */
    g_test_add_func ("/dialect/sqlite/ddl/create-table", test_sqlite_ddl_create_table);
    g_test_add_func ("/dialect/sqlite/ddl/create-table-if-not-exists", test_sqlite_ddl_create_table_if_not_exists);
    g_test_add_func ("/dialect/sqlite/ddl/create-table-with-pk", test_sqlite_ddl_create_table_with_pk);
    g_test_add_func ("/dialect/sqlite/ddl/drop-table", test_sqlite_ddl_drop_table);
    g_test_add_func ("/dialect/sqlite/ddl/drop-table-if-exists", test_sqlite_ddl_drop_table_if_exists);
    g_test_add_func ("/dialect/sqlite/ddl/create-index", test_sqlite_ddl_create_index);
    g_test_add_func ("/dialect/sqlite/ddl/create-unique-index", test_sqlite_ddl_create_unique_index);
#else
    g_test_skip ("SQLite support not enabled");
#endif

    return g_test_run ();
}
