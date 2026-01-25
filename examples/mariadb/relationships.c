/* relationships.c
 *
 * Copyright 2025 Zach Pobiel
 *
 * This file is part of orm-glib.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Example: Foreign Key Relationships with MariaDB
 * ================================================
 *
 * Prerequisites:
 *   make container-mariadb-start
 */

#include <orm.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/example-models.h"

#define DEFAULT_MYSQL_URL "mysql://orm_test:orm_test_pass@localhost:13306/orm_test"

int
main (int argc, char **argv)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(OrmEngine) engine = NULL;
    g_autoptr(OrmConnection) conn = NULL;
    g_autoptr(OrmSession) session = NULL;
    g_autoptr(OrmMapper) author_mapper = NULL;
    g_autoptr(OrmMapper) post_mapper = NULL;
    const gchar *url;

    (void) argc;
    (void) argv;

    g_print ("orm-glib MariaDB Relationships Example\n");
    g_print ("=======================================\n\n");

    url = g_getenv ("ORM_MYSQL_URL");
    if (url == NULL) url = DEFAULT_MYSQL_URL;
    g_print ("Connecting to: %s\n\n", url);

    engine = orm_engine_new (url, &error);
    if (engine == NULL)
    {
        g_printerr ("Error: %s\nRun: make container-mariadb-start\n", error->message);
        return 1;
    }

    conn = orm_engine_connect (engine, &error);
    if (conn == NULL) { g_printerr ("Error: %s\n", error->message); return 1; }

    /* Create mappers */
    author_mapper = orm_mapper_new_from_serializable (EXAMPLE_TYPE_AUTHOR);
    post_mapper = orm_mapper_new_from_serializable (EXAMPLE_TYPE_POST);

    /* Create tables */
    g_print ("1. Creating tables with foreign key...\n");
    {
        OrmDialect *dialect = orm_engine_get_dialect (engine);
        OrmDdlCompiler *ddl = ORM_DDL_COMPILER (orm_dialect_get_ddl_compiler (dialect));

        /* Drop tables first (in reverse order due to FK) */
        g_autoptr(OrmTable) posts_table = orm_mapper_to_table (post_mapper);
        g_autoptr(OrmTable) authors_table = orm_mapper_to_table (author_mapper);

        g_autofree gchar *drop_posts = orm_ddl_compiler_compile_drop_table (ddl, posts_table, TRUE, FALSE);
        g_autofree gchar *drop_authors = orm_ddl_compiler_compile_drop_table (ddl, authors_table, TRUE, FALSE);
        orm_connection_execute (conn, drop_posts, NULL);
        orm_connection_execute (conn, drop_authors, NULL);

        /* Authors table */
        g_autofree gchar *sql1 = orm_ddl_compiler_compile_create_table (ddl, authors_table, TRUE);
        g_print ("   %s\n", sql1);
        orm_connection_execute (conn, sql1, &error);

        /* Posts table with FK */
        g_autoptr(OrmForeignKey) fk = orm_foreign_key_new ("fk_posts_author", "authors");
        orm_foreign_key_add_column (fk, "author_id", "id");
        orm_foreign_key_set_on_delete (fk, ORM_FK_CASCADE);
        orm_table_add_foreign_key (posts_table, fk);

        g_autofree gchar *sql2 = orm_ddl_compiler_compile_create_table (ddl, posts_table, TRUE);
        g_print ("   %s\n\n", sql2);
        orm_connection_execute (conn, sql2, &error);
    }

    session = orm_session_new_with_connection (conn);
    orm_session_register_mapper (session, author_mapper);
    orm_session_register_mapper (session, post_mapper);

    /* Create authors */
    g_print ("2. Creating authors...\n");
    {
        g_autoptr(ExampleAuthor) alice = example_author_new ("Alice");
        g_autoptr(ExampleAuthor) bob = example_author_new ("Bob");

        orm_session_add (session, G_OBJECT (alice));
        orm_session_add (session, G_OBJECT (bob));
        orm_session_commit (session, &error);

        g_print ("   Created: Alice (id=%ld), Bob (id=%ld)\n\n", (long) alice->id, (long) bob->id);
    }

    /* Create posts */
    g_print ("3. Creating posts...\n");
    {
        g_autoptr(ExamplePost) p1 = example_post_new ("MariaDB Tutorial", 1);
        g_autoptr(ExamplePost) p2 = example_post_new ("MySQL Optimization", 1);
        g_autoptr(ExamplePost) p3 = example_post_new ("Database Indexing", 2);

        orm_session_add (session, G_OBJECT (p1));
        orm_session_add (session, G_OBJECT (p2));
        orm_session_add (session, G_OBJECT (p3));
        orm_session_commit (session, &error);

        g_print ("   Created 3 posts\n\n");
    }

    /* Query posts by author */
    g_print ("4. Querying Alice's posts...\n");
    {
        g_autoptr(OrmQuery) query = orm_session_query (session, EXAMPLE_TYPE_POST);
        g_autoptr(OrmValue) author_val = orm_value_new_integer (1);
        orm_query_filter_by (query, "author-id", author_val);
        GList *posts = orm_query_all (query, &error);

        g_print ("   Alice has %u posts\n\n", g_list_length (posts));
        g_list_free_full (posts, g_object_unref);
    }

    orm_session_close (session);
    g_print ("Example completed!\n");
    return 0;
}
