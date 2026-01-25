/* basic-crud.c
 *
 * Copyright 2025 Zach Pobiel
 *
 * This file is part of orm-glib.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Example: Basic CRUD Operations with MariaDB
 * ============================================
 *
 * This example demonstrates basic Create, Read, Update, Delete operations
 * using orm-glib with MariaDB.
 *
 * Prerequisites:
 *   make container-mariadb-start
 *
 * Or set ORM_MYSQL_URL environment variable to your MariaDB/MySQL URL.
 */

#include <orm.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/example-models.h"

#define DEFAULT_MYSQL_URL "mysql://orm_test:orm_test_pass@localhost:13306/orm_test"

int
main (int    argc,
      char **argv)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmEngine) engine = NULL;
    g_autoptr(OrmConnection) conn = NULL;
    g_autoptr(OrmSession) session = NULL;
    g_autoptr(OrmMapper) mapper = NULL;
    const gchar *url;

    (void) argc;
    (void) argv;

    g_print ("orm-glib MariaDB Basic CRUD Example\n");
    g_print ("====================================\n\n");

    /* Get connection URL from environment or use default */
    url = g_getenv ("ORM_MYSQL_URL");
    if (url == NULL)
    {
        url = DEFAULT_MYSQL_URL;
    }
    g_print ("Connecting to: %s\n\n", url);

    /*
     * Step 1: Create an engine with MariaDB database
     */
    g_print ("1. Creating MariaDB engine...\n");
    engine = orm_engine_new (url, &error);
    if (engine == NULL)
    {
        g_printerr ("Failed to create engine: %s\n", error->message);
        g_printerr ("Make sure MariaDB is running: make container-mariadb-start\n");
        return EXIT_FAILURE;
    }

    /*
     * Step 2: Create a connection
     */
    g_print ("2. Connecting to database...\n");
    conn = orm_engine_connect (engine, &error);
    if (conn == NULL)
    {
        g_printerr ("Failed to connect: %s\n", error->message);
        return EXIT_FAILURE;
    }

    /*
     * Step 3: Create a mapper for our User model
     */
    g_print ("3. Creating mapper for User model...\n");
    mapper = orm_mapper_new_from_serializable (EXAMPLE_TYPE_USER);
    if (mapper == NULL)
    {
        g_printerr ("Failed to create mapper\n");
        return EXIT_FAILURE;
    }

    /*
     * Step 4: Create the table in the database (drop first for clean state)
     */
    g_print ("4. Creating users table...\n");
    {
        g_autoptr(OrmTable) table = NULL;
        OrmDdlCompiler *ddl = NULL;
        g_autofree gchar *drop_sql = NULL;
        g_autofree gchar *create_sql = NULL;
        OrmDialect *dialect;

        table = orm_mapper_to_table (mapper);
        dialect = orm_engine_get_dialect (engine);
        ddl = ORM_DDL_COMPILER (orm_dialect_get_ddl_compiler (dialect));

        /* Drop table if exists for clean state */
        drop_sql = orm_ddl_compiler_compile_drop_table (ddl, table, TRUE, FALSE);
        orm_connection_execute (conn, drop_sql, NULL);

        create_sql = orm_ddl_compiler_compile_create_table (ddl, table, TRUE);
        g_print ("   SQL: %s\n", create_sql);

        if (!orm_connection_execute (conn, create_sql, &error))
        {
            g_printerr ("Failed to create table: %s\n", error->message);
            return EXIT_FAILURE;
        }
    }

    /*
     * Step 5: Create a session for managing objects
     */
    g_print ("5. Creating session...\n");
    session = orm_session_new_with_connection (conn);
    orm_session_register_mapper (session, mapper);

    /*
     * Step 6: CREATE - Add new users
     */
    g_print ("\n6. CREATE - Adding users...\n");
    {
        g_autoptr(ExampleUser) alice = NULL;
        g_autoptr(ExampleUser) bob = NULL;
        g_autoptr(ExampleUser) charlie = NULL;

        alice = example_user_new ("Alice", "alice@example.com");
        bob = example_user_new ("Bob", "bob@example.com");
        charlie = example_user_new ("Charlie", "charlie@example.com");

        g_object_set (charlie, "active", FALSE, NULL);

        orm_session_add (session, G_OBJECT (alice));
        orm_session_add (session, G_OBJECT (bob));
        orm_session_add (session, G_OBJECT (charlie));

        if (!orm_session_commit (session, &error))
        {
            g_printerr ("Failed to commit: %s\n", error->message);
            return EXIT_FAILURE;
        }

        g_print ("   Added: Alice (id=%ld)\n", (long) alice->id);
        g_print ("   Added: Bob (id=%ld)\n", (long) bob->id);
        g_print ("   Added: Charlie (id=%ld, inactive)\n", (long) charlie->id);
    }

    /*
     * Step 7: READ - Query users
     */
    g_print ("\n7. READ - Querying users...\n");
    {
        g_autoptr(OrmQuery) query = NULL;
        GList *users = NULL;
        GList *l;

        query = orm_session_query (session, EXAMPLE_TYPE_USER);
        users = orm_query_all (query, &error);

        if (error != NULL)
        {
            g_printerr ("Failed to query: %s\n", error->message);
            return EXIT_FAILURE;
        }

        g_print ("   Found %u users:\n", g_list_length (users));
        for (l = users; l != NULL; l = l->next)
        {
            ExampleUser *user = EXAMPLE_USER (l->data);
            g_print ("   - %s <%s> (active=%s)\n",
                     user->name, user->email,
                     user->active ? "yes" : "no");
        }

        g_list_free_full (users, g_object_unref);
    }

    /*
     * Step 8: Cleanup
     */
    g_print ("\n8. Closing session...\n");
    orm_session_close (session);

    g_print ("\nExample completed successfully!\n");
    return EXIT_SUCCESS;
}
