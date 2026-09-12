/* migrations.c
 *
 * Copyright 2025 Zach Pobiel
 *
 * This file is part of orm-glib.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Example: Versioned schema migrations with SQLite
 * ================================================
 *
 * Creates a customers table, inserts a row, then adds a nullable email
 * column without dropping existing data.
 */

#include <orm.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int    argc,
      char **argv)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmEngine) engine = NULL;
    g_autoptr(OrmConnection) conn = NULL;
    g_autoptr(OrmMigration) initial = NULL;
    g_autoptr(OrmMigration) email = NULL;
    g_autoptr(OrmMigrator) migrator = NULL;
    g_autoptr(OrmResult) result = NULL;
    OrmMigration *migrations[2];

    (void) argc;
    (void) argv;

    g_print ("orm-glib SQLite Migrations Example\n");
    g_print ("==================================\n\n");

    engine = orm_engine_new ("sqlite:///:memory:", &error);
    if (engine == NULL)
    {
        g_printerr ("Failed to create engine: %s\n", error->message);
        return EXIT_FAILURE;
    }

    conn = orm_engine_connect (engine, &error);
    if (conn == NULL)
    {
        g_printerr ("Failed to connect: %s\n", error->message);
        return EXIT_FAILURE;
    }

    initial = orm_migration_new (1, "customers",
        "CREATE TABLE customers (id INTEGER PRIMARY KEY, name VARCHAR(200))",
        "DROP TABLE customers");
    email = orm_migration_new (2, "customer email",
        "ALTER TABLE customers ADD COLUMN email VARCHAR(255)",
        "ALTER TABLE customers DROP COLUMN email");
    migrations[0] = initial;
    migrations[1] = email;

    migrator = orm_migrator_new (conn, migrations, 2, &error);
    if (migrator == NULL)
    {
        g_printerr ("Failed to create migrator: %s\n", error->message);
        return EXIT_FAILURE;
    }

    g_print ("1. Applying version 1...\n");
    if (!orm_migrator_up (migrator, 1, &error))
    {
        g_printerr ("Migration to v1 failed: %s\n", error->message);
        return EXIT_FAILURE;
    }

    g_print ("2. Inserting Ada...\n");
    if (!orm_connection_execute (conn,
            "INSERT INTO customers (id, name) VALUES (1, 'Ada')", &error))
    {
        g_printerr ("Insert failed: %s\n", error->message);
        return EXIT_FAILURE;
    }

    g_print ("3. Applying remaining migrations...\n");
    if (!orm_migrator_up (migrator, 0, &error))
    {
        g_printerr ("Migration to latest failed: %s\n", error->message);
        return EXIT_FAILURE;
    }

    result = orm_connection_query (conn,
        "SELECT id, name, email FROM customers", &error);
    if (result == NULL)
    {
        g_printerr ("Query failed: %s\n", error->message);
        return EXIT_FAILURE;
    }

    if (orm_result_next (result))
    {
        OrmRow *row = orm_result_get_row (result);

        g_print ("   id=%" G_GINT64_FORMAT " name=%s email=%s\n",
                 orm_row_get_integer (row, 0),
                 orm_row_get_string (row, 1),
                 orm_row_is_null (row, 2) ? "(null)" :
                     orm_row_get_string (row, 2));
    }

    g_print ("\nDone.\n");
    return EXIT_SUCCESS;
}
