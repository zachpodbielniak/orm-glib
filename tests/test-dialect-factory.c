/* test-dialect-factory.c
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
 * Guards orm_dialect_for_type against the failure it actually had: the
 * MySQL arm returned NULL even with ENABLE_MYSQL=1, so every mysql:// URL
 * died at "Dialect not available" while the dialect class sat there fully
 * implemented.  A factory that compiles in a backend and then refuses to
 * hand it out is worse than one that was never built, because the build
 * flags say the feature is present.
 *
 * Each test is compiled only when its backend is, so this file is a
 * meaningful check in every configuration of the build matrix.
 */

#include <glib.h>
#include <glib-object.h>

#define ORM_INSIDE
#include "orm.h"
#undef ORM_INSIDE

/*
 * Asserts the factory returns a usable dialect for TYPE: non-NULL, of the
 * right dialect type, and able to answer the interface calls every caller
 * makes before it gets anywhere near a database.
 */
static void
assert_factory_yields (OrmDialectType type)
{
    g_autoptr(GObject)  obj = NULL;
    OrmDialect         *dialect;
    g_autofree gchar   *quoted = NULL;

    dialect = orm_dialect_for_type (type);
    g_assert_nonnull (dialect);
    obj = G_OBJECT (dialect);

    g_assert_cmpint (orm_dialect_get_dialect_type (dialect), ==, type);
    g_assert_nonnull (orm_dialect_get_name (dialect));

    quoted = orm_dialect_quote_identifier (dialect, "users");
    g_assert_nonnull (quoted);
    g_assert_nonnull (g_strstr_len (quoted, -1, "users"));
}

#ifdef ORM_ENABLE_SQLITE
static void
test_factory_sqlite (void)
{
    assert_factory_yields (ORM_DIALECT_SQLITE);
}
#endif

#ifdef ORM_ENABLE_POSTGRES
static void
test_factory_postgres (void)
{
    assert_factory_yields (ORM_DIALECT_POSTGRES);
}
#endif

#ifdef ORM_ENABLE_MYSQL
static void
test_factory_mysql (void)
{
    assert_factory_yields (ORM_DIALECT_MYSQL);
}

/*
 * The bug in its original shape: an engine built from a mysql:// URL got
 * as far as dialect construction and stopped.  Reaching engine creation at
 * all proves the factory handed something back.
 */
static void
test_factory_mysql_engine_url (void)
{
    g_autoptr(OrmEngine) engine = NULL;
    g_autoptr(GError)    error = NULL;

    engine = orm_engine_new ("mysql://user:pass@localhost:3306/testdb", &error);

    g_assert_no_error (error);
    g_assert_nonnull (engine);
    g_assert_cmpint (orm_engine_get_dialect_type (engine), ==, ORM_DIALECT_MYSQL);
    g_assert_nonnull (orm_engine_get_dialect (engine));
}
#endif

gint
main (gint    argc,
      gchar **argv)
{
    g_test_init (&argc, &argv, NULL);

#ifdef ORM_ENABLE_SQLITE
    g_test_add_func ("/dialect-factory/sqlite", test_factory_sqlite);
#endif
#ifdef ORM_ENABLE_POSTGRES
    g_test_add_func ("/dialect-factory/postgres", test_factory_postgres);
#endif
#ifdef ORM_ENABLE_MYSQL
    g_test_add_func ("/dialect-factory/mysql", test_factory_mysql);
    g_test_add_func ("/dialect-factory/mysql/engine-url",
                     test_factory_mysql_engine_url);
#endif

    return g_test_run ();
}
