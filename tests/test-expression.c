/* test-expression.c
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

/* ============================================================================
 * OrmExpression Tests
 * ============================================================================ */

static void
test_expression_is_abstract (void)
{
    /* OrmExpression is abstract, we test its subclasses */
    g_assert_true (g_type_is_a (ORM_TYPE_LITERAL, ORM_TYPE_EXPRESSION));
    g_assert_true (g_type_is_a (ORM_TYPE_COLUMN_ELEMENT, ORM_TYPE_EXPRESSION));
    g_assert_true (g_type_is_a (ORM_TYPE_BINARY_EXPRESSION, ORM_TYPE_EXPRESSION));
}

/* ============================================================================
 * OrmLiteral Tests
 * ============================================================================ */

static void
test_literal_integer (void)
{
    OrmValue *value = orm_value_new_integer (42);
    g_autoptr(OrmLiteral) literal = orm_literal_new (value);  /* takes ownership */

    g_assert_nonnull (literal);
    g_assert_true (ORM_IS_LITERAL (literal));
    g_assert_true (ORM_IS_EXPRESSION (literal));

    OrmValue *result = orm_literal_get_value (literal);
    g_assert_cmpint (orm_value_get_integer (result), ==, 42);
}

static void
test_literal_string (void)
{
    OrmValue *value = orm_value_new_string ("hello");
    g_autoptr(OrmLiteral) literal = orm_literal_new (value);  /* takes ownership */

    OrmValue *result = orm_literal_get_value (literal);
    g_assert_cmpstr (orm_value_get_string (result), ==, "hello");
}

static void
test_literal_null (void)
{
    OrmValue *value = orm_value_new_null ();
    g_autoptr(OrmLiteral) literal = orm_literal_new (value);  /* takes ownership */

    OrmValue *result = orm_literal_get_value (literal);
    g_assert_true (orm_value_is_null (result));
}

static void
test_literal_compile (void)
{
    OrmValue *value = orm_value_new_integer (42);
    g_autoptr(OrmLiteral) literal = orm_literal_new (value);  /* takes ownership */
    GList *params = NULL;

    g_autofree gchar *sql = orm_expression_compile (ORM_EXPRESSION (literal),
                                                     ORM_DIALECT_SQLITE,
                                                     &params);

    g_assert_nonnull (sql);
    /* Literal should compile to a placeholder or the value itself */
    g_assert_true (g_strcmp0 (sql, "?") == 0 || g_strcmp0 (sql, "42") == 0);

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

/* ============================================================================
 * OrmColumnElement Tests
 * ============================================================================ */

static void
test_column_element_new (void)
{
    g_autoptr(OrmColumnElement) col = orm_column_element_new_with_table ("users", "id");

    g_assert_nonnull (col);
    g_assert_true (ORM_IS_COLUMN_ELEMENT (col));
    g_assert_true (ORM_IS_EXPRESSION (col));
}

static void
test_column_element_table_and_name (void)
{
    g_autoptr(OrmColumnElement) col = orm_column_element_new_with_table ("users", "id");

    g_assert_cmpstr (orm_column_element_get_table (col), ==, "users");
    g_assert_cmpstr (orm_column_element_get_name (col), ==, "id");
}

static void
test_column_element_without_table (void)
{
    g_autoptr(OrmColumnElement) col = orm_column_element_new ("id");

    g_assert_null (orm_column_element_get_table (col));
    g_assert_cmpstr (orm_column_element_get_name (col), ==, "id");
}

static void
test_column_element_compile (void)
{
    g_autoptr(OrmColumnElement) col = orm_column_element_new_with_table ("users", "id");
    GList *params = NULL;

    g_autofree gchar *sql = orm_expression_compile (ORM_EXPRESSION (col),
                                                     ORM_DIALECT_SQLITE,
                                                     &params);

    g_assert_nonnull (sql);
    /* Should compile to "users"."id" or similar */
    g_assert_nonnull (strstr (sql, "id"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_column_element_compile_no_table (void)
{
    g_autoptr(OrmColumnElement) col = orm_column_element_new ("name");
    GList *params = NULL;

    g_autofree gchar *sql = orm_expression_compile (ORM_EXPRESSION (col),
                                                     ORM_DIALECT_SQLITE,
                                                     &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "name"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

/* ============================================================================
 * OrmBinaryExpression Tests
 * ============================================================================ */

static void
test_binary_expression_eq (void)
{
    /* Binary expression takes ownership of left and right expressions */
    OrmColumnElement *col = orm_column_element_new ("id");
    OrmValue *value = orm_value_new_integer (42);
    OrmLiteral *literal = orm_literal_new (value);

    g_autoptr(OrmBinaryExpression) expr = orm_binary_expression_new_compare (
        ORM_EXPRESSION (col),
        ORM_OP_EQ,
        ORM_EXPRESSION (literal));

    g_assert_nonnull (expr);
    g_assert_true (ORM_IS_BINARY_EXPRESSION (expr));
    g_assert_true (ORM_IS_EXPRESSION (expr));
}

static void
test_binary_expression_compile_eq (void)
{
    /* Binary expression takes ownership of left and right expressions */
    OrmColumnElement *col = orm_column_element_new ("id");
    OrmValue *value = orm_value_new_integer (42);
    OrmLiteral *literal = orm_literal_new (value);
    GList *params = NULL;

    g_autoptr(OrmBinaryExpression) expr = orm_binary_expression_new_compare (
        ORM_EXPRESSION (col),
        ORM_OP_EQ,
        ORM_EXPRESSION (literal));

    g_autofree gchar *sql = orm_expression_compile (ORM_EXPRESSION (expr),
                                                     ORM_DIALECT_SQLITE,
                                                     &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "="));
    g_assert_nonnull (strstr (sql, "id"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_binary_expression_compile_lt (void)
{
    /* Binary expression takes ownership of left and right expressions */
    OrmColumnElement *col = orm_column_element_new ("age");
    OrmValue *value = orm_value_new_integer (18);
    OrmLiteral *literal = orm_literal_new (value);
    GList *params = NULL;

    g_autoptr(OrmBinaryExpression) expr = orm_binary_expression_new_compare (
        ORM_EXPRESSION (col),
        ORM_OP_LT,
        ORM_EXPRESSION (literal));

    g_autofree gchar *sql = orm_expression_compile (ORM_EXPRESSION (expr),
                                                     ORM_DIALECT_SQLITE,
                                                     &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "<"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_binary_expression_compile_like (void)
{
    /* Binary expression takes ownership of left and right expressions */
    OrmColumnElement *col = orm_column_element_new ("name");
    OrmValue *value = orm_value_new_string ("%alice%");
    OrmLiteral *literal = orm_literal_new (value);
    GList *params = NULL;

    g_autoptr(OrmBinaryExpression) expr = orm_binary_expression_new_compare (
        ORM_EXPRESSION (col),
        ORM_OP_LIKE,
        ORM_EXPRESSION (literal));

    g_autofree gchar *sql = orm_expression_compile (ORM_EXPRESSION (expr),
                                                     ORM_DIALECT_SQLITE,
                                                     &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "LIKE"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_binary_expression_is_null (void)
{
    /* Binary expression takes ownership of left expression */
    OrmColumnElement *col = orm_column_element_new ("deleted_at");
    GList *params = NULL;

    g_autoptr(OrmBinaryExpression) expr = orm_binary_expression_new_compare (
        ORM_EXPRESSION (col),
        ORM_OP_IS_NULL,
        NULL);

    g_autofree gchar *sql = orm_expression_compile (ORM_EXPRESSION (expr),
                                                     ORM_DIALECT_SQLITE,
                                                     &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "IS NULL"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_binary_expression_get_compare_op (void)
{
    /* Binary expression takes ownership of left and right expressions */
    OrmColumnElement *col = orm_column_element_new ("id");
    OrmValue *value = orm_value_new_integer (42);
    OrmLiteral *literal = orm_literal_new (value);

    g_autoptr(OrmBinaryExpression) expr = orm_binary_expression_new_compare (
        ORM_EXPRESSION (col),
        ORM_OP_GE,
        ORM_EXPRESSION (literal));

    g_assert_true (orm_binary_expression_is_comparison (expr));
    g_assert_cmpint (orm_binary_expression_get_compare_op (expr), ==, ORM_OP_GE);
}

/* ============================================================================
 * Expression Combination Tests
 * ============================================================================ */

static void
test_expression_and (void)
{
    /* Binary expressions take ownership of all their sub-expressions */
    OrmColumnElement *col1 = orm_column_element_new ("active");
    OrmValue *value1 = orm_value_new_boolean (TRUE);
    OrmLiteral *literal1 = orm_literal_new (value1);

    OrmColumnElement *col2 = orm_column_element_new ("age");
    OrmValue *value2 = orm_value_new_integer (18);
    OrmLiteral *literal2 = orm_literal_new (value2);

    OrmBinaryExpression *expr1 = orm_binary_expression_new_compare (
        ORM_EXPRESSION (col1), ORM_OP_EQ, ORM_EXPRESSION (literal1));
    OrmBinaryExpression *expr2 = orm_binary_expression_new_compare (
        ORM_EXPRESSION (col2), ORM_OP_GE, ORM_EXPRESSION (literal2));

    /* Combine with AND - takes ownership of expr1 and expr2 */
    g_autoptr(OrmBinaryExpression) combined = orm_binary_expression_new_logical (
        ORM_EXPRESSION (expr1), ORM_LOGICAL_AND, ORM_EXPRESSION (expr2));

    g_assert_nonnull (combined);
    g_assert_true (orm_binary_expression_is_logical (combined));
    g_assert_cmpint (orm_binary_expression_get_logical_op (combined), ==, ORM_LOGICAL_AND);
}

static void
test_expression_or (void)
{
    /* Binary expressions take ownership of all their sub-expressions */
    OrmColumnElement *col1 = orm_column_element_new ("status");
    OrmValue *value1 = orm_value_new_string ("active");
    OrmLiteral *literal1 = orm_literal_new (value1);

    OrmColumnElement *col2 = orm_column_element_new ("status");
    OrmValue *value2 = orm_value_new_string ("pending");
    OrmLiteral *literal2 = orm_literal_new (value2);

    OrmBinaryExpression *expr1 = orm_binary_expression_new_compare (
        ORM_EXPRESSION (col1), ORM_OP_EQ, ORM_EXPRESSION (literal1));
    OrmBinaryExpression *expr2 = orm_binary_expression_new_compare (
        ORM_EXPRESSION (col2), ORM_OP_EQ, ORM_EXPRESSION (literal2));

    /* Combine with OR - takes ownership of expr1 and expr2 */
    g_autoptr(OrmBinaryExpression) combined = orm_binary_expression_new_logical (
        ORM_EXPRESSION (expr1), ORM_LOGICAL_OR, ORM_EXPRESSION (expr2));

    g_assert_nonnull (combined);
    g_assert_true (orm_binary_expression_is_logical (combined));
    g_assert_cmpint (orm_binary_expression_get_logical_op (combined), ==, ORM_LOGICAL_OR);
}

static void
test_expression_not (void)
{
    /* Binary expressions take ownership of all their sub-expressions */
    OrmColumnElement *col = orm_column_element_new ("deleted");
    OrmValue *value = orm_value_new_boolean (TRUE);
    OrmLiteral *literal = orm_literal_new (value);

    OrmBinaryExpression *expr = orm_binary_expression_new_compare (
        ORM_EXPRESSION (col), ORM_OP_EQ, ORM_EXPRESSION (literal));

    /* Negate the expression - takes ownership of expr */
    g_autoptr(OrmBinaryExpression) negated = orm_binary_expression_new_not (
        ORM_EXPRESSION (expr));

    g_assert_nonnull (negated);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Expression base tests */
    g_test_add_func ("/expression/is-abstract", test_expression_is_abstract);

    /* Literal tests */
    g_test_add_func ("/expression/literal/integer", test_literal_integer);
    g_test_add_func ("/expression/literal/string", test_literal_string);
    g_test_add_func ("/expression/literal/null", test_literal_null);
    g_test_add_func ("/expression/literal/compile", test_literal_compile);

    /* Column element tests */
    g_test_add_func ("/expression/column/new", test_column_element_new);
    g_test_add_func ("/expression/column/table-and-name", test_column_element_table_and_name);
    g_test_add_func ("/expression/column/without-table", test_column_element_without_table);
    g_test_add_func ("/expression/column/compile", test_column_element_compile);
    g_test_add_func ("/expression/column/compile-no-table", test_column_element_compile_no_table);

    /* Binary expression tests */
    g_test_add_func ("/expression/binary/eq", test_binary_expression_eq);
    g_test_add_func ("/expression/binary/compile-eq", test_binary_expression_compile_eq);
    g_test_add_func ("/expression/binary/compile-lt", test_binary_expression_compile_lt);
    g_test_add_func ("/expression/binary/compile-like", test_binary_expression_compile_like);
    g_test_add_func ("/expression/binary/is-null", test_binary_expression_is_null);
    g_test_add_func ("/expression/binary/get-compare-op", test_binary_expression_get_compare_op);

    /* Combination tests */
    g_test_add_func ("/expression/combination/and", test_expression_and);
    g_test_add_func ("/expression/combination/or", test_expression_or);
    g_test_add_func ("/expression/combination/not", test_expression_not);

    return g_test_run ();
}
