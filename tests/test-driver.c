/* test-driver.c
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
 * The driver registry, and the promise it makes: a backend is reachable
 * by URL scheme, and adding one out of tree requires nothing but a
 * subclass and a registration call.
 *
 * The out-of-tree case is the one worth testing, because it is the claim
 * that is easy to make and easy to get wrong.  So this file defines a
 * complete fake backend and drives it through the registry -- if that
 * ever stops compiling, the extension point has quietly closed.
 */

#include <glib.h>
#include <glib-object.h>

#define ORM_INSIDE
#include "orm.h"
#undef ORM_INSIDE

/* ------------------------------------------------------------------ */
/* A minimal out-of-tree backend                                      */
/* ------------------------------------------------------------------ */

#define TEST_TYPE_DRIVER (test_driver_get_type ())

G_DECLARE_FINAL_TYPE (TestDriver, test_driver, TEST, DRIVER, OrmDriver)

struct _TestDriver
{
    OrmDriver parent_instance;
};

G_DEFINE_FINAL_TYPE (TestDriver, test_driver, ORM_TYPE_DRIVER)

static const gchar *
test_driver_get_name (OrmDriver *driver)
{
    return "fake";
}

static const gchar * const *
test_driver_get_schemes (OrmDriver *driver)
{
    static const gchar * const schemes[] = { "fake", "fake2", NULL };

    return schemes;
}

static OrmDialectType
test_driver_get_dialect_type (OrmDriver *driver)
{
    /*
     * An out-of-tree backend borrows an in-tree dialect for SQL
     * generation; the dialect enum is still closed.  That limitation is
     * real and documented, and this test exists partly to keep it
     * visible.
     */
    return ORM_DIALECT_SQLITE;
}

static OrmDialect *
test_driver_create_dialect (OrmDriver *driver)
{
    return NULL;
}

static OrmDriverConnection *
test_driver_open (OrmDriver  *driver,
                  OrmEngine  *engine,
                  GError    **error)
{
    g_set_error (error, ORM_ERROR, ORM_ERROR_CONNECTION,
                 "the fake driver never connects");
    return NULL;
}

static void
test_driver_class_init (TestDriverClass *klass)
{
    OrmDriverClass *driver_class = ORM_DRIVER_CLASS (klass);

    driver_class->get_name = test_driver_get_name;
    driver_class->get_schemes = test_driver_get_schemes;
    driver_class->get_dialect_type = test_driver_get_dialect_type;
    driver_class->create_dialect = test_driver_create_dialect;
    driver_class->open = test_driver_open;
}

static void
test_driver_init (TestDriver *self)
{
}

/* A second driver that deliberately collides on one scheme. */

#define TEST_TYPE_COLLIDING_DRIVER (test_colliding_driver_get_type ())

G_DECLARE_FINAL_TYPE (TestCollidingDriver, test_colliding_driver,
                      TEST, COLLIDING_DRIVER, OrmDriver)

struct _TestCollidingDriver
{
    OrmDriver parent_instance;
};

G_DEFINE_FINAL_TYPE (TestCollidingDriver, test_colliding_driver, ORM_TYPE_DRIVER)

static const gchar *
test_colliding_driver_get_name (OrmDriver *driver)
{
    return "collider";
}

static const gchar * const *
test_colliding_driver_get_schemes (OrmDriver *driver)
{
    /* "brandnew" is free; "fake" is not. */
    static const gchar * const schemes[] = { "brandnew", "fake", NULL };

    return schemes;
}

static OrmDialectType
test_colliding_driver_get_dialect_type (OrmDriver *driver)
{
    return ORM_DIALECT_SQLITE;
}

static void
test_colliding_driver_class_init (TestCollidingDriverClass *klass)
{
    OrmDriverClass *driver_class = ORM_DRIVER_CLASS (klass);

    driver_class->get_name = test_colliding_driver_get_name;
    driver_class->get_schemes = test_colliding_driver_get_schemes;
    driver_class->get_dialect_type = test_colliding_driver_get_dialect_type;
}

static void
test_colliding_driver_init (TestCollidingDriver *self)
{
}

/* A driver that claims no schemes at all. */

#define TEST_TYPE_SCHEMELESS_DRIVER (test_schemeless_driver_get_type ())

G_DECLARE_FINAL_TYPE (TestSchemelessDriver, test_schemeless_driver,
                      TEST, SCHEMELESS_DRIVER, OrmDriver)

struct _TestSchemelessDriver
{
    OrmDriver parent_instance;
};

G_DEFINE_FINAL_TYPE (TestSchemelessDriver, test_schemeless_driver, ORM_TYPE_DRIVER)

static const gchar *
test_schemeless_driver_get_name (OrmDriver *driver)
{
    return "schemeless";
}

static const gchar * const *
test_schemeless_driver_get_schemes (OrmDriver *driver)
{
    static const gchar * const schemes[] = { NULL };

    return schemes;
}

static void
test_schemeless_driver_class_init (TestSchemelessDriverClass *klass)
{
    OrmDriverClass *driver_class = ORM_DRIVER_CLASS (klass);

    driver_class->get_name = test_schemeless_driver_get_name;
    driver_class->get_schemes = test_schemeless_driver_get_schemes;
}

static void
test_schemeless_driver_init (TestSchemelessDriver *self)
{
}

/* ------------------------------------------------------------------ */
/* Tests                                                              */
/* ------------------------------------------------------------------ */

static void
test_driver_registry_default_exists (void)
{
    OrmDriverRegistry *registry = orm_driver_registry_get_default ();

    g_assert_nonnull (registry);
    /* The singleton must be a singleton. */
    g_assert_true (registry == orm_driver_registry_get_default ());
}

/*
 * Whichever backends were compiled in must be reachable by their schemes.
 * Compiled out, they must be absent rather than present-but-broken --
 * that distinction is what turns "MySQL is not built" into a clear error
 * instead of a NULL dialect three layers down.
 */
static void
test_driver_registry_builtins (void)
{
    OrmDriverRegistry *registry = orm_driver_registry_get_default ();

#ifdef ORM_ENABLE_SQLITE
    g_assert_nonnull (orm_driver_registry_lookup (registry, "sqlite"));
    g_assert_cmpstr (orm_driver_get_name (
                         orm_driver_registry_lookup (registry, "sqlite")),
                     ==, "sqlite");
#else
    g_assert_null (orm_driver_registry_lookup (registry, "sqlite"));
#endif

#ifdef ORM_ENABLE_POSTGRES
    g_assert_nonnull (orm_driver_registry_lookup (registry, "postgresql"));
    /* Both spellings must reach the same driver instance. */
    g_assert_true (orm_driver_registry_lookup (registry, "postgresql") ==
                   orm_driver_registry_lookup (registry, "postgres"));
#else
    g_assert_null (orm_driver_registry_lookup (registry, "postgresql"));
#endif

#ifdef ORM_ENABLE_MYSQL
    g_assert_nonnull (orm_driver_registry_lookup (registry, "mysql"));
    g_assert_true (orm_driver_registry_lookup (registry, "mysql") ==
                   orm_driver_registry_lookup (registry, "mariadb"));
#else
    g_assert_null (orm_driver_registry_lookup (registry, "mysql"));
#endif
}

static void
test_driver_registry_unknown_scheme (void)
{
    OrmDriverRegistry *registry = orm_driver_registry_get_default ();

    g_assert_null (orm_driver_registry_lookup (registry, "oracle"));
    g_assert_null (orm_driver_registry_lookup (registry, ""));
}

/*
 * The extension-point claim, exercised end to end on a private registry
 * so the process-wide one is not polluted.
 */
static void
test_driver_registry_out_of_tree (void)
{
    g_autoptr(OrmDriverRegistry) registry = NULL;
    g_autoptr(TestDriver)        driver = NULL;
    g_autoptr(GError)            error = NULL;

    registry = g_object_new (ORM_TYPE_DRIVER_REGISTRY, NULL);
    driver = g_object_new (TEST_TYPE_DRIVER, NULL);

    g_assert_true (orm_driver_registry_register (registry, ORM_DRIVER (driver),
                                                 &error));
    g_assert_no_error (error);

    /* Reachable under every scheme it claimed. */
    g_assert_true (orm_driver_registry_lookup (registry, "fake") ==
                   ORM_DRIVER (driver));
    g_assert_true (orm_driver_registry_lookup (registry, "fake2") ==
                   ORM_DRIVER (driver));

    /* And it answers the vtable. */
    g_assert_cmpstr (orm_driver_get_name (ORM_DRIVER (driver)), ==, "fake");
}

/*
 * A scheme collision must fail, and must fail atomically: the colliding
 * driver also claims a free scheme, and that one must NOT be left
 * registered.  A half-registered driver is reachable by some of its names
 * and not others, which is a far worse state than a clean refusal.
 */
static void
test_driver_registry_collision_is_atomic (void)
{
    g_autoptr(OrmDriverRegistry)   registry = NULL;
    g_autoptr(TestDriver)          first = NULL;
    g_autoptr(TestCollidingDriver) second = NULL;
    g_autoptr(GError)              error = NULL;

    registry = g_object_new (ORM_TYPE_DRIVER_REGISTRY, NULL);
    first = g_object_new (TEST_TYPE_DRIVER, NULL);
    second = g_object_new (TEST_TYPE_COLLIDING_DRIVER, NULL);

    g_assert_true (orm_driver_registry_register (registry, ORM_DRIVER (first),
                                                 NULL));

    g_assert_false (orm_driver_registry_register (registry, ORM_DRIVER (second),
                                                  &error));
    g_assert_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION);

    /* The free scheme must not have been taken on the way to failing. */
    g_assert_null (orm_driver_registry_lookup (registry, "brandnew"));

    /* And the original owner still owns the contested one. */
    g_assert_true (orm_driver_registry_lookup (registry, "fake") ==
                   ORM_DRIVER (first));
}

static void
test_driver_registry_list (void)
{
    g_autoptr(OrmDriverRegistry) registry = NULL;
    g_autoptr(TestDriver)        driver = NULL;
    g_autoptr(GPtrArray)         drivers = NULL;
    g_auto(GStrv)                schemes = NULL;

    registry = g_object_new (ORM_TYPE_DRIVER_REGISTRY, NULL);
    driver = g_object_new (TEST_TYPE_DRIVER, NULL);
    g_assert_true (orm_driver_registry_register (registry, ORM_DRIVER (driver),
                                                 NULL));

    /* One entry per driver, not per scheme. */
    drivers = orm_driver_registry_list (registry);
    g_assert_cmpuint (drivers->len, ==, 1);

    /* But every scheme is listed, sorted. */
    schemes = orm_driver_registry_list_schemes (registry);
    g_assert_cmpuint (g_strv_length (schemes), ==, 2);
    g_assert_cmpstr (schemes[0], ==, "fake");
    g_assert_cmpstr (schemes[1], ==, "fake2");
}

/*
 * A driver claiming nothing cannot be looked up by anything, so accepting
 * it would put an unreachable entry in the registry.
 */
static void
test_driver_registry_rejects_schemeless (void)
{
    g_autoptr(OrmDriverRegistry)    registry = NULL;
    g_autoptr(TestSchemelessDriver) driver = NULL;
    g_autoptr(GError)               error = NULL;
    g_autoptr(GPtrArray)            drivers = NULL;

    registry = g_object_new (ORM_TYPE_DRIVER_REGISTRY, NULL);
    driver = g_object_new (TEST_TYPE_SCHEMELESS_DRIVER, NULL);

    g_assert_false (orm_driver_registry_register (registry, ORM_DRIVER (driver),
                                                  &error));
    g_assert_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION);

    drivers = orm_driver_registry_list (registry);
    g_assert_cmpuint (drivers->len, ==, 0);
}

/*
 * The dialect lookup is what the engine uses once a URL has been parsed,
 * so it must agree with the scheme lookup.
 */
static void
test_driver_registry_lookup_dialect (void)
{
    OrmDriverRegistry *registry = orm_driver_registry_get_default ();

#ifdef ORM_ENABLE_SQLITE
    g_assert_true (orm_driver_registry_lookup_dialect (registry,
                                                       ORM_DIALECT_SQLITE) ==
                   orm_driver_registry_lookup (registry, "sqlite"));
#endif
#ifdef ORM_ENABLE_POSTGRES
    g_assert_true (orm_driver_registry_lookup_dialect (registry,
                                                       ORM_DIALECT_POSTGRES) ==
                   orm_driver_registry_lookup (registry, "postgresql"));
#endif
#ifdef ORM_ENABLE_MYSQL
    g_assert_true (orm_driver_registry_lookup_dialect (registry,
                                                       ORM_DIALECT_MYSQL) ==
                   orm_driver_registry_lookup (registry, "mysql"));
#endif
}

gint
main (gint    argc,
      gchar **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/driver/registry/default-exists",
                     test_driver_registry_default_exists);
    g_test_add_func ("/driver/registry/builtins",
                     test_driver_registry_builtins);
    g_test_add_func ("/driver/registry/unknown-scheme",
                     test_driver_registry_unknown_scheme);
    g_test_add_func ("/driver/registry/out-of-tree",
                     test_driver_registry_out_of_tree);
    g_test_add_func ("/driver/registry/collision-is-atomic",
                     test_driver_registry_collision_is_atomic);
    g_test_add_func ("/driver/registry/list", test_driver_registry_list);
    g_test_add_func ("/driver/registry/rejects-schemeless",
                     test_driver_registry_rejects_schemeless);
    g_test_add_func ("/driver/registry/lookup-dialect",
                     test_driver_registry_lookup_dialect);

    return g_test_run ();
}
