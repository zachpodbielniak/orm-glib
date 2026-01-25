/* query-builder.c
 *
 * Copyright 2025 Zach Pobiel
 *
 * This file is part of orm-glib.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Example: Query Builder with SQLite
 * ===================================
 *
 * Demonstrates OrmQuery operations: filtering, ordering, limit/offset, count.
 */

#include <orm.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/example-models.h"

static void
print_products (const gchar *title, GList *products)
{
    GList *l;
    g_print ("   %s (%u results):\n", title, g_list_length (products));
    for (l = products; l != NULL; l = l->next)
    {
        ExampleProduct *p = EXAMPLE_PRODUCT (l->data);
        g_print ("   - %-20s | %-12s | $%7.2f | stock: %d\n",
                 p->name, p->category, p->price, p->stock);
    }
    g_print ("\n");
}

int
main (int argc, char **argv)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmEngine) engine = NULL;
    g_autoptr(OrmConnection) conn = NULL;
    g_autoptr(OrmSession) session = NULL;
    g_autoptr(OrmMapper) mapper = NULL;

    (void) argc;
    (void) argv;

    g_print ("orm-glib SQLite Query Builder Example\n");
    g_print ("=====================================\n\n");

    /* Setup */
    engine = orm_engine_new ("sqlite:///:memory:", &error);
    if (engine == NULL) { g_printerr ("Error: %s\n", error->message); return 1; }

    conn = orm_engine_connect (engine, &error);
    if (conn == NULL) { g_printerr ("Error: %s\n", error->message); return 1; }

    mapper = orm_mapper_new_from_serializable (EXAMPLE_TYPE_PRODUCT);

    /* Create table */
    {
        g_autoptr(OrmTable) table = orm_mapper_to_table (mapper);
        OrmDialect *dialect = orm_engine_get_dialect (engine);
        OrmDdlCompiler *ddl = ORM_DDL_COMPILER (orm_dialect_get_ddl_compiler (dialect));
        g_autofree gchar *sql = orm_ddl_compiler_compile_create_table (ddl, table, TRUE);
        orm_connection_execute (conn, sql, &error);
    }

    session = orm_session_new_with_connection (conn);
    orm_session_register_mapper (session, mapper);

    /* Seed data */
    g_print ("1. Seeding products...\n\n");
    {
        g_autoptr(ExampleProduct) p1 = example_product_new ("Laptop Pro", "Electronics", 1299.99, 50);
        g_autoptr(ExampleProduct) p2 = example_product_new ("Wireless Mouse", "Electronics", 29.99, 200);
        g_autoptr(ExampleProduct) p3 = example_product_new ("Office Chair", "Furniture", 299.99, 30);
        g_autoptr(ExampleProduct) p4 = example_product_new ("Desk Lamp", "Furniture", 39.99, 80);
        g_autoptr(ExampleProduct) p5 = example_product_new ("Notebook Pack", "Office", 12.99, 500);

        orm_session_add (session, G_OBJECT (p1));
        orm_session_add (session, G_OBJECT (p2));
        orm_session_add (session, G_OBJECT (p3));
        orm_session_add (session, G_OBJECT (p4));
        orm_session_add (session, G_OBJECT (p5));
        orm_session_commit (session, &error);
    }

    /* Query all */
    g_print ("2. Query all products\n");
    {
        g_autoptr(OrmQuery) query = orm_session_query (session, EXAMPLE_TYPE_PRODUCT);
        GList *products = orm_query_all (query, &error);
        print_products ("All", products);
        g_list_free_full (products, g_object_unref);
    }

    /* Filter by category */
    g_print ("3. Filter by category (Electronics)\n");
    {
        g_autoptr(OrmQuery) query = orm_session_query (session, EXAMPLE_TYPE_PRODUCT);
        g_autoptr(OrmValue) cat_val = orm_value_new_string ("Electronics");
        orm_query_filter_by (query, "category", cat_val);
        GList *products = orm_query_all (query, &error);
        print_products ("Electronics", products);
        g_list_free_full (products, g_object_unref);
    }

    /* Filter by price > 100 */
    g_print ("4. Filter by price > $100\n");
    {
        g_autoptr(OrmQuery) query = orm_session_query (session, EXAMPLE_TYPE_PRODUCT);
        g_autoptr(OrmValue) price_val = orm_value_new_float (100.0);
        orm_query_filter (query, "price", ORM_OP_GT, price_val);
        GList *products = orm_query_all (query, &error);
        print_products ("Expensive", products);
        g_list_free_full (products, g_object_unref);
    }

    /* Order by price descending */
    g_print ("5. Order by price (descending)\n");
    {
        g_autoptr(OrmQuery) query = orm_session_query (session, EXAMPLE_TYPE_PRODUCT);
        orm_query_order_by (query, "price", TRUE);
        GList *products = orm_query_all (query, &error);
        print_products ("By price desc", products);
        g_list_free_full (products, g_object_unref);
    }

    /* Limit and offset (pagination) */
    g_print ("6. Pagination: limit 2, offset 1\n");
    {
        g_autoptr(OrmQuery) query = orm_session_query (session, EXAMPLE_TYPE_PRODUCT);
        orm_query_order_by (query, "name", FALSE);
        orm_query_limit (query, 2);
        orm_query_offset (query, 1);
        GList *products = orm_query_all (query, &error);
        print_products ("Page 2 (2 items)", products);
        g_list_free_full (products, g_object_unref);
    }

    /* Count */
    g_print ("7. Count queries\n");
    {
        g_autoptr(OrmQuery) q1 = orm_session_query (session, EXAMPLE_TYPE_PRODUCT);
        gint64 total = orm_query_count (q1, &error);

        g_autoptr(OrmQuery) q2 = orm_session_query (session, EXAMPLE_TYPE_PRODUCT);
        g_autoptr(OrmValue) cat_val = orm_value_new_string ("Electronics");
        orm_query_filter_by (q2, "category", cat_val);
        gint64 electronics = orm_query_count (q2, &error);

        g_print ("   Total products: %ld\n", (long) total);
        g_print ("   Electronics: %ld\n\n", (long) electronics);
    }

    orm_session_close (session);
    g_print ("Example completed!\n");
    return 0;
}
