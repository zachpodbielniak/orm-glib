/* basic-crud.c
 *
 * Copyright 2025 Zach Pobiel
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
 *
 * Example: Basic CRUD Operations with SQLite
 * ==========================================
 *
 * This example demonstrates basic Create, Read, Update, Delete operations
 * using orm-glib with SQLite.
 */

#include <orm.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/example-models.h"

int
main (int    argc,
      char **argv)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmEngine) engine = NULL;
    g_autoptr(OrmConnection) conn = NULL;
    g_autoptr(OrmSession) session = NULL;
    g_autoptr(OrmMapper) mapper = NULL;

    (void) argc;
    (void) argv;

    g_print ("orm-glib SQLite Basic CRUD Example\n");
    g_print ("===================================\n\n");

    /*
     * Step 1: Create an engine with SQLite in-memory database
     */
    g_print ("1. Creating SQLite engine...\n");
    engine = orm_engine_new ("sqlite:///:memory:", &error);
    if (engine == NULL)
    {
        g_printerr ("Failed to create engine: %s\n", error->message);
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
     * Step 4: Create the table in the database
     */
    g_print ("4. Creating users table...\n");
    {
        g_autoptr(OrmTable) table = NULL;
        OrmDdlCompiler *ddl = NULL;
        g_autofree gchar *create_sql = NULL;
        OrmDialect *dialect;

        table = orm_mapper_to_table (mapper);
        dialect = orm_engine_get_dialect (engine);
        ddl = ORM_DDL_COMPILER (orm_dialect_get_ddl_compiler (dialect));
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

        /* Mark Charlie as inactive */
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
     * Step 8: READ - Query with filter
     */
    g_print ("\n8. READ - Querying active users only...\n");
    {
        g_autoptr(OrmQuery) query = NULL;
        GList *users = NULL;
        g_autoptr(OrmValue) active_val = NULL;
        GList *l;

        query = orm_session_query (session, EXAMPLE_TYPE_USER);
        active_val = orm_value_new_boolean (TRUE);
        orm_query_filter_by (query, "active", active_val);
        users = orm_query_all (query, &error);

        if (error != NULL)
        {
            g_printerr ("Failed to query: %s\n", error->message);
            return EXIT_FAILURE;
        }

        g_print ("   Found %u active users:\n", g_list_length (users));
        for (l = users; l != NULL; l = l->next)
        {
            ExampleUser *user = EXAMPLE_USER (l->data);
            g_print ("   - %s <%s>\n", user->name, user->email);
        }

        g_list_free_full (users, g_object_unref);
    }

    /*
     * Step 9: UPDATE - Modify a user
     */
    g_print ("\n9. UPDATE - Modifying Alice's email...\n");
    {
        g_autoptr(OrmQuery) query = NULL;
        g_autoptr(ExampleUser) alice = NULL;
        g_autoptr(OrmValue) name_val = NULL;

        query = orm_session_query (session, EXAMPLE_TYPE_USER);
        name_val = orm_value_new_string ("Alice");
        orm_query_filter_by (query, "name", name_val);
        alice = EXAMPLE_USER (orm_query_first (query, &error));

        if (alice == NULL)
        {
            g_printerr ("Failed to find Alice: %s\n",
                        error ? error->message : "not found");
            return EXIT_FAILURE;
        }

        g_print ("   Before: %s <%s>\n", alice->name, alice->email);

        g_object_set (alice, "email", "alice.smith@example.com", NULL);
        orm_session_add (session, G_OBJECT (alice));

        if (!orm_session_commit (session, &error))
        {
            g_printerr ("Failed to commit: %s\n", error->message);
            return EXIT_FAILURE;
        }

        g_print ("   After:  %s <%s>\n", alice->name, alice->email);
    }

    /*
     * Step 10: DELETE - Remove a user
     */
    g_print ("\n10. DELETE - Removing Bob...\n");
    {
        g_autoptr(OrmQuery) query = NULL;
        g_autoptr(ExampleUser) bob = NULL;
        g_autoptr(OrmValue) name_val = NULL;

        query = orm_session_query (session, EXAMPLE_TYPE_USER);
        name_val = orm_value_new_string ("Bob");
        orm_query_filter_by (query, "name", name_val);
        bob = EXAMPLE_USER (orm_query_first (query, &error));

        if (bob != NULL)
        {
            orm_session_delete (session, G_OBJECT (bob));

            if (!orm_session_commit (session, &error))
            {
                g_printerr ("Failed to commit: %s\n", error->message);
                return EXIT_FAILURE;
            }

            g_print ("   Deleted: %s\n", bob->name);
        }
    }

    /*
     * Step 11: Verify final state
     */
    g_print ("\n11. Final state of users table:\n");
    {
        g_autoptr(OrmQuery) query = NULL;
        GList *users = NULL;
        GList *l;

        query = orm_session_query (session, EXAMPLE_TYPE_USER);
        users = orm_query_all (query, &error);

        for (l = users; l != NULL; l = l->next)
        {
            ExampleUser *user = EXAMPLE_USER (l->data);
            g_print ("   - id=%ld name=%s email=%s active=%s\n",
                     (long) user->id,
                     user->name,
                     user->email,
                     user->active ? "yes" : "no");
        }

        g_list_free_full (users, g_object_unref);
    }

    /*
     * Step 12: Cleanup
     */
    g_print ("\n12. Closing session...\n");
    orm_session_close (session);

    g_print ("\nExample completed successfully!\n");
    return EXIT_SUCCESS;
}
