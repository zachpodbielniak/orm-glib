/* orm-mysql-dialect.c
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

#include "orm-mysql-dialect.h"
#include "orm-mysql-type-compiler.h"
#include "orm-mysql-ddl-compiler.h"

/*
 * OrmMysqlDialect - MySQL/MariaDB dialect implementation.
 *
 * This dialect provides MySQL-specific SQL generation. Key characteristics:
 * - Backtick identifier quoting (non-standard)
 * - No RETURNING clause support
 * - AUTO_INCREMENT for auto-generated keys
 * - No native boolean (uses TINYINT(1))
 * - Storage engine requirements (InnoDB for FK support)
 */
struct _OrmMysqlDialect
{
    GObject parent_instance;

    OrmMysqlTypeCompiler *type_compiler;
    OrmMysqlDdlCompiler  *ddl_compiler;
};

static void orm_mysql_dialect_iface_init (OrmDialectInterface *iface);

G_DEFINE_TYPE_WITH_CODE (OrmMysqlDialect, orm_mysql_dialect, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_DIALECT,
                                                orm_mysql_dialect_iface_init))

static void
orm_mysql_dialect_finalize (GObject *object)
{
    OrmMysqlDialect *self = ORM_MYSQL_DIALECT (object);

    g_clear_object (&self->type_compiler);
    g_clear_object (&self->ddl_compiler);

    G_OBJECT_CLASS (orm_mysql_dialect_parent_class)->finalize (object);
}

static OrmDialectType
orm_mysql_dialect_get_dialect_type (OrmDialect *dialect)
{
    (void) dialect;
    return ORM_DIALECT_MYSQL;
}

static const gchar *
orm_mysql_dialect_get_name (OrmDialect *dialect)
{
    (void) dialect;
    return "MySQL";
}

static const gchar *
orm_mysql_dialect_get_driver_name (OrmDialect *dialect)
{
    (void) dialect;
    return "mysql";
}

static gchar
orm_mysql_dialect_get_identifier_quote_char (OrmDialect *dialect)
{
    (void) dialect;
    /* MySQL uses backticks for identifier quoting (non-standard) */
    return '`';
}

static gchar
orm_mysql_dialect_get_string_quote_char (OrmDialect *dialect)
{
    (void) dialect;
    return '\'';
}

static const gchar *
orm_mysql_dialect_get_parameter_style (OrmDialect *dialect)
{
    (void) dialect;
    /* MySQL uses ? for positional parameters */
    return "?";
}

static gboolean
orm_mysql_dialect_supports_returning (OrmDialect *dialect)
{
    (void) dialect;
    /* MySQL doesn't support RETURNING clause (limited support in 8.0.21+) */
    return FALSE;
}

static gboolean
orm_mysql_dialect_supports_schemas (OrmDialect *dialect)
{
    (void) dialect;
    /* MySQL uses databases instead of schemas */
    return FALSE;
}

static gboolean
orm_mysql_dialect_supports_sequences (OrmDialect *dialect)
{
    (void) dialect;
    /* MySQL uses AUTO_INCREMENT instead of sequences */
    return FALSE;
}

static gboolean
orm_mysql_dialect_supports_autoincrement (OrmDialect *dialect)
{
    (void) dialect;
    return TRUE;
}

static gboolean
orm_mysql_dialect_supports_boolean_type (OrmDialect *dialect)
{
    (void) dialect;
    /* MySQL has no native boolean, uses TINYINT(1) */
    return FALSE;
}

static gpointer
orm_mysql_dialect_get_type_compiler (OrmDialect *dialect)
{
    OrmMysqlDialect *self = ORM_MYSQL_DIALECT (dialect);

    if (self->type_compiler == NULL)
    {
        self->type_compiler = orm_mysql_type_compiler_new ();
    }

    return self->type_compiler;
}

static gpointer
orm_mysql_dialect_get_ddl_compiler (OrmDialect *dialect)
{
    OrmMysqlDialect *self = ORM_MYSQL_DIALECT (dialect);

    if (self->ddl_compiler == NULL)
    {
        self->ddl_compiler = orm_mysql_ddl_compiler_new ();
    }

    return self->ddl_compiler;
}

/*
 * MySQL identifier quoting uses backticks, and a backtick inside an
 * identifier is escaped by doubling it -- without that, a name
 * containing one closes its own quoting and the remainder is parsed as
 * SQL.
 */
static gchar *
orm_mysql_dialect_quote_identifier (OrmDialect  *dialect,
                                     const gchar *identifier)
{
    GString     *result;
    const gchar *p;

    (void) dialect;

    result = g_string_new ("`");

    for (p = identifier; *p != '\0'; p++)
    {
        if (*p == '`')
            g_string_append_c (result, '`');

        g_string_append_c (result, *p);
    }

    g_string_append_c (result, '`');

    return g_string_free (result, FALSE);
}

static void
orm_mysql_dialect_iface_init (OrmDialectInterface *iface)
{
    iface->get_dialect_type = orm_mysql_dialect_get_dialect_type;
    iface->get_name = orm_mysql_dialect_get_name;
    iface->get_driver_name = orm_mysql_dialect_get_driver_name;
    iface->get_identifier_quote_char = orm_mysql_dialect_get_identifier_quote_char;
    iface->get_string_quote_char = orm_mysql_dialect_get_string_quote_char;
    iface->get_parameter_style = orm_mysql_dialect_get_parameter_style;
    iface->supports_returning = orm_mysql_dialect_supports_returning;
    iface->supports_schemas = orm_mysql_dialect_supports_schemas;
    iface->supports_sequences = orm_mysql_dialect_supports_sequences;
    iface->supports_autoincrement = orm_mysql_dialect_supports_autoincrement;
    iface->supports_boolean_type = orm_mysql_dialect_supports_boolean_type;
    iface->get_type_compiler = orm_mysql_dialect_get_type_compiler;
    iface->get_ddl_compiler = orm_mysql_dialect_get_ddl_compiler;
    iface->quote_identifier = orm_mysql_dialect_quote_identifier;
    /* quote_string uses default implementation */
}

static void
orm_mysql_dialect_class_init (OrmMysqlDialectClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_mysql_dialect_finalize;
}

static void
orm_mysql_dialect_init (OrmMysqlDialect *self)
{
    self->type_compiler = NULL;
    self->ddl_compiler = NULL;
}

/**
 * orm_mysql_dialect_new:
 *
 * Creates a new MySQL dialect instance.
 *
 * Returns: (transfer full): A new #OrmMysqlDialect
 */
OrmMysqlDialect *
orm_mysql_dialect_new (void)
{
    return g_object_new (ORM_TYPE_MYSQL_DIALECT, NULL);
}
