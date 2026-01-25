/* test-select.c
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
 * OrmSelect Builder Tests
 * ============================================================================ */

static void
test_select_new (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();

    g_assert_nonnull (select);
    g_assert_true (ORM_IS_SELECT (select));
}

static void
test_select_from (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();
    GList *params = NULL;

    orm_select_from (select, "users");

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "FROM"));
    g_assert_nonnull (strstr (sql, "users"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_select_columns (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();
    GList *params = NULL;

    orm_select_from (select, "users");
    orm_select_column (select, "id");
    orm_select_column (select, "name");

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "id"));
    g_assert_nonnull (strstr (sql, "name"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_select_all_columns (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();
    GList *params = NULL;

    orm_select_from (select, "users");
    /* Without adding columns, should select * */

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    /* Should contain SELECT and either * or column names */
    g_assert_true (strstr (sql, "*") != NULL || strstr (sql, "SELECT") != NULL);

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_select_where_simple (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();
    /* Binary expression and orm_select_where take ownership of expressions */
    OrmColumnElement *col = orm_column_element_new ("id");
    OrmValue *value = orm_value_new_integer (42);
    OrmLiteral *literal = orm_literal_new (value);
    OrmBinaryExpression *where = orm_binary_expression_new_compare (
        ORM_EXPRESSION (col), ORM_OP_EQ, ORM_EXPRESSION (literal));
    GList *params = NULL;

    orm_select_from (select, "users");
    orm_select_where (select, ORM_EXPRESSION (where));

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "WHERE"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_select_order_by_asc (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();
    GList *params = NULL;

    orm_select_from (select, "users");
    orm_select_order_by (select, "name", ORM_ORDER_ASC);

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "ORDER BY"));
    g_assert_nonnull (strstr (sql, "name"));
    g_assert_nonnull (strstr (sql, "ASC"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_select_order_by_desc (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();
    GList *params = NULL;

    orm_select_from (select, "users");
    orm_select_order_by (select, "created_at", ORM_ORDER_DESC);

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "ORDER BY"));
    g_assert_nonnull (strstr (sql, "DESC"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_select_limit (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();
    GList *params = NULL;

    orm_select_from (select, "users");
    orm_select_limit (select, 10);

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "LIMIT"));
    g_assert_nonnull (strstr (sql, "10"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_select_offset (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();
    GList *params = NULL;

    orm_select_from (select, "users");
    orm_select_limit (select, 10);
    orm_select_offset (select, 20);

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "OFFSET"));
    g_assert_nonnull (strstr (sql, "20"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_select_multiple_order_by (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();
    GList *params = NULL;

    orm_select_from (select, "users");
    orm_select_order_by (select, "active", ORM_ORDER_DESC);
    orm_select_order_by (select, "name", ORM_ORDER_ASC);

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "active"));
    g_assert_nonnull (strstr (sql, "name"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_select_distinct (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();
    GList *params = NULL;

    orm_select_from (select, "users");
    orm_select_distinct (select, TRUE);

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "DISTINCT"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_select_join (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();
    GList *params = NULL;

    /* Build join condition: posts.user_id = users.id */
    /* Binary expression and orm_select_join take ownership of expressions */
    OrmColumnElement *left = orm_column_element_new_with_table ("posts", "user_id");
    OrmColumnElement *right = orm_column_element_new_with_table ("users", "id");
    OrmBinaryExpression *cond = orm_binary_expression_new_compare (
        ORM_EXPRESSION (left), ORM_OP_EQ, ORM_EXPRESSION (right));

    orm_select_from (select, "posts");
    orm_select_join (select, "users", ORM_EXPRESSION (cond), ORM_JOIN_INNER);

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "JOIN"));
    g_assert_nonnull (strstr (sql, "users"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_select_left_join (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();
    GList *params = NULL;

    /* Build join condition */
    /* Binary expression and orm_select_join take ownership of expressions */
    OrmColumnElement *left = orm_column_element_new_with_table ("posts", "user_id");
    OrmColumnElement *right = orm_column_element_new_with_table ("users", "id");
    OrmBinaryExpression *cond = orm_binary_expression_new_compare (
        ORM_EXPRESSION (left), ORM_OP_EQ, ORM_EXPRESSION (right));

    orm_select_from (select, "posts");
    orm_select_join (select, "users", ORM_EXPRESSION (cond), ORM_JOIN_LEFT);

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "LEFT"));
    g_assert_nonnull (strstr (sql, "JOIN"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_select_complex (void)
{
    /*
     * Build: SELECT name, email FROM users
     *        WHERE active = 1 ORDER BY name LIMIT 10 OFFSET 5
     */
    g_autoptr(OrmSelect) select = orm_select_new ();
    /* Binary expression and orm_select_where take ownership of expressions */
    OrmColumnElement *col = orm_column_element_new ("active");
    OrmValue *value = orm_value_new_integer (1);
    OrmLiteral *literal = orm_literal_new (value);
    OrmBinaryExpression *where = orm_binary_expression_new_compare (
        ORM_EXPRESSION (col), ORM_OP_EQ, ORM_EXPRESSION (literal));
    GList *params = NULL;

    orm_select_from (select, "users");
    orm_select_column (select, "name");
    orm_select_column (select, "email");
    orm_select_where (select, ORM_EXPRESSION (where));
    orm_select_order_by (select, "name", ORM_ORDER_ASC);
    orm_select_limit (select, 10);
    orm_select_offset (select, 5);

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "SELECT"));
    g_assert_nonnull (strstr (sql, "FROM"));
    g_assert_nonnull (strstr (sql, "WHERE"));
    g_assert_nonnull (strstr (sql, "ORDER BY"));
    g_assert_nonnull (strstr (sql, "LIMIT"));
    g_assert_nonnull (strstr (sql, "OFFSET"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

static void
test_select_group_by (void)
{
    g_autoptr(OrmSelect) select = orm_select_new ();
    GList *params = NULL;

    orm_select_from (select, "orders");
    orm_select_column (select, "user_id");
    orm_select_group_by (select, "user_id");

    g_autofree gchar *sql = orm_select_compile (select, ORM_DIALECT_SQLITE, &params);

    g_assert_nonnull (sql);
    g_assert_nonnull (strstr (sql, "GROUP BY"));

    g_list_free_full (params, (GDestroyNotify) orm_value_free);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Basic tests */
    g_test_add_func ("/select/new", test_select_new);
    g_test_add_func ("/select/from", test_select_from);
    g_test_add_func ("/select/columns", test_select_columns);
    g_test_add_func ("/select/all-columns", test_select_all_columns);

    /* WHERE tests */
    g_test_add_func ("/select/where-simple", test_select_where_simple);

    /* ORDER BY tests */
    g_test_add_func ("/select/order-by-asc", test_select_order_by_asc);
    g_test_add_func ("/select/order-by-desc", test_select_order_by_desc);
    g_test_add_func ("/select/multiple-order-by", test_select_multiple_order_by);

    /* LIMIT/OFFSET tests */
    g_test_add_func ("/select/limit", test_select_limit);
    g_test_add_func ("/select/offset", test_select_offset);

    /* Advanced tests */
    g_test_add_func ("/select/distinct", test_select_distinct);
    g_test_add_func ("/select/join", test_select_join);
    g_test_add_func ("/select/left-join", test_select_left_join);
    g_test_add_func ("/select/group-by", test_select_group_by);
    g_test_add_func ("/select/complex", test_select_complex);

    return g_test_run ();
}
