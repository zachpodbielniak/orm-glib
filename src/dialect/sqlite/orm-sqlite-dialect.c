/* orm-sqlite-dialect.c
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

#include "orm-sqlite-dialect.h"
#include "orm-sqlite-type-compiler.h"
#include "orm-sqlite-ddl-compiler.h"

/*
 * OrmSqliteDialect - SQLite dialect implementation.
 *
 * This dialect provides SQLite-specific SQL generation. Key characteristics:
 * - Dynamic typing with type affinity
 * - Limited schema modification capabilities
 * - RETURNING clause support (3.35+)
 * - No native boolean type
 * - No sequences (uses AUTOINCREMENT instead)
 */
struct _OrmSqliteDialect
{
    GObject parent_instance;

    OrmSqliteTypeCompiler *type_compiler;
    OrmSqliteDdlCompiler  *ddl_compiler;
};

static void orm_sqlite_dialect_iface_init (OrmDialectInterface *iface);

G_DEFINE_TYPE_WITH_CODE (OrmSqliteDialect, orm_sqlite_dialect, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_DIALECT,
                                                orm_sqlite_dialect_iface_init))

static void
orm_sqlite_dialect_finalize (GObject *object)
{
    OrmSqliteDialect *self = ORM_SQLITE_DIALECT (object);

    g_clear_object (&self->type_compiler);
    g_clear_object (&self->ddl_compiler);

    G_OBJECT_CLASS (orm_sqlite_dialect_parent_class)->finalize (object);
}

static OrmDialectType
orm_sqlite_dialect_get_dialect_type (OrmDialect *dialect)
{
    (void) dialect;
    return ORM_DIALECT_SQLITE;
}

static const gchar *
orm_sqlite_dialect_get_name (OrmDialect *dialect)
{
    (void) dialect;
    return "SQLite";
}

static const gchar *
orm_sqlite_dialect_get_driver_name (OrmDialect *dialect)
{
    (void) dialect;
    return "sqlite";
}

static gchar
orm_sqlite_dialect_get_identifier_quote_char (OrmDialect *dialect)
{
    (void) dialect;
    /* SQLite uses double quotes for identifiers (SQL standard) */
    return '"';
}

static gchar
orm_sqlite_dialect_get_string_quote_char (OrmDialect *dialect)
{
    (void) dialect;
    return '\'';
}

static const gchar *
orm_sqlite_dialect_get_parameter_style (OrmDialect *dialect)
{
    (void) dialect;
    /* SQLite uses ? for positional parameters */
    return "?";
}

static gboolean
orm_sqlite_dialect_supports_returning (OrmDialect *dialect)
{
    (void) dialect;
    /* RETURNING supported since SQLite 3.35.0 (2021-03-12) */
    return TRUE;
}

static gboolean
orm_sqlite_dialect_supports_schemas (OrmDialect *dialect)
{
    (void) dialect;
    /* SQLite doesn't have traditional schemas, only attached databases */
    return FALSE;
}

static gboolean
orm_sqlite_dialect_supports_sequences (OrmDialect *dialect)
{
    (void) dialect;
    /* SQLite uses AUTOINCREMENT instead of sequences */
    return FALSE;
}

static gboolean
orm_sqlite_dialect_supports_autoincrement (OrmDialect *dialect)
{
    (void) dialect;
    return TRUE;
}

static gboolean
orm_sqlite_dialect_supports_boolean_type (OrmDialect *dialect)
{
    (void) dialect;
    /* SQLite has no native boolean, uses INTEGER 0/1 */
    return FALSE;
}

static gpointer
orm_sqlite_dialect_get_type_compiler (OrmDialect *dialect)
{
    OrmSqliteDialect *self = ORM_SQLITE_DIALECT (dialect);

    if (self->type_compiler == NULL)
    {
        self->type_compiler = orm_sqlite_type_compiler_new ();
    }

    return self->type_compiler;
}

static gpointer
orm_sqlite_dialect_get_ddl_compiler (OrmDialect *dialect)
{
    OrmSqliteDialect *self = ORM_SQLITE_DIALECT (dialect);

    if (self->ddl_compiler == NULL)
    {
        self->ddl_compiler = orm_sqlite_ddl_compiler_new ();
    }

    return self->ddl_compiler;
}

static void
orm_sqlite_dialect_iface_init (OrmDialectInterface *iface)
{
    iface->get_dialect_type = orm_sqlite_dialect_get_dialect_type;
    iface->get_name = orm_sqlite_dialect_get_name;
    iface->get_driver_name = orm_sqlite_dialect_get_driver_name;
    iface->get_identifier_quote_char = orm_sqlite_dialect_get_identifier_quote_char;
    iface->get_string_quote_char = orm_sqlite_dialect_get_string_quote_char;
    iface->get_parameter_style = orm_sqlite_dialect_get_parameter_style;
    iface->supports_returning = orm_sqlite_dialect_supports_returning;
    iface->supports_schemas = orm_sqlite_dialect_supports_schemas;
    iface->supports_sequences = orm_sqlite_dialect_supports_sequences;
    iface->supports_autoincrement = orm_sqlite_dialect_supports_autoincrement;
    iface->supports_boolean_type = orm_sqlite_dialect_supports_boolean_type;
    iface->get_type_compiler = orm_sqlite_dialect_get_type_compiler;
    iface->get_ddl_compiler = orm_sqlite_dialect_get_ddl_compiler;
    /* quote_identifier and quote_string use default implementation */
}

static void
orm_sqlite_dialect_class_init (OrmSqliteDialectClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_sqlite_dialect_finalize;
}

static void
orm_sqlite_dialect_init (OrmSqliteDialect *self)
{
    self->type_compiler = NULL;
    self->ddl_compiler = NULL;
}

/**
 * orm_sqlite_dialect_new:
 *
 * Creates a new SQLite dialect instance.
 *
 * Returns: (transfer full): A new #OrmSqliteDialect
 */
OrmSqliteDialect *
orm_sqlite_dialect_new (void)
{
    return g_object_new (ORM_TYPE_SQLITE_DIALECT, NULL);
}
