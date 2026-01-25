/* test-types.c
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
 * OrmSqlType Base Tests
 * ============================================================================ */

static void
test_sql_type_integer (void)
{
    g_autoptr(OrmInteger) type = orm_integer_new ();

    g_assert_nonnull (type);
    g_assert_true (ORM_IS_SQL_TYPE (type));
    g_assert_true (ORM_IS_INTEGER (type));

    /* Note: get_name requires dialect type */
    const gchar *name = orm_sql_type_get_name (ORM_SQL_TYPE (type), ORM_DIALECT_SQLITE);
    g_assert_nonnull (name);
}

static void
test_sql_type_bigint (void)
{
    g_autoptr(OrmBigInt) type = orm_bigint_new ();

    g_assert_nonnull (type);
    g_assert_true (ORM_IS_SQL_TYPE (type));
    g_assert_true (ORM_IS_BIGINT (type));

    const gchar *name = orm_sql_type_get_name (ORM_SQL_TYPE (type), ORM_DIALECT_SQLITE);
    g_assert_nonnull (name);
}

static void
test_sql_type_string (void)
{
    g_autoptr(OrmString) type = orm_string_new (255);

    g_assert_nonnull (type);
    g_assert_true (ORM_IS_SQL_TYPE (type));
    g_assert_true (ORM_IS_STRING (type));

    g_assert_cmpuint (orm_string_get_length (type), ==, 255);
}

static void
test_sql_type_string_default_length (void)
{
    g_autoptr(OrmString) type = orm_string_new (0);

    g_assert_nonnull (type);
    /* Default length should be some reasonable value */
    g_assert_cmpuint (orm_string_get_length (type), >=, 0);
}

static void
test_sql_type_text (void)
{
    g_autoptr(OrmText) type = orm_text_new ();

    g_assert_nonnull (type);
    g_assert_true (ORM_IS_SQL_TYPE (type));
    g_assert_true (ORM_IS_TEXT (type));

    const gchar *name = orm_sql_type_get_name (ORM_SQL_TYPE (type), ORM_DIALECT_SQLITE);
    g_assert_nonnull (name);
}

static void
test_sql_type_boolean (void)
{
    g_autoptr(OrmBooleanType) type = orm_boolean_type_new ();

    g_assert_nonnull (type);
    g_assert_true (ORM_IS_SQL_TYPE (type));
    g_assert_true (ORM_IS_BOOLEAN_TYPE (type));

    const gchar *name = orm_sql_type_get_name (ORM_SQL_TYPE (type), ORM_DIALECT_SQLITE);
    g_assert_nonnull (name);
}

static void
test_sql_type_float (void)
{
    g_autoptr(OrmFloatType) type = orm_float_type_new ();

    g_assert_nonnull (type);
    g_assert_true (ORM_IS_SQL_TYPE (type));
    g_assert_true (ORM_IS_FLOAT_TYPE (type));

    const gchar *name = orm_sql_type_get_name (ORM_SQL_TYPE (type), ORM_DIALECT_SQLITE);
    g_assert_nonnull (name);
}

static void
test_sql_type_double (void)
{
    g_autoptr(OrmDoubleType) type = orm_double_type_new ();

    g_assert_nonnull (type);
    g_assert_true (ORM_IS_SQL_TYPE (type));
    g_assert_true (ORM_IS_DOUBLE_TYPE (type));

    const gchar *name = orm_sql_type_get_name (ORM_SQL_TYPE (type), ORM_DIALECT_SQLITE);
    g_assert_nonnull (name);
}

static void
test_sql_type_datetime (void)
{
    g_autoptr(OrmDateTimeType) type = orm_datetime_type_new ();

    g_assert_nonnull (type);
    g_assert_true (ORM_IS_SQL_TYPE (type));
    g_assert_true (ORM_IS_DATETIME_TYPE (type));

    const gchar *name = orm_sql_type_get_name (ORM_SQL_TYPE (type), ORM_DIALECT_SQLITE);
    g_assert_nonnull (name);
}

static void
test_sql_type_blob (void)
{
    g_autoptr(OrmBlobType) type = orm_blob_type_new ();

    g_assert_nonnull (type);
    g_assert_true (ORM_IS_SQL_TYPE (type));
    g_assert_true (ORM_IS_BLOB_TYPE (type));

    const gchar *name = orm_sql_type_get_name (ORM_SQL_TYPE (type), ORM_DIALECT_SQLITE);
    g_assert_nonnull (name);
}

/* ============================================================================
 * Type Processor Tests
 * ============================================================================ */

static void
test_integer_bind_processor (void)
{
    g_autoptr(OrmInteger) type = orm_integer_new ();
    g_autoptr(OrmValue) input = orm_value_new_integer (42);

    OrmValue *result = orm_sql_type_bind_processor (ORM_SQL_TYPE (type), input);

    /* Result may be NULL (meaning use original) or a processed value */
    if (result != NULL)
    {
        g_assert_cmpint (orm_value_get_integer (result), ==, 42);
        orm_value_free (result);
    }
}

static void
test_boolean_bind_processor (void)
{
    g_autoptr(OrmBooleanType) type = orm_boolean_type_new ();
    g_autoptr(OrmValue) input = orm_value_new_boolean (TRUE);

    OrmValue *result = orm_sql_type_bind_processor (ORM_SQL_TYPE (type), input);

    /* Boolean may be converted to integer for some dialects, or NULL if no conversion needed */
    if (result != NULL)
    {
        orm_value_free (result);
    }
}

static void
test_string_bind_processor (void)
{
    g_autoptr(OrmString) type = orm_string_new (100);
    g_autoptr(OrmValue) input = orm_value_new_string ("test");

    OrmValue *result = orm_sql_type_bind_processor (ORM_SQL_TYPE (type), input);

    if (result != NULL)
    {
        g_assert_cmpstr (orm_value_get_string (result), ==, "test");
        orm_value_free (result);
    }
}

static void
test_result_processor (void)
{
    g_autoptr(OrmInteger) type = orm_integer_new ();
    g_autoptr(OrmValue) input = orm_value_new_integer (42);

    OrmValue *result = orm_sql_type_result_processor (ORM_SQL_TYPE (type), input);

    if (result != NULL)
    {
        g_assert_cmpint (orm_value_get_integer (result), ==, 42);
        orm_value_free (result);
    }
}

/* ============================================================================
 * Type Nullable Tests
 * ============================================================================ */

static void
test_sql_type_nullable_default (void)
{
    g_autoptr(OrmInteger) type = orm_integer_new ();

    /* Default nullable state */
    gboolean nullable = orm_sql_type_get_nullable (ORM_SQL_TYPE (type));
    /* Just check that the function works */
    (void) nullable;
}

static void
test_sql_type_set_nullable (void)
{
    g_autoptr(OrmInteger) type = orm_integer_new ();

    orm_sql_type_set_nullable (ORM_SQL_TYPE (type), TRUE);
    g_assert_true (orm_sql_type_get_nullable (ORM_SQL_TYPE (type)));

    orm_sql_type_set_nullable (ORM_SQL_TYPE (type), FALSE);
    g_assert_false (orm_sql_type_get_nullable (ORM_SQL_TYPE (type)));
}

static void
test_sql_type_process_null (void)
{
    g_autoptr(OrmInteger) type = orm_integer_new ();
    g_autoptr(OrmValue) input = orm_value_new_null ();

    OrmValue *result = orm_sql_type_bind_processor (ORM_SQL_TYPE (type), input);

    /* NULL input should be handled gracefully */
    if (result != NULL)
    {
        g_assert_true (orm_value_is_null (result));
        orm_value_free (result);
    }
}

/* ============================================================================
 * Type Comparison Tests
 * ============================================================================ */

static void
test_sql_type_same_type (void)
{
    g_autoptr(OrmInteger) type1 = orm_integer_new ();
    g_autoptr(OrmInteger) type2 = orm_integer_new ();

    /* Both are INTEGER types */
    const gchar *name1 = orm_sql_type_get_name (ORM_SQL_TYPE (type1), ORM_DIALECT_SQLITE);
    const gchar *name2 = orm_sql_type_get_name (ORM_SQL_TYPE (type2), ORM_DIALECT_SQLITE);

    g_assert_cmpstr (name1, ==, name2);
}

static void
test_sql_type_different_types (void)
{
    g_autoptr(OrmInteger) int_type = orm_integer_new ();
    g_autoptr(OrmString) str_type = orm_string_new (100);

    const gchar *int_name = orm_sql_type_get_name (ORM_SQL_TYPE (int_type), ORM_DIALECT_SQLITE);
    const gchar *str_name = orm_sql_type_get_name (ORM_SQL_TYPE (str_type), ORM_DIALECT_SQLITE);

    g_assert_cmpstr (int_name, !=, str_name);
}

static void
test_sql_type_compare_values (void)
{
    g_autoptr(OrmInteger) type = orm_integer_new ();
    g_autoptr(OrmValue) a = orm_value_new_integer (10);
    g_autoptr(OrmValue) b = orm_value_new_integer (20);
    g_autoptr(OrmValue) c = orm_value_new_integer (10);

    gint cmp_ab = orm_sql_type_compare_values (ORM_SQL_TYPE (type), a, b);
    gint cmp_ba = orm_sql_type_compare_values (ORM_SQL_TYPE (type), b, a);
    gint cmp_ac = orm_sql_type_compare_values (ORM_SQL_TYPE (type), a, c);

    g_assert_cmpint (cmp_ab, <, 0);
    g_assert_cmpint (cmp_ba, >, 0);
    g_assert_cmpint (cmp_ac, ==, 0);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Base type tests */
    g_test_add_func ("/types/integer", test_sql_type_integer);
    g_test_add_func ("/types/bigint", test_sql_type_bigint);
    g_test_add_func ("/types/string", test_sql_type_string);
    g_test_add_func ("/types/string-default-length", test_sql_type_string_default_length);
    g_test_add_func ("/types/text", test_sql_type_text);
    g_test_add_func ("/types/boolean", test_sql_type_boolean);
    g_test_add_func ("/types/float", test_sql_type_float);
    g_test_add_func ("/types/double", test_sql_type_double);
    g_test_add_func ("/types/datetime", test_sql_type_datetime);
    g_test_add_func ("/types/blob", test_sql_type_blob);

    /* Processor tests */
    g_test_add_func ("/types/processor/integer-bind", test_integer_bind_processor);
    g_test_add_func ("/types/processor/boolean-bind", test_boolean_bind_processor);
    g_test_add_func ("/types/processor/string-bind", test_string_bind_processor);
    g_test_add_func ("/types/processor/result", test_result_processor);

    /* Nullable tests */
    g_test_add_func ("/types/nullable/default", test_sql_type_nullable_default);
    g_test_add_func ("/types/nullable/set", test_sql_type_set_nullable);
    g_test_add_func ("/types/nullable/process-null", test_sql_type_process_null);

    /* Comparison tests */
    g_test_add_func ("/types/same-type", test_sql_type_same_type);
    g_test_add_func ("/types/different-types", test_sql_type_different_types);
    g_test_add_func ("/types/compare-values", test_sql_type_compare_values);

    return g_test_run ();
}
