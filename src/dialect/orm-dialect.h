/* orm-dialect.h
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

#ifndef ORM_DIALECT_H
#define ORM_DIALECT_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-enums.h"

G_BEGIN_DECLS

#define ORM_TYPE_DIALECT (orm_dialect_get_type ())

G_DECLARE_INTERFACE (OrmDialect, orm_dialect, ORM, DIALECT, GObject)

/**
 * OrmDialectInterface:
 * @g_iface: Parent interface
 * @get_dialect_type: Returns the dialect type enum value
 * @get_name: Returns the dialect name string
 * @get_driver_name: Returns the database driver name
 * @get_identifier_quote_char: Returns the character used to quote identifiers
 * @get_string_quote_char: Returns the character used to quote strings
 * @get_parameter_style: Returns the parameter placeholder style
 * @supports_returning: Whether dialect supports RETURNING clause
 * @supports_schemas: Whether dialect supports schema namespaces
 * @supports_sequences: Whether dialect supports sequences
 * @supports_autoincrement: Whether dialect supports auto-increment columns
 * @get_type_compiler: Returns the type compiler for this dialect
 * @get_ddl_compiler: Returns the DDL compiler for this dialect
 *
 * Interface for database dialect implementations. Each supported database
 * (SQLite, PostgreSQL, MySQL) implements this interface to provide
 * database-specific SQL generation.
 */
struct _OrmDialectInterface
{
    GTypeInterface g_iface;

    /* Dialect identification */
    OrmDialectType  (*get_dialect_type)         (OrmDialect *self);
    const gchar *   (*get_name)                 (OrmDialect *self);
    const gchar *   (*get_driver_name)          (OrmDialect *self);

    /* SQL syntax characteristics */
    gchar           (*get_identifier_quote_char)(OrmDialect *self);
    gchar           (*get_string_quote_char)    (OrmDialect *self);
    const gchar *   (*get_parameter_style)      (OrmDialect *self);

    /* Feature support */
    gboolean        (*supports_returning)       (OrmDialect *self);
    gboolean        (*supports_schemas)         (OrmDialect *self);
    gboolean        (*supports_sequences)       (OrmDialect *self);
    gboolean        (*supports_autoincrement)   (OrmDialect *self);
    gboolean        (*supports_boolean_type)    (OrmDialect *self);

    /* Compilers - return types are forward declared */
    gpointer        (*get_type_compiler)        (OrmDialect *self);
    gpointer        (*get_ddl_compiler)         (OrmDialect *self);

    /* SQL generation helpers */
    gchar *         (*quote_identifier)         (OrmDialect  *self,
                                                 const gchar *identifier);
    gchar *         (*quote_string)             (OrmDialect  *self,
                                                 const gchar *str);

    /* Reserved for future expansion */
    gpointer _reserved[8];
};

/* Dialect identification */
OrmDialectType  orm_dialect_get_dialect_type        (OrmDialect *self);
const gchar *   orm_dialect_get_name                (OrmDialect *self);
const gchar *   orm_dialect_get_driver_name         (OrmDialect *self);

/* SQL syntax characteristics */
gchar           orm_dialect_get_identifier_quote_char (OrmDialect *self);
gchar           orm_dialect_get_string_quote_char   (OrmDialect *self);
const gchar *   orm_dialect_get_parameter_style     (OrmDialect *self);

/* Feature support queries */
gboolean        orm_dialect_supports_returning      (OrmDialect *self);
gboolean        orm_dialect_supports_schemas        (OrmDialect *self);
gboolean        orm_dialect_supports_sequences      (OrmDialect *self);
gboolean        orm_dialect_supports_autoincrement  (OrmDialect *self);
gboolean        orm_dialect_supports_boolean_type   (OrmDialect *self);

/* Compilers */
gpointer        orm_dialect_get_type_compiler       (OrmDialect *self);
gpointer        orm_dialect_get_ddl_compiler        (OrmDialect *self);

/* SQL generation helpers */
gchar *         orm_dialect_quote_identifier        (OrmDialect  *self,
                                                     const gchar *identifier);
gchar *         orm_dialect_quote_string            (OrmDialect  *self,
                                                     const gchar *str);

/* Dialect factory */
OrmDialect *    orm_dialect_for_type                (OrmDialectType type);

G_END_DECLS

#endif /* ORM_DIALECT_H */
