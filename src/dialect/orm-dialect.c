/* orm-dialect.c
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

#include "orm-dialect.h"
#ifdef ORM_ENABLE_SQLITE
#include "sqlite/orm-sqlite-dialect.h"
#endif

/*
 * OrmDialect interface implementation.
 *
 * This file provides the default implementations for the OrmDialect
 * interface methods. Concrete dialect implementations (SQLite, PostgreSQL,
 * MySQL) override these methods to provide database-specific behavior.
 */

G_DEFINE_INTERFACE (OrmDialect, orm_dialect, G_TYPE_OBJECT)

/*
 * Default interface initialization.
 * Sets all virtual methods to NULL - implementations must provide them.
 */
static void
orm_dialect_default_init (OrmDialectInterface *iface)
{
    (void) iface;
}

/**
 * orm_dialect_get_dialect_type:
 * @self: An #OrmDialect
 *
 * Gets the dialect type enumeration value.
 *
 * Returns: The #OrmDialectType for this dialect
 */
OrmDialectType
orm_dialect_get_dialect_type (OrmDialect *self)
{
    OrmDialectInterface *iface;

    g_return_val_if_fail (ORM_IS_DIALECT (self), ORM_DIALECT_SQLITE);

    iface = ORM_DIALECT_GET_IFACE (self);
    g_return_val_if_fail (iface->get_dialect_type != NULL, ORM_DIALECT_SQLITE);

    return iface->get_dialect_type (self);
}

/**
 * orm_dialect_get_name:
 * @self: An #OrmDialect
 *
 * Gets the human-readable name of the dialect.
 *
 * Returns: (transfer none): The dialect name (e.g., "SQLite", "PostgreSQL")
 */
const gchar *
orm_dialect_get_name (OrmDialect *self)
{
    OrmDialectInterface *iface;

    g_return_val_if_fail (ORM_IS_DIALECT (self), NULL);

    iface = ORM_DIALECT_GET_IFACE (self);
    g_return_val_if_fail (iface->get_name != NULL, NULL);

    return iface->get_name (self);
}

/**
 * orm_dialect_get_driver_name:
 * @self: An #OrmDialect
 *
 * Gets the database driver name used in connection URLs.
 *
 * Returns: (transfer none): The driver name (e.g., "sqlite", "postgresql")
 */
const gchar *
orm_dialect_get_driver_name (OrmDialect *self)
{
    OrmDialectInterface *iface;

    g_return_val_if_fail (ORM_IS_DIALECT (self), NULL);

    iface = ORM_DIALECT_GET_IFACE (self);
    g_return_val_if_fail (iface->get_driver_name != NULL, NULL);

    return iface->get_driver_name (self);
}

/**
 * orm_dialect_get_identifier_quote_char:
 * @self: An #OrmDialect
 *
 * Gets the character used to quote identifiers (table names, column names).
 *
 * Returns: The quote character (e.g., '"' for SQL standard, '`' for MySQL)
 */
gchar
orm_dialect_get_identifier_quote_char (OrmDialect *self)
{
    OrmDialectInterface *iface;

    g_return_val_if_fail (ORM_IS_DIALECT (self), '"');

    iface = ORM_DIALECT_GET_IFACE (self);
    g_return_val_if_fail (iface->get_identifier_quote_char != NULL, '"');

    return iface->get_identifier_quote_char (self);
}

/**
 * orm_dialect_get_string_quote_char:
 * @self: An #OrmDialect
 *
 * Gets the character used to quote string literals.
 *
 * Returns: The quote character (typically '\'')
 */
gchar
orm_dialect_get_string_quote_char (OrmDialect *self)
{
    OrmDialectInterface *iface;

    g_return_val_if_fail (ORM_IS_DIALECT (self), '\'');

    iface = ORM_DIALECT_GET_IFACE (self);
    g_return_val_if_fail (iface->get_string_quote_char != NULL, '\'');

    return iface->get_string_quote_char (self);
}

/**
 * orm_dialect_get_parameter_style:
 * @self: An #OrmDialect
 *
 * Gets the parameter placeholder style used in prepared statements.
 *
 * Returns: (transfer none): The parameter style (e.g., "?", "$1", ":name")
 */
const gchar *
orm_dialect_get_parameter_style (OrmDialect *self)
{
    OrmDialectInterface *iface;

    g_return_val_if_fail (ORM_IS_DIALECT (self), "?");

    iface = ORM_DIALECT_GET_IFACE (self);
    g_return_val_if_fail (iface->get_parameter_style != NULL, "?");

    return iface->get_parameter_style (self);
}

/**
 * orm_dialect_supports_returning:
 * @self: An #OrmDialect
 *
 * Checks if the dialect supports the RETURNING clause in INSERT/UPDATE/DELETE.
 *
 * Returns: %TRUE if RETURNING is supported
 */
gboolean
orm_dialect_supports_returning (OrmDialect *self)
{
    OrmDialectInterface *iface;

    g_return_val_if_fail (ORM_IS_DIALECT (self), FALSE);

    iface = ORM_DIALECT_GET_IFACE (self);
    if (iface->supports_returning == NULL)
    {
        return FALSE;
    }

    return iface->supports_returning (self);
}

/**
 * orm_dialect_supports_schemas:
 * @self: An #OrmDialect
 *
 * Checks if the dialect supports schema namespaces.
 *
 * Returns: %TRUE if schemas are supported
 */
gboolean
orm_dialect_supports_schemas (OrmDialect *self)
{
    OrmDialectInterface *iface;

    g_return_val_if_fail (ORM_IS_DIALECT (self), FALSE);

    iface = ORM_DIALECT_GET_IFACE (self);
    if (iface->supports_schemas == NULL)
    {
        return FALSE;
    }

    return iface->supports_schemas (self);
}

/**
 * orm_dialect_supports_sequences:
 * @self: An #OrmDialect
 *
 * Checks if the dialect supports sequences for auto-generated values.
 *
 * Returns: %TRUE if sequences are supported
 */
gboolean
orm_dialect_supports_sequences (OrmDialect *self)
{
    OrmDialectInterface *iface;

    g_return_val_if_fail (ORM_IS_DIALECT (self), FALSE);

    iface = ORM_DIALECT_GET_IFACE (self);
    if (iface->supports_sequences == NULL)
    {
        return FALSE;
    }

    return iface->supports_sequences (self);
}

/**
 * orm_dialect_supports_autoincrement:
 * @self: An #OrmDialect
 *
 * Checks if the dialect supports auto-increment columns.
 *
 * Returns: %TRUE if auto-increment is supported
 */
gboolean
orm_dialect_supports_autoincrement (OrmDialect *self)
{
    OrmDialectInterface *iface;

    g_return_val_if_fail (ORM_IS_DIALECT (self), FALSE);

    iface = ORM_DIALECT_GET_IFACE (self);
    if (iface->supports_autoincrement == NULL)
    {
        return TRUE; /* Most databases support this */
    }

    return iface->supports_autoincrement (self);
}

/**
 * orm_dialect_supports_boolean_type:
 * @self: An #OrmDialect
 *
 * Checks if the dialect has a native boolean type.
 *
 * Returns: %TRUE if native boolean is supported
 */
gboolean
orm_dialect_supports_boolean_type (OrmDialect *self)
{
    OrmDialectInterface *iface;

    g_return_val_if_fail (ORM_IS_DIALECT (self), FALSE);

    iface = ORM_DIALECT_GET_IFACE (self);
    if (iface->supports_boolean_type == NULL)
    {
        return FALSE;
    }

    return iface->supports_boolean_type (self);
}

/**
 * orm_dialect_get_type_compiler:
 * @self: An #OrmDialect
 *
 * Gets the type compiler for this dialect. The type compiler is
 * responsible for rendering SQL type names.
 *
 * Returns: (transfer none): The type compiler instance
 */
gpointer
orm_dialect_get_type_compiler (OrmDialect *self)
{
    OrmDialectInterface *iface;

    g_return_val_if_fail (ORM_IS_DIALECT (self), NULL);

    iface = ORM_DIALECT_GET_IFACE (self);
    g_return_val_if_fail (iface->get_type_compiler != NULL, NULL);

    return iface->get_type_compiler (self);
}

/**
 * orm_dialect_get_ddl_compiler:
 * @self: An #OrmDialect
 *
 * Gets the DDL compiler for this dialect. The DDL compiler is
 * responsible for generating CREATE TABLE, ALTER TABLE, etc.
 *
 * Returns: (transfer none): The DDL compiler instance
 */
gpointer
orm_dialect_get_ddl_compiler (OrmDialect *self)
{
    OrmDialectInterface *iface;

    g_return_val_if_fail (ORM_IS_DIALECT (self), NULL);

    iface = ORM_DIALECT_GET_IFACE (self);
    g_return_val_if_fail (iface->get_ddl_compiler != NULL, NULL);

    return iface->get_ddl_compiler (self);
}

/**
 * orm_dialect_quote_identifier:
 * @self: An #OrmDialect
 * @identifier: The identifier to quote
 *
 * Quotes an identifier (table name, column name) for safe use in SQL.
 *
 * Returns: (transfer full): The quoted identifier
 */
gchar *
orm_dialect_quote_identifier (OrmDialect  *self,
                              const gchar *identifier)
{
    OrmDialectInterface *iface;
    gchar quote_char;

    g_return_val_if_fail (ORM_IS_DIALECT (self), NULL);
    g_return_val_if_fail (identifier != NULL, NULL);

    iface = ORM_DIALECT_GET_IFACE (self);

    /* Use custom implementation if provided */
    if (iface->quote_identifier != NULL)
    {
        return iface->quote_identifier (self, identifier);
    }

    /* Default implementation: surround with quote chars */
    quote_char = orm_dialect_get_identifier_quote_char (self);
    return g_strdup_printf ("%c%s%c", quote_char, identifier, quote_char);
}

/**
 * orm_dialect_quote_string:
 * @self: An #OrmDialect
 * @str: The string to quote
 *
 * Quotes a string literal for safe use in SQL. This includes escaping
 * any special characters within the string.
 *
 * Returns: (transfer full): The quoted string
 */
gchar *
orm_dialect_quote_string (OrmDialect  *self,
                          const gchar *str)
{
    OrmDialectInterface *iface;
    gchar quote_char;
    GString *result;
    const gchar *p;

    g_return_val_if_fail (ORM_IS_DIALECT (self), NULL);
    g_return_val_if_fail (str != NULL, NULL);

    iface = ORM_DIALECT_GET_IFACE (self);

    /* Use custom implementation if provided */
    if (iface->quote_string != NULL)
    {
        return iface->quote_string (self, str);
    }

    /* Default implementation: escape quotes by doubling them */
    quote_char = orm_dialect_get_string_quote_char (self);
    result = g_string_new (NULL);
    g_string_append_c (result, quote_char);

    for (p = str; *p != '\0'; p++)
    {
        if (*p == quote_char)
        {
            /* Escape by doubling the quote character */
            g_string_append_c (result, quote_char);
        }
        g_string_append_c (result, *p);
    }

    g_string_append_c (result, quote_char);

    return g_string_free (result, FALSE);
}

/**
 * orm_dialect_for_type:
 * @type: The dialect type
 *
 * Creates a dialect instance for the specified type. This is a factory
 * function that returns the appropriate dialect implementation.
 *
 * Note: This function requires the dialect implementations to be linked.
 * It will return %NULL if the requested dialect is not available.
 *
 * Returns: (transfer full) (nullable): A new dialect instance, or %NULL
 */
OrmDialect *
orm_dialect_for_type (OrmDialectType type)
{
    switch (type)
    {
    case ORM_DIALECT_SQLITE:
#ifdef ORM_ENABLE_SQLITE
        return ORM_DIALECT (orm_sqlite_dialect_new ());
#else
        g_warning ("SQLite dialect not enabled at compile time");
        return NULL;
#endif

    case ORM_DIALECT_POSTGRES:
#ifdef ORM_ENABLE_POSTGRES
        /* Will be implemented when PostgreSQL dialect is created */
        return NULL;
#else
        g_warning ("PostgreSQL dialect not enabled at compile time");
        return NULL;
#endif

    case ORM_DIALECT_MYSQL:
#ifdef ORM_ENABLE_MYSQL
        /* Will be implemented when MySQL dialect is created */
        return NULL;
#else
        g_warning ("MySQL dialect not enabled at compile time");
        return NULL;
#endif

    default:
        g_warning ("Unknown dialect type: %d", type);
        return NULL;
    }
}
