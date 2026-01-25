/* test-orm.c
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

#include "test-fixtures.h"
#include "test-model.h"

/* ============================================================================
 * OrmSerializable Tests
 * ============================================================================ */

static void
test_serializable_interface (void)
{
    g_autoptr(TestUser) user = test_user_new ();

    g_assert_true (ORM_IS_SERIALIZABLE (user));
}

static void
test_serializable_get_table_name (void)
{
    g_autoptr(TestUser) user = test_user_new ();

    const gchar *table = orm_serializable_get_table_name (ORM_SERIALIZABLE (user));
    g_assert_cmpstr (table, ==, "test_user");
}

static void
test_serializable_get_primary_key (void)
{
    g_autoptr(TestUser) user = test_user_new ();

    const gchar *pk = orm_serializable_get_primary_key (ORM_SERIALIZABLE (user));
    g_assert_cmpstr (pk, ==, "id");
}

static void
test_serializable_find_property (void)
{
    g_autoptr(TestUser) user = test_user_new ();

    GParamSpec *pspec = orm_serializable_find_property (ORM_SERIALIZABLE (user), "name");
    g_assert_nonnull (pspec);
    g_assert_cmpstr (pspec->name, ==, "name");
}

static void
test_serializable_find_property_nonexistent (void)
{
    g_autoptr(TestUser) user = test_user_new ();

    GParamSpec *pspec = orm_serializable_find_property (ORM_SERIALIZABLE (user), "nonexistent");
    g_assert_null (pspec);
}

static void
test_serializable_list_properties (void)
{
    g_autoptr(TestUser) user = test_user_new ();
    guint n_props;

    GParamSpec **props = orm_serializable_list_properties (ORM_SERIALIZABLE (user), &n_props);

    g_assert_nonnull (props);
    g_assert_cmpuint (n_props, >=, 4);  /* At least id, name, email, active */

    g_free (props);
}

static void
test_serializable_get_property_value (void)
{
    g_autoptr(TestUser) user = test_user_new_with_values ("Alice", "alice@example.com");

    g_autoptr(OrmValue) name_val = orm_serializable_get_property_value (
        ORM_SERIALIZABLE (user), "name");

    g_assert_nonnull (name_val);
    g_assert_cmpstr (orm_value_get_string (name_val), ==, "Alice");
}

static void
test_serializable_set_property_value (void)
{
    g_autoptr(TestUser) user = test_user_new ();
    g_autoptr(OrmValue) name_val = orm_value_new_string ("Bob");

    gboolean result = orm_serializable_set_property_value (
        ORM_SERIALIZABLE (user), "name", name_val);

    g_assert_true (result);
    g_assert_cmpstr (test_user_get_name (user), ==, "Bob");
}

static void
test_serializable_get_primary_key_value (void)
{
    g_autoptr(TestUser) user = test_user_new ();
    test_user_set_id (user, 42);

    g_autoptr(OrmValue) pk_val = orm_serializable_get_primary_key_value (
        ORM_SERIALIZABLE (user));

    g_assert_nonnull (pk_val);
    g_assert_cmpint (orm_value_get_integer (pk_val), ==, 42);
}

/* ============================================================================
 * OrmProperty Tests
 * ============================================================================ */

static void
test_property_new (void)
{
    g_autoptr(OrmInteger) int_type = orm_integer_new ();
    g_autoptr(OrmProperty) prop = orm_property_new ("id", "id",
                                                     ORM_SQL_TYPE (int_type),
                                                     ORM_PROPERTY_PRIMARY_KEY);

    g_assert_nonnull (prop);
    g_assert_true (ORM_IS_PROPERTY (prop));
}

static void
test_property_names (void)
{
    g_autoptr(OrmInteger) int_type = orm_integer_new ();
    g_autoptr(OrmProperty) prop = orm_property_new ("user_id", "user_id_column",
                                                     ORM_SQL_TYPE (int_type),
                                                     ORM_PROPERTY_NONE);

    g_assert_cmpstr (orm_property_get_property_name (prop), ==, "user_id");
    g_assert_cmpstr (orm_property_get_column_name (prop), ==, "user_id_column");
}

static void
test_property_flags (void)
{
    g_autoptr(OrmInteger) int_type = orm_integer_new ();
    g_autoptr(OrmProperty) prop = orm_property_new ("id", NULL,
                                                     ORM_SQL_TYPE (int_type),
                                                     ORM_PROPERTY_PRIMARY_KEY |
                                                     ORM_PROPERTY_AUTO_INCREMENT);

    g_assert_true (orm_property_is_primary_key (prop));
    g_assert_true (orm_property_is_auto_increment (prop));
    g_assert_false (orm_property_is_nullable (prop));
}

static void
test_property_nullable (void)
{
    g_autoptr(OrmString) str_type = orm_string_new (100);
    g_autoptr(OrmProperty) prop = orm_property_new ("email", NULL,
                                                     ORM_SQL_TYPE (str_type),
                                                     ORM_PROPERTY_NULLABLE |
                                                     ORM_PROPERTY_UNIQUE);

    g_assert_true (orm_property_is_nullable (prop));
    g_assert_true (orm_property_is_unique (prop));
}

static void
test_property_default_value (void)
{
    g_autoptr(OrmBooleanType) bool_type = orm_boolean_type_new ();
    g_autoptr(OrmProperty) prop = orm_property_new ("active", NULL,
                                                     ORM_SQL_TYPE (bool_type),
                                                     ORM_PROPERTY_NONE);
    g_autoptr(OrmValue) default_val = orm_value_new_boolean (TRUE);

    orm_property_set_default_value (prop, default_val);

    OrmValue *result = orm_property_get_default_value (prop);
    g_assert_nonnull (result);
    g_assert_true (orm_value_get_boolean (result));
}

/* ============================================================================
 * OrmMapper Tests
 * ============================================================================ */

static void
test_mapper_new (void)
{
    g_autoptr(OrmMapper) mapper = orm_mapper_new (TEST_TYPE_USER, "users");

    g_assert_nonnull (mapper);
    g_assert_true (ORM_IS_MAPPER (mapper));
}

static void
test_mapper_gtype (void)
{
    g_autoptr(OrmMapper) mapper = orm_mapper_new (TEST_TYPE_USER, "users");

    g_assert_true (orm_mapper_get_gtype (mapper) == TEST_TYPE_USER);
}

static void
test_mapper_table_name (void)
{
    g_autoptr(OrmMapper) mapper = orm_mapper_new (TEST_TYPE_USER, "users");

    g_assert_cmpstr (orm_mapper_get_table_name (mapper), ==, "users");
}

static void
test_mapper_add_property (void)
{
    g_autoptr(OrmMapper) mapper = orm_mapper_new (TEST_TYPE_USER, "users");
    g_autoptr(OrmInteger) int_type = orm_integer_new ();
    g_autoptr(OrmProperty) prop = orm_property_new ("id", NULL,
                                                     ORM_SQL_TYPE (int_type),
                                                     ORM_PROPERTY_PRIMARY_KEY);

    orm_mapper_add_property (mapper, prop);

    OrmProperty *found = orm_mapper_get_property (mapper, "id");
    g_assert_nonnull (found);
}

static void
test_mapper_from_serializable (void)
{
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);

    g_assert_nonnull (mapper);

    /* Should have properties auto-mapped */
    /* Note: orm_mapper_get_properties returns internal list (transfer none) */
    GList *props = orm_mapper_get_properties (mapper);
    g_assert_cmpuint (g_list_length (props), >=, 4);
    /* Don't free - it's the internal list */
}

static void
test_mapper_get_primary_key_property (void)
{
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);

    OrmProperty *pk_prop = orm_mapper_get_primary_key_property (mapper);
    g_assert_nonnull (pk_prop);
    g_assert_cmpstr (orm_property_get_property_name (pk_prop), ==, "id");
}

static void
test_mapper_to_table (void)
{
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (TEST_TYPE_USER);

    g_autoptr(OrmTable) table = orm_mapper_to_table (mapper);

    g_assert_nonnull (table);
    g_assert_cmpstr (orm_table_get_name (table), ==, "test_user");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Serializable tests */
    g_test_add_func ("/orm/serializable/interface", test_serializable_interface);
    g_test_add_func ("/orm/serializable/get-table-name", test_serializable_get_table_name);
    g_test_add_func ("/orm/serializable/get-primary-key", test_serializable_get_primary_key);
    g_test_add_func ("/orm/serializable/find-property", test_serializable_find_property);
    g_test_add_func ("/orm/serializable/find-property-nonexistent", test_serializable_find_property_nonexistent);
    g_test_add_func ("/orm/serializable/list-properties", test_serializable_list_properties);
    g_test_add_func ("/orm/serializable/get-property-value", test_serializable_get_property_value);
    g_test_add_func ("/orm/serializable/set-property-value", test_serializable_set_property_value);
    g_test_add_func ("/orm/serializable/get-primary-key-value", test_serializable_get_primary_key_value);

    /* Property tests */
    g_test_add_func ("/orm/property/new", test_property_new);
    g_test_add_func ("/orm/property/names", test_property_names);
    g_test_add_func ("/orm/property/flags", test_property_flags);
    g_test_add_func ("/orm/property/nullable", test_property_nullable);
    g_test_add_func ("/orm/property/default-value", test_property_default_value);

    /* Mapper tests */
    g_test_add_func ("/orm/mapper/new", test_mapper_new);
    g_test_add_func ("/orm/mapper/gtype", test_mapper_gtype);
    g_test_add_func ("/orm/mapper/table-name", test_mapper_table_name);
    g_test_add_func ("/orm/mapper/add-property", test_mapper_add_property);
    g_test_add_func ("/orm/mapper/from-serializable", test_mapper_from_serializable);
    g_test_add_func ("/orm/mapper/get-primary-key-property", test_mapper_get_primary_key_property);
    g_test_add_func ("/orm/mapper/to-table", test_mapper_to_table);

    return g_test_run ();
}
