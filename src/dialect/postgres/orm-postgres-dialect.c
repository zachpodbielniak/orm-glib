/* orm-postgres-dialect.c
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

#include "orm-postgres-dialect.h"
#include "orm-postgres-type-compiler.h"
#include "orm-postgres-ddl-compiler.h"

/*
 * OrmPostgresDialect - PostgreSQL dialect implementation.
 *
 * This dialect provides PostgreSQL-specific SQL generation. Key characteristics:
 * - Strong typing with rich type system
 * - Schema (namespace) support
 * - Sequence support for auto-increment
 * - Native BOOLEAN type
 * - RETURNING clause support
 * - $1, $2 positional parameter style
 */
struct _OrmPostgresDialect
{
    GObject parent_instance;

    OrmPostgresTypeCompiler *type_compiler;
    OrmPostgresDdlCompiler  *ddl_compiler;
};

static void orm_postgres_dialect_iface_init (OrmDialectInterface *iface);

G_DEFINE_TYPE_WITH_CODE (OrmPostgresDialect, orm_postgres_dialect, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_DIALECT,
                                                orm_postgres_dialect_iface_init))

static void
orm_postgres_dialect_finalize (GObject *object)
{
    OrmPostgresDialect *self = ORM_POSTGRES_DIALECT (object);

    g_clear_object (&self->type_compiler);
    g_clear_object (&self->ddl_compiler);

    G_OBJECT_CLASS (orm_postgres_dialect_parent_class)->finalize (object);
}

static OrmDialectType
orm_postgres_dialect_get_dialect_type (OrmDialect *dialect)
{
    (void) dialect;
    return ORM_DIALECT_POSTGRES;
}

static const gchar *
orm_postgres_dialect_get_name (OrmDialect *dialect)
{
    (void) dialect;
    return "PostgreSQL";
}

static const gchar *
orm_postgres_dialect_get_driver_name (OrmDialect *dialect)
{
    (void) dialect;
    return "postgresql";
}

static gchar
orm_postgres_dialect_get_identifier_quote_char (OrmDialect *dialect)
{
    (void) dialect;
    /* PostgreSQL uses double quotes for identifiers (SQL standard) */
    return '"';
}

static gchar
orm_postgres_dialect_get_string_quote_char (OrmDialect *dialect)
{
    (void) dialect;
    return '\'';
}

static const gchar *
orm_postgres_dialect_get_parameter_style (OrmDialect *dialect)
{
    (void) dialect;
    /* PostgreSQL uses $1, $2, etc. for positional parameters */
    return "$";
}

static gboolean
orm_postgres_dialect_supports_returning (OrmDialect *dialect)
{
    (void) dialect;
    /* PostgreSQL has excellent RETURNING support */
    return TRUE;
}

static gboolean
orm_postgres_dialect_supports_schemas (OrmDialect *dialect)
{
    (void) dialect;
    /* PostgreSQL supports schemas (namespaces) */
    return TRUE;
}

static gboolean
orm_postgres_dialect_supports_sequences (OrmDialect *dialect)
{
    (void) dialect;
    /* PostgreSQL supports sequences via CREATE SEQUENCE and SERIAL types */
    return TRUE;
}

static gboolean
orm_postgres_dialect_supports_autoincrement (OrmDialect *dialect)
{
    (void) dialect;
    /* PostgreSQL uses SERIAL/BIGSERIAL which are backed by sequences */
    return TRUE;
}

static gboolean
orm_postgres_dialect_supports_boolean_type (OrmDialect *dialect)
{
    (void) dialect;
    /* PostgreSQL has native BOOLEAN type */
    return TRUE;
}

static gpointer
orm_postgres_dialect_get_type_compiler (OrmDialect *dialect)
{
    OrmPostgresDialect *self = ORM_POSTGRES_DIALECT (dialect);

    if (self->type_compiler == NULL)
    {
        self->type_compiler = orm_postgres_type_compiler_new ();
    }

    return self->type_compiler;
}

static gpointer
orm_postgres_dialect_get_ddl_compiler (OrmDialect *dialect)
{
    OrmPostgresDialect *self = ORM_POSTGRES_DIALECT (dialect);

    if (self->ddl_compiler == NULL)
    {
        self->ddl_compiler = orm_postgres_ddl_compiler_new ();
    }

    return self->ddl_compiler;
}

static void
orm_postgres_dialect_iface_init (OrmDialectInterface *iface)
{
    iface->get_dialect_type = orm_postgres_dialect_get_dialect_type;
    iface->get_name = orm_postgres_dialect_get_name;
    iface->get_driver_name = orm_postgres_dialect_get_driver_name;
    iface->get_identifier_quote_char = orm_postgres_dialect_get_identifier_quote_char;
    iface->get_string_quote_char = orm_postgres_dialect_get_string_quote_char;
    iface->get_parameter_style = orm_postgres_dialect_get_parameter_style;
    iface->supports_returning = orm_postgres_dialect_supports_returning;
    iface->supports_schemas = orm_postgres_dialect_supports_schemas;
    iface->supports_sequences = orm_postgres_dialect_supports_sequences;
    iface->supports_autoincrement = orm_postgres_dialect_supports_autoincrement;
    iface->supports_boolean_type = orm_postgres_dialect_supports_boolean_type;
    iface->get_type_compiler = orm_postgres_dialect_get_type_compiler;
    iface->get_ddl_compiler = orm_postgres_dialect_get_ddl_compiler;
    /* quote_identifier and quote_string use default implementation */
}

static void
orm_postgres_dialect_class_init (OrmPostgresDialectClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_postgres_dialect_finalize;
}

static void
orm_postgres_dialect_init (OrmPostgresDialect *self)
{
    self->type_compiler = NULL;
    self->ddl_compiler = NULL;
}

/**
 * orm_postgres_dialect_new:
 *
 * Creates a new PostgreSQL dialect instance.
 *
 * Returns: (transfer full): A new #OrmPostgresDialect
 */
OrmPostgresDialect *
orm_postgres_dialect_new (void)
{
    return g_object_new (ORM_TYPE_POSTGRES_DIALECT, NULL);
}
