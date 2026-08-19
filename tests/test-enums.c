/* test-enums.c
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
 * Every public enum must carry a GType.  Without one it cannot appear in a
 * GObject property, cannot be introspected, and so is invisible to every
 * language binding -- which for a library whose selling point is GObject
 * Introspection is a hole, not a detail.
 *
 * The tests also pin down enum-versus-flags: a bitmask registered with
 * g_enum_register_static cannot represent two values ORed together, and the
 * failure shows up far from the cause, in a binding.
 */

#include <glib.h>
#include <glib-object.h>

#define ORM_INSIDE
#include "orm.h"
#undef ORM_INSIDE

/*
 * Asserts that TYPE is a registered enum whose value VALUE is spelled
 * NICK, and returns having consumed the class reference.
 */
static void
assert_enum_value (GType        type,
                   gint         value,
                   const gchar *nick)
{
    GEnumClass *klass;
    GEnumValue *entry;

    g_assert_true (G_TYPE_IS_ENUM (type));

    klass = g_type_class_ref (type);
    g_assert_nonnull (klass);

    entry = g_enum_get_value (klass, value);
    g_assert_nonnull (entry);
    g_assert_cmpstr (entry->value_nick, ==, nick);

    g_type_class_unref (klass);
}

/*
 * Same, for a flags type.
 */
static void
assert_flags_value (GType        type,
                    guint        value,
                    const gchar *nick)
{
    GFlagsClass *klass;
    GFlagsValue *entry;

    g_assert_true (G_TYPE_IS_FLAGS (type));

    klass = g_type_class_ref (type);
    g_assert_nonnull (klass);

    entry = g_flags_get_first_value (klass, value);
    g_assert_nonnull (entry);
    g_assert_cmpstr (entry->value_nick, ==, nick);

    g_type_class_unref (klass);
}

static void
test_enums_object_state (void)
{
    assert_enum_value (ORM_TYPE_OBJECT_STATE, ORM_OBJECT_TRANSIENT, "transient");
    assert_enum_value (ORM_TYPE_OBJECT_STATE, ORM_OBJECT_PERSISTENT, "persistent");
    assert_enum_value (ORM_TYPE_OBJECT_STATE, ORM_OBJECT_DETACHED, "detached");
}

static void
test_enums_sort_order (void)
{
    assert_enum_value (ORM_TYPE_SORT_ORDER, ORM_SORT_ASC, "asc");
    assert_enum_value (ORM_TYPE_SORT_ORDER, ORM_SORT_DESC, "desc");
}

static void
test_enums_load_strategy (void)
{
    assert_enum_value (ORM_TYPE_LOAD_STRATEGY, ORM_LOAD_LAZY, "lazy");
    assert_enum_value (ORM_TYPE_LOAD_STRATEGY, ORM_LOAD_JOIN, "join");
}

static void
test_enums_property_flags (void)
{
    assert_flags_value (ORM_TYPE_PROPERTY_FLAGS,
                        ORM_PROPERTY_PRIMARY_KEY, "primary-key");
    assert_flags_value (ORM_TYPE_PROPERTY_FLAGS,
                        ORM_PROPERTY_DEFERRED, "deferred");
}

/*
 * The point of registering a bitmask as flags rather than as an enum: a
 * combination has to decompose into its members.
 */
static void
test_enums_property_flags_combination (void)
{
    GFlagsClass      *klass;
    g_autofree gchar *rendered = NULL;
    guint             combined;

    combined = ORM_PROPERTY_PRIMARY_KEY | ORM_PROPERTY_AUTO_INCREMENT;

    klass = g_type_class_ref (ORM_TYPE_PROPERTY_FLAGS);
    g_assert_nonnull (klass);

    /* First value found in the mask is the lowest set bit. */
    g_assert_cmpstr (g_flags_get_first_value (klass, combined)->value_nick,
                     ==, "primary-key");

    rendered = g_flags_to_string (ORM_TYPE_PROPERTY_FLAGS, combined);
    g_assert_nonnull (rendered);
    g_assert_nonnull (g_strstr_len (rendered, -1, "ORM_PROPERTY_PRIMARY_KEY"));
    g_assert_nonnull (g_strstr_len (rendered, -1, "ORM_PROPERTY_AUTO_INCREMENT"));

    g_type_class_unref (klass);
}

static void
test_enums_cascade (void)
{
    assert_flags_value (ORM_TYPE_CASCADE, ORM_CASCADE_SAVE, "save");
    assert_flags_value (ORM_TYPE_CASCADE, ORM_CASCADE_DELETE, "delete");

    /* ORM_CASCADE_ALL is SAVE|DELETE, so it must survive a round trip. */
    g_assert_cmpint (ORM_CASCADE_ALL, ==, ORM_CASCADE_SAVE | ORM_CASCADE_DELETE);
}

/*
 * The enums that already had GTypes before this suite existed; here to
 * catch a rename or a dropped registration.
 */
static void
test_enums_preexisting (void)
{
    g_assert_true (G_TYPE_IS_ENUM (ORM_TYPE_DIALECT_TYPE));
    g_assert_true (G_TYPE_IS_ENUM (ORM_TYPE_VALUE_TYPE));
    g_assert_true (G_TYPE_IS_ENUM (ORM_TYPE_ISOLATION_LEVEL));
    g_assert_true (G_TYPE_IS_ENUM (ORM_TYPE_SESSION_STATE));
}

gint
main (gint    argc,
      gchar **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/enums/object-state", test_enums_object_state);
    g_test_add_func ("/enums/sort-order", test_enums_sort_order);
    g_test_add_func ("/enums/load-strategy", test_enums_load_strategy);
    g_test_add_func ("/enums/property-flags", test_enums_property_flags);
    g_test_add_func ("/enums/property-flags/combination",
                     test_enums_property_flags_combination);
    g_test_add_func ("/enums/cascade", test_enums_cascade);
    g_test_add_func ("/enums/preexisting", test_enums_preexisting);

    return g_test_run ();
}
