/* orm-sqlite-ddl-compiler.c
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
 */

#include "orm-sqlite-ddl-compiler.h"
#include "../../core/orm-enums.h"
#include "../../types/orm-sql-type.h"

/*
 * OrmSqliteDdlCompiler - SQLite DDL compiler implementation.
 *
 * Generates SQLite-specific DDL statements for creating and
 * modifying database schema.
 */
struct _OrmSqliteDdlCompiler
{
    GObject parent_instance;
};

static void orm_sqlite_ddl_compiler_iface_init (OrmDdlCompilerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (OrmSqliteDdlCompiler, orm_sqlite_ddl_compiler, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_DDL_COMPILER,
                                                orm_sqlite_ddl_compiler_iface_init))

/*
 * Helper: Quote an identifier for SQLite.
 * SQLite uses double quotes for identifiers.
 */
static gchar *
quote_identifier (const gchar *identifier)
{
    return g_strdup_printf ("\"%s\"", identifier);
}

/*
 * Get SQL type string for a column.
 */
static const gchar *
get_column_type_sql (OrmColumn *column)
{
    OrmSqlType *type;

    type = orm_column_get_sql_type (column);
    if (type == NULL)
    {
        return "TEXT";
    }

    return orm_sql_type_get_name (type, ORM_DIALECT_SQLITE);
}

/*
 * Compile a column definition for CREATE TABLE.
 */
static gchar *
orm_sqlite_ddl_compiler_compile_column_definition (OrmDdlCompiler *compiler,
                                                   OrmColumn      *column)
{
    GString *result;
    g_autofree gchar *quoted_name = NULL;
    const gchar *type_sql;
    const gchar *default_value;

    (void) compiler;

    result = g_string_new (NULL);

    /* Column name */
    quoted_name = quote_identifier (orm_column_get_name (column));
    g_string_append (result, quoted_name);
    g_string_append_c (result, ' ');

    /* Type */
    type_sql = get_column_type_sql (column);
    g_string_append (result, type_sql);

    /* PRIMARY KEY (inline for single-column PKs) */
    if (orm_column_get_primary_key (column))
    {
        g_string_append (result, " PRIMARY KEY");

        /* AUTOINCREMENT only valid with INTEGER PRIMARY KEY */
        if (orm_column_get_autoincrement (column))
        {
            g_string_append (result, " AUTOINCREMENT");
        }
    }

    /* NOT NULL */
    if (!orm_column_get_nullable (column) && !orm_column_get_primary_key (column))
    {
        g_string_append (result, " NOT NULL");
    }

    /* UNIQUE */
    if (orm_column_get_unique (column) && !orm_column_get_primary_key (column))
    {
        g_string_append (result, " UNIQUE");
    }

    /* DEFAULT */
    default_value = orm_column_get_default (column);
    if (default_value != NULL)
    {
        g_string_append (result, " DEFAULT ");
        g_string_append (result, default_value);
    }

    return g_string_free (result, FALSE);
}

/*
 * Compile a PRIMARY KEY constraint.
 */
static gchar *
orm_sqlite_ddl_compiler_compile_primary_key (OrmDdlCompiler *compiler,
                                             OrmPrimaryKey  *pk)
{
    GString *result;
    GList *columns;
    GList *l;
    const gchar *name;
    gboolean first;

    (void) compiler;

    result = g_string_new (NULL);

    /* Optional constraint name */
    name = orm_primary_key_get_name (pk);
    if (name != NULL)
    {
        g_string_append (result, "CONSTRAINT ");
        g_string_append_printf (result, "\"%s\" ", name);
    }

    g_string_append (result, "PRIMARY KEY (");

    /* Column list */
    columns = orm_primary_key_get_columns (pk);
    first = TRUE;
    for (l = columns; l != NULL; l = l->next)
    {
        if (!first)
        {
            g_string_append (result, ", ");
        }
        g_string_append_printf (result, "\"%s\"", (const gchar *) l->data);
        first = FALSE;
    }

    g_string_append_c (result, ')');

    return g_string_free (result, FALSE);
}

/*
 * Compile a FOREIGN KEY constraint.
 */
static gchar *
orm_sqlite_ddl_compiler_compile_foreign_key (OrmDdlCompiler *compiler,
                                             OrmForeignKey  *fk)
{
    GString *result;
    GList *columns;
    GList *ref_columns;
    GList *l;
    const gchar *name;
    const gchar *ref_table;
    OrmForeignKeyAction on_delete;
    OrmForeignKeyAction on_update;
    gboolean first;

    (void) compiler;

    result = g_string_new (NULL);

    /* Optional constraint name */
    name = orm_foreign_key_get_name (fk);
    if (name != NULL)
    {
        g_string_append (result, "CONSTRAINT ");
        g_string_append_printf (result, "\"%s\" ", name);
    }

    g_string_append (result, "FOREIGN KEY (");

    /* Local column list */
    columns = orm_foreign_key_get_local_columns (fk);
    first = TRUE;
    for (l = columns; l != NULL; l = l->next)
    {
        if (!first)
        {
            g_string_append (result, ", ");
        }
        g_string_append_printf (result, "\"%s\"", (const gchar *) l->data);
        first = FALSE;
    }

    g_string_append (result, ") REFERENCES ");

    /* Referenced table */
    ref_table = orm_foreign_key_get_ref_table (fk);
    g_string_append_printf (result, "\"%s\" (", ref_table);

    /* Referenced column list */
    ref_columns = orm_foreign_key_get_ref_columns (fk);
    first = TRUE;
    for (l = ref_columns; l != NULL; l = l->next)
    {
        if (!first)
        {
            g_string_append (result, ", ");
        }
        g_string_append_printf (result, "\"%s\"", (const gchar *) l->data);
        first = FALSE;
    }

    g_string_append_c (result, ')');

    /* ON DELETE action */
    on_delete = orm_foreign_key_get_on_delete (fk);
    switch (on_delete)
    {
    case ORM_FK_CASCADE:
        g_string_append (result, " ON DELETE CASCADE");
        break;
    case ORM_FK_SET_NULL:
        g_string_append (result, " ON DELETE SET NULL");
        break;
    case ORM_FK_SET_DEFAULT:
        g_string_append (result, " ON DELETE SET DEFAULT");
        break;
    case ORM_FK_RESTRICT:
        g_string_append (result, " ON DELETE RESTRICT");
        break;
    case ORM_FK_NO_ACTION:
    default:
        /* NO ACTION is the default, don't need to specify */
        break;
    }

    /* ON UPDATE action */
    on_update = orm_foreign_key_get_on_update (fk);
    switch (on_update)
    {
    case ORM_FK_CASCADE:
        g_string_append (result, " ON UPDATE CASCADE");
        break;
    case ORM_FK_SET_NULL:
        g_string_append (result, " ON UPDATE SET NULL");
        break;
    case ORM_FK_SET_DEFAULT:
        g_string_append (result, " ON UPDATE SET DEFAULT");
        break;
    case ORM_FK_RESTRICT:
        g_string_append (result, " ON UPDATE RESTRICT");
        break;
    case ORM_FK_NO_ACTION:
    default:
        break;
    }

    return g_string_free (result, FALSE);
}

/*
 * Compile a CREATE TABLE statement.
 */
static gchar *
orm_sqlite_ddl_compiler_compile_create_table (OrmDdlCompiler *compiler,
                                              OrmTable       *table,
                                              gboolean        if_not_exists)
{
    GString *result;
    GList *columns;
    GList *foreign_keys;
    OrmPrimaryKey *pk;
    GList *l;
    gboolean first;
    guint pk_column_count;

    result = g_string_new ("CREATE TABLE ");

    if (if_not_exists)
    {
        g_string_append (result, "IF NOT EXISTS ");
    }

    /* Table name */
    g_string_append_printf (result, "\"%s\" (\n", orm_table_get_name (table));

    /* Columns */
    columns = orm_table_get_columns (table);
    first = TRUE;
    for (l = columns; l != NULL; l = l->next)
    {
        OrmColumn *column = ORM_COLUMN (l->data);
        g_autofree gchar *col_def = NULL;

        if (!first)
        {
            g_string_append (result, ",\n");
        }

        col_def = orm_sqlite_ddl_compiler_compile_column_definition (compiler, column);
        g_string_append (result, "    ");
        g_string_append (result, col_def);
        first = FALSE;
    }

    /* Primary key constraint (if multi-column) */
    pk = orm_table_get_primary_key (table);
    if (pk != NULL)
    {
        pk_column_count = orm_primary_key_get_column_count (pk);
        if (pk_column_count > 1)
        {
            g_autofree gchar *pk_sql = NULL;

            g_string_append (result, ",\n    ");
            pk_sql = orm_sqlite_ddl_compiler_compile_primary_key (compiler, pk);
            g_string_append (result, pk_sql);
        }
    }

    /* Foreign key constraints */
    foreign_keys = orm_table_get_foreign_keys (table);
    for (l = foreign_keys; l != NULL; l = l->next)
    {
        OrmForeignKey *fk = ORM_FOREIGN_KEY (l->data);
        g_autofree gchar *fk_sql = NULL;

        g_string_append (result, ",\n    ");
        fk_sql = orm_sqlite_ddl_compiler_compile_foreign_key (compiler, fk);
        g_string_append (result, fk_sql);
    }

    g_string_append (result, "\n)");

    return g_string_free (result, FALSE);
}

/*
 * Compile a DROP TABLE statement.
 */
static gchar *
orm_sqlite_ddl_compiler_compile_drop_table (OrmDdlCompiler *compiler,
                                            OrmTable       *table,
                                            gboolean        if_exists,
                                            gboolean        cascade)
{
    GString *result;

    (void) compiler;
    (void) cascade; /* SQLite doesn't support CASCADE on DROP TABLE */

    result = g_string_new ("DROP TABLE ");

    if (if_exists)
    {
        g_string_append (result, "IF EXISTS ");
    }

    g_string_append_printf (result, "\"%s\"", orm_table_get_name (table));

    return g_string_free (result, FALSE);
}

/*
 * Compile a CREATE INDEX statement.
 */
static gchar *
orm_sqlite_ddl_compiler_compile_create_index (OrmDdlCompiler *compiler,
                                              OrmIndex       *index,
                                              OrmTable       *table,
                                              gboolean        if_not_exists)
{
    GString *result;
    GList *columns;
    GList *l;
    gboolean first;

    (void) compiler;

    result = g_string_new ("CREATE ");

    if (orm_index_get_unique (index))
    {
        g_string_append (result, "UNIQUE ");
    }

    g_string_append (result, "INDEX ");

    if (if_not_exists)
    {
        g_string_append (result, "IF NOT EXISTS ");
    }

    /* Index name */
    g_string_append_printf (result, "\"%s\" ON \"%s\" (",
                            orm_index_get_name (index),
                            orm_table_get_name (table));

    /* Columns */
    columns = orm_index_get_columns (index);
    first = TRUE;
    for (l = columns; l != NULL; l = l->next)
    {
        if (!first)
        {
            g_string_append (result, ", ");
        }
        g_string_append_printf (result, "\"%s\"", (const gchar *) l->data);
        first = FALSE;
    }

    g_string_append_c (result, ')');

    return g_string_free (result, FALSE);
}

/*
 * Compile a DROP INDEX statement.
 */
static gchar *
orm_sqlite_ddl_compiler_compile_drop_index (OrmDdlCompiler *compiler,
                                            OrmIndex       *index,
                                            OrmTable       *table,
                                            gboolean        if_exists)
{
    GString *result;

    (void) compiler;
    (void) table; /* SQLite doesn't need table name for DROP INDEX */

    result = g_string_new ("DROP INDEX ");

    if (if_exists)
    {
        g_string_append (result, "IF EXISTS ");
    }

    g_string_append_printf (result, "\"%s\"", orm_index_get_name (index));

    return g_string_free (result, FALSE);
}

static void
orm_sqlite_ddl_compiler_iface_init (OrmDdlCompilerInterface *iface)
{
    iface->compile_create_table = orm_sqlite_ddl_compiler_compile_create_table;
    iface->compile_drop_table = orm_sqlite_ddl_compiler_compile_drop_table;
    iface->compile_column_definition = orm_sqlite_ddl_compiler_compile_column_definition;
    iface->compile_primary_key = orm_sqlite_ddl_compiler_compile_primary_key;
    iface->compile_foreign_key = orm_sqlite_ddl_compiler_compile_foreign_key;
    iface->compile_create_index = orm_sqlite_ddl_compiler_compile_create_index;
    iface->compile_drop_index = orm_sqlite_ddl_compiler_compile_drop_index;
}

static void
orm_sqlite_ddl_compiler_class_init (OrmSqliteDdlCompilerClass *klass)
{
    (void) klass;
}

static void
orm_sqlite_ddl_compiler_init (OrmSqliteDdlCompiler *self)
{
    (void) self;
}

/**
 * orm_sqlite_ddl_compiler_new:
 *
 * Creates a new SQLite DDL compiler.
 *
 * Returns: (transfer full): A new #OrmSqliteDdlCompiler
 */
OrmSqliteDdlCompiler *
orm_sqlite_ddl_compiler_new (void)
{
    return g_object_new (ORM_TYPE_SQLITE_DDL_COMPILER, NULL);
}
