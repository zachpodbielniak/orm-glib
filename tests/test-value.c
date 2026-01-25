/* test-value.c
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
 * Constructor Tests
 * ============================================================================ */

static void
test_value_new_null (void)
{
    g_autoptr(OrmValue) value = orm_value_new_null ();

    g_assert_nonnull (value);
    g_assert_true (orm_value_is_null (value));
    g_assert_cmpint (orm_value_get_value_type (value), ==, ORM_VALUE_NULL);
}

static void
test_value_new_integer (void)
{
    g_autoptr(OrmValue) value = orm_value_new_integer (42);

    g_assert_nonnull (value);
    g_assert_false (orm_value_is_null (value));
    g_assert_cmpint (orm_value_get_value_type (value), ==, ORM_VALUE_INTEGER);
    g_assert_cmpint (orm_value_get_integer (value), ==, 42);
}

static void
test_value_new_integer_negative (void)
{
    g_autoptr(OrmValue) value = orm_value_new_integer (-999);

    g_assert_cmpint (orm_value_get_integer (value), ==, -999);
}

static void
test_value_new_integer_max (void)
{
    g_autoptr(OrmValue) value = orm_value_new_integer (G_MAXINT64);

    g_assert_cmpint (orm_value_get_integer (value), ==, G_MAXINT64);
}

static void
test_value_new_float (void)
{
    g_autoptr(OrmValue) value = orm_value_new_float (3.14159);

    g_assert_nonnull (value);
    g_assert_cmpint (orm_value_get_value_type (value), ==, ORM_VALUE_FLOAT);
    g_assert_cmpfloat_with_epsilon (orm_value_get_float (value), 3.14159, 0.0001);
}

static void
test_value_new_string (void)
{
    g_autoptr(OrmValue) value = orm_value_new_string ("hello world");

    g_assert_nonnull (value);
    g_assert_cmpint (orm_value_get_value_type (value), ==, ORM_VALUE_STRING);
    g_assert_cmpstr (orm_value_get_string (value), ==, "hello world");
}

static void
test_value_new_string_null (void)
{
    /* Passing NULL to new_string should create a NULL value */
    g_autoptr(OrmValue) value = orm_value_new_string (NULL);

    g_assert_nonnull (value);
    g_assert_true (orm_value_is_null (value));
}

static void
test_value_new_string_empty (void)
{
    g_autoptr(OrmValue) value = orm_value_new_string ("");

    g_assert_false (orm_value_is_null (value));
    g_assert_cmpstr (orm_value_get_string (value), ==, "");
}

static void
test_value_new_blob (void)
{
    const guint8 data[] = { 0x01, 0x02, 0x03, 0x04 };
    g_autoptr(GBytes) bytes = g_bytes_new (data, sizeof (data));
    g_autoptr(OrmValue) value = orm_value_new_blob (bytes);

    g_assert_nonnull (value);
    g_assert_cmpint (orm_value_get_value_type (value), ==, ORM_VALUE_BLOB);

    GBytes *result = orm_value_get_blob (value);
    g_assert_nonnull (result);
    g_assert_cmpuint (g_bytes_get_size (result), ==, 4);
}

static void
test_value_new_blob_null (void)
{
    g_autoptr(OrmValue) value = orm_value_new_blob (NULL);

    g_assert_true (orm_value_is_null (value));
}

static void
test_value_new_boolean_true (void)
{
    g_autoptr(OrmValue) value = orm_value_new_boolean (TRUE);

    g_assert_cmpint (orm_value_get_value_type (value), ==, ORM_VALUE_BOOLEAN);
    g_assert_true (orm_value_get_boolean (value));
}

static void
test_value_new_boolean_false (void)
{
    g_autoptr(OrmValue) value = orm_value_new_boolean (FALSE);

    g_assert_cmpint (orm_value_get_value_type (value), ==, ORM_VALUE_BOOLEAN);
    g_assert_false (orm_value_get_boolean (value));
}

static void
test_value_new_datetime (void)
{
    g_autoptr(GDateTime) dt = g_date_time_new_utc (2025, 1, 15, 12, 30, 0);
    g_autoptr(OrmValue) value = orm_value_new_datetime (dt);

    g_assert_cmpint (orm_value_get_value_type (value), ==, ORM_VALUE_DATETIME);

    GDateTime *result = orm_value_get_datetime (value);
    g_assert_nonnull (result);
    g_assert_cmpint (g_date_time_get_year (result), ==, 2025);
    g_assert_cmpint (g_date_time_get_month (result), ==, 1);
    g_assert_cmpint (g_date_time_get_day_of_month (result), ==, 15);
}

static void
test_value_new_datetime_null (void)
{
    g_autoptr(OrmValue) value = orm_value_new_datetime (NULL);

    g_assert_true (orm_value_is_null (value));
}

/* ============================================================================
 * Copy Tests
 * ============================================================================ */

static void
test_value_copy_null (void)
{
    g_autoptr(OrmValue) original = orm_value_new_null ();
    g_autoptr(OrmValue) copy = orm_value_copy (original);

    g_assert_nonnull (copy);
    g_assert_true (orm_value_is_null (copy));
}

static void
test_value_copy_integer (void)
{
    g_autoptr(OrmValue) original = orm_value_new_integer (42);
    g_autoptr(OrmValue) copy = orm_value_copy (original);

    g_assert_cmpint (orm_value_get_integer (copy), ==, 42);
}

static void
test_value_copy_string (void)
{
    g_autoptr(OrmValue) original = orm_value_new_string ("test");
    g_autoptr(OrmValue) copy = orm_value_copy (original);

    g_assert_cmpstr (orm_value_get_string (copy), ==, "test");

    /* Verify it's a deep copy (different memory) */
    g_assert_true (orm_value_get_string (original) != orm_value_get_string (copy));
}

static void
test_value_copy_null_pointer (void)
{
    /* Copying NULL should return NULL */
    OrmValue *copy = orm_value_copy (NULL);

    g_assert_null (copy);
}

/* ============================================================================
 * GValue Conversion Tests
 * ============================================================================ */

static void
test_value_to_gvalue_integer (void)
{
    g_autoptr(OrmValue) value = orm_value_new_integer (42);
    GValue gvalue = G_VALUE_INIT;

    gboolean result = orm_value_to_gvalue (value, &gvalue);

    g_assert_true (result);
    g_assert_cmpint (G_VALUE_TYPE (&gvalue), ==, G_TYPE_INT64);
    g_assert_cmpint (g_value_get_int64 (&gvalue), ==, 42);

    g_value_unset (&gvalue);
}

static void
test_value_to_gvalue_string (void)
{
    g_autoptr(OrmValue) value = orm_value_new_string ("hello");
    GValue gvalue = G_VALUE_INIT;

    gboolean result = orm_value_to_gvalue (value, &gvalue);

    g_assert_true (result);
    g_assert_cmpint (G_VALUE_TYPE (&gvalue), ==, G_TYPE_STRING);
    g_assert_cmpstr (g_value_get_string (&gvalue), ==, "hello");

    g_value_unset (&gvalue);
}

static void
test_value_to_gvalue_boolean (void)
{
    g_autoptr(OrmValue) value = orm_value_new_boolean (TRUE);
    GValue gvalue = G_VALUE_INIT;

    gboolean result = orm_value_to_gvalue (value, &gvalue);

    g_assert_true (result);
    g_assert_cmpint (G_VALUE_TYPE (&gvalue), ==, G_TYPE_BOOLEAN);
    g_assert_true (g_value_get_boolean (&gvalue));

    g_value_unset (&gvalue);
}

static void
test_value_to_gvalue_reinitialization (void)
{
    /*
     * Test that orm_value_to_gvalue handles already-initialized GValues.
     * This was Bug #1 - calling g_value_init on an already initialized GValue.
     */
    g_autoptr(OrmValue) value1 = orm_value_new_integer (42);
    g_autoptr(OrmValue) value2 = orm_value_new_string ("hello");
    GValue gvalue = G_VALUE_INIT;

    /* First conversion */
    g_assert_true (orm_value_to_gvalue (value1, &gvalue));
    g_assert_cmpint (g_value_get_int64 (&gvalue), ==, 42);

    /* Second conversion should work without crashing */
    g_assert_true (orm_value_to_gvalue (value2, &gvalue));
    g_assert_cmpstr (g_value_get_string (&gvalue), ==, "hello");

    g_value_unset (&gvalue);
}

static void
test_value_to_gvalue_with_type_int (void)
{
    /*
     * Test type-aware conversion from INTEGER to G_TYPE_INT.
     */
    g_autoptr(OrmValue) value = orm_value_new_integer (42);
    GValue gvalue = G_VALUE_INIT;

    gboolean result = orm_value_to_gvalue_with_type (value, &gvalue, G_TYPE_INT);

    g_assert_true (result);
    g_assert_cmpint (G_VALUE_TYPE (&gvalue), ==, G_TYPE_INT);
    g_assert_cmpint (g_value_get_int (&gvalue), ==, 42);

    g_value_unset (&gvalue);
}

static void
test_value_to_gvalue_with_type_boolean_from_int (void)
{
    /*
     * Test coercion from INTEGER to BOOLEAN.
     */
    g_autoptr(OrmValue) value = orm_value_new_integer (1);
    GValue gvalue = G_VALUE_INIT;

    gboolean result = orm_value_to_gvalue_with_type (value, &gvalue, G_TYPE_BOOLEAN);

    g_assert_true (result);
    g_assert_cmpint (G_VALUE_TYPE (&gvalue), ==, G_TYPE_BOOLEAN);
    g_assert_true (g_value_get_boolean (&gvalue));

    g_value_unset (&gvalue);
}

static void
test_value_from_gvalue_int (void)
{
    GValue gvalue = G_VALUE_INIT;
    g_value_init (&gvalue, G_TYPE_INT);
    g_value_set_int (&gvalue, 42);

    g_autoptr(OrmValue) value = orm_value_from_gvalue (&gvalue);

    g_assert_nonnull (value);
    g_assert_cmpint (orm_value_get_value_type (value), ==, ORM_VALUE_INTEGER);
    g_assert_cmpint (orm_value_get_integer (value), ==, 42);

    g_value_unset (&gvalue);
}

static void
test_value_from_gvalue_string (void)
{
    GValue gvalue = G_VALUE_INIT;
    g_value_init (&gvalue, G_TYPE_STRING);
    g_value_set_string (&gvalue, "test");

    g_autoptr(OrmValue) value = orm_value_from_gvalue (&gvalue);

    g_assert_nonnull (value);
    g_assert_cmpint (orm_value_get_value_type (value), ==, ORM_VALUE_STRING);
    g_assert_cmpstr (orm_value_get_string (value), ==, "test");

    g_value_unset (&gvalue);
}

static void
test_value_from_gvalue_uninitialized (void)
{
    GValue gvalue = G_VALUE_INIT;

    g_autoptr(OrmValue) value = orm_value_from_gvalue (&gvalue);

    g_assert_nonnull (value);
    g_assert_true (orm_value_is_null (value));
}

/* ============================================================================
 * Comparison Tests
 * ============================================================================ */

static void
test_value_compare_integers (void)
{
    g_autoptr(OrmValue) a = orm_value_new_integer (10);
    g_autoptr(OrmValue) b = orm_value_new_integer (20);
    g_autoptr(OrmValue) c = orm_value_new_integer (10);

    g_assert_cmpint (orm_value_compare (a, b), <, 0);
    g_assert_cmpint (orm_value_compare (b, a), >, 0);
    g_assert_cmpint (orm_value_compare (a, c), ==, 0);
}

static void
test_value_compare_strings (void)
{
    g_autoptr(OrmValue) a = orm_value_new_string ("apple");
    g_autoptr(OrmValue) b = orm_value_new_string ("banana");
    g_autoptr(OrmValue) c = orm_value_new_string ("apple");

    g_assert_cmpint (orm_value_compare (a, b), <, 0);
    g_assert_cmpint (orm_value_compare (b, a), >, 0);
    g_assert_cmpint (orm_value_compare (a, c), ==, 0);
}

static void
test_value_compare_null (void)
{
    g_autoptr(OrmValue) null1 = orm_value_new_null ();
    g_autoptr(OrmValue) null2 = orm_value_new_null ();
    g_autoptr(OrmValue) integer = orm_value_new_integer (42);

    g_assert_cmpint (orm_value_compare (null1, null2), ==, 0);
    g_assert_cmpint (orm_value_compare (null1, integer), <, 0);
    g_assert_cmpint (orm_value_compare (integer, null1), >, 0);
}

static void
test_value_compare_null_pointer (void)
{
    g_autoptr(OrmValue) value = orm_value_new_integer (42);

    g_assert_cmpint (orm_value_compare (NULL, value), <, 0);
    g_assert_cmpint (orm_value_compare (value, NULL), >, 0);
    g_assert_cmpint (orm_value_compare (NULL, NULL), ==, 0);
}

static void
test_value_equal (void)
{
    g_autoptr(OrmValue) a = orm_value_new_integer (42);
    g_autoptr(OrmValue) b = orm_value_new_integer (42);
    g_autoptr(OrmValue) c = orm_value_new_integer (99);

    g_assert_true (orm_value_equal (a, b));
    g_assert_false (orm_value_equal (a, c));
}

/* ============================================================================
 * Hash Tests
 * ============================================================================ */

static void
test_value_hash_integer (void)
{
    g_autoptr(OrmValue) a = orm_value_new_integer (42);
    g_autoptr(OrmValue) b = orm_value_new_integer (42);
    g_autoptr(OrmValue) c = orm_value_new_integer (99);

    g_assert_cmpuint (orm_value_hash (a), ==, orm_value_hash (b));
    /* Different values might have same hash, but usually don't */
    g_assert_cmpuint (orm_value_hash (a), !=, orm_value_hash (c));
}

static void
test_value_hash_null (void)
{
    g_autoptr(OrmValue) value = orm_value_new_null ();

    g_assert_cmpuint (orm_value_hash (value), ==, 0);
    g_assert_cmpuint (orm_value_hash (NULL), ==, 0);
}

/* ============================================================================
 * String Representation Tests
 * ============================================================================ */

static void
test_value_to_string_null (void)
{
    g_autoptr(OrmValue) value = orm_value_new_null ();
    g_autofree gchar *str = orm_value_to_string (value);

    g_assert_cmpstr (str, ==, "NULL");
}

static void
test_value_to_string_integer (void)
{
    g_autoptr(OrmValue) value = orm_value_new_integer (42);
    g_autofree gchar *str = orm_value_to_string (value);

    g_assert_cmpstr (str, ==, "42");
}

static void
test_value_to_string_string (void)
{
    g_autoptr(OrmValue) value = orm_value_new_string ("hello");
    g_autofree gchar *str = orm_value_to_string (value);

    g_assert_cmpstr (str, ==, "'hello'");
}

static void
test_value_to_string_boolean (void)
{
    g_autoptr(OrmValue) t = orm_value_new_boolean (TRUE);
    g_autoptr(OrmValue) f = orm_value_new_boolean (FALSE);

    g_autofree gchar *str_t = orm_value_to_string (t);
    g_autofree gchar *str_f = orm_value_to_string (f);

    g_assert_cmpstr (str_t, ==, "TRUE");
    g_assert_cmpstr (str_f, ==, "FALSE");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Constructor tests */
    g_test_add_func ("/value/new/null", test_value_new_null);
    g_test_add_func ("/value/new/integer", test_value_new_integer);
    g_test_add_func ("/value/new/integer-negative", test_value_new_integer_negative);
    g_test_add_func ("/value/new/integer-max", test_value_new_integer_max);
    g_test_add_func ("/value/new/float", test_value_new_float);
    g_test_add_func ("/value/new/string", test_value_new_string);
    g_test_add_func ("/value/new/string-null", test_value_new_string_null);
    g_test_add_func ("/value/new/string-empty", test_value_new_string_empty);
    g_test_add_func ("/value/new/blob", test_value_new_blob);
    g_test_add_func ("/value/new/blob-null", test_value_new_blob_null);
    g_test_add_func ("/value/new/boolean-true", test_value_new_boolean_true);
    g_test_add_func ("/value/new/boolean-false", test_value_new_boolean_false);
    g_test_add_func ("/value/new/datetime", test_value_new_datetime);
    g_test_add_func ("/value/new/datetime-null", test_value_new_datetime_null);

    /* Copy tests */
    g_test_add_func ("/value/copy/null", test_value_copy_null);
    g_test_add_func ("/value/copy/integer", test_value_copy_integer);
    g_test_add_func ("/value/copy/string", test_value_copy_string);
    g_test_add_func ("/value/copy/null-pointer", test_value_copy_null_pointer);

    /* GValue conversion tests */
    g_test_add_func ("/value/gvalue/to-integer", test_value_to_gvalue_integer);
    g_test_add_func ("/value/gvalue/to-string", test_value_to_gvalue_string);
    g_test_add_func ("/value/gvalue/to-boolean", test_value_to_gvalue_boolean);
    g_test_add_func ("/value/gvalue/reinitialization", test_value_to_gvalue_reinitialization);
    g_test_add_func ("/value/gvalue/with-type-int", test_value_to_gvalue_with_type_int);
    g_test_add_func ("/value/gvalue/with-type-boolean-from-int", test_value_to_gvalue_with_type_boolean_from_int);
    g_test_add_func ("/value/gvalue/from-int", test_value_from_gvalue_int);
    g_test_add_func ("/value/gvalue/from-string", test_value_from_gvalue_string);
    g_test_add_func ("/value/gvalue/from-uninitialized", test_value_from_gvalue_uninitialized);

    /* Comparison tests */
    g_test_add_func ("/value/compare/integers", test_value_compare_integers);
    g_test_add_func ("/value/compare/strings", test_value_compare_strings);
    g_test_add_func ("/value/compare/null", test_value_compare_null);
    g_test_add_func ("/value/compare/null-pointer", test_value_compare_null_pointer);
    g_test_add_func ("/value/equal", test_value_equal);

    /* Hash tests */
    g_test_add_func ("/value/hash/integer", test_value_hash_integer);
    g_test_add_func ("/value/hash/null", test_value_hash_null);

    /* String representation tests */
    g_test_add_func ("/value/to-string/null", test_value_to_string_null);
    g_test_add_func ("/value/to-string/integer", test_value_to_string_integer);
    g_test_add_func ("/value/to-string/string", test_value_to_string_string);
    g_test_add_func ("/value/to-string/boolean", test_value_to_string_boolean);

    return g_test_run ();
}
