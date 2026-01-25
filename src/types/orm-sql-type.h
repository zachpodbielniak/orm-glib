/* orm-sql-type.h
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

#ifndef ORM_SQL_TYPE_H
#define ORM_SQL_TYPE_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-enums.h"
#include "../core/orm-value.h"

G_BEGIN_DECLS

#define ORM_TYPE_SQL_TYPE (orm_sql_type_get_type ())

G_DECLARE_DERIVABLE_TYPE (OrmSqlType, orm_sql_type, ORM, SQL_TYPE, GObject)

/**
 * OrmSqlTypeClass:
 * @parent_class: Parent class
 * @get_name: Get the SQL type name for a dialect
 * @bind_processor: Process a value before binding to a statement
 * @result_processor: Process a value retrieved from results
 * @compare_values: Compare two values of this type
 *
 * Class structure for #OrmSqlType. Subclasses should override the
 * virtual methods to provide type-specific behavior.
 */
struct _OrmSqlTypeClass
{
    GObjectClass parent_class;

    /**
     * OrmSqlTypeClass::get_name:
     * @self: The SQL type
     * @dialect_type: The target dialect
     *
     * Gets the SQL type name for a specific database dialect.
     *
     * Returns: (transfer none): The SQL type name string
     */
    const gchar *   (*get_name)         (OrmSqlType     *self,
                                         OrmDialectType  dialect_type);

    /**
     * OrmSqlTypeClass::bind_processor:
     * @self: The SQL type
     * @value: The value to process
     *
     * Processes a value before binding to a prepared statement.
     * This can be used to convert application values to database values.
     *
     * Returns: (transfer full) (nullable): The processed value, or %NULL
     *          to use the original value
     */
    OrmValue *      (*bind_processor)   (OrmSqlType     *self,
                                         const OrmValue *value);

    /**
     * OrmSqlTypeClass::result_processor:
     * @self: The SQL type
     * @value: The value from the database
     *
     * Processes a value retrieved from query results.
     * This can be used to convert database values to application values.
     *
     * Returns: (transfer full) (nullable): The processed value, or %NULL
     *          to use the original value
     */
    OrmValue *      (*result_processor) (OrmSqlType     *self,
                                         const OrmValue *value);

    /**
     * OrmSqlTypeClass::compare_values:
     * @self: The SQL type
     * @a: First value
     * @b: Second value
     *
     * Compares two values of this SQL type.
     *
     * Returns: negative if @a < @b, 0 if equal, positive if @a > @b
     */
    gint            (*compare_values)   (OrmSqlType     *self,
                                         const OrmValue *a,
                                         const OrmValue *b);

    /* Reserved for future expansion */
    gpointer _reserved[8];
};

/* Base methods */
const gchar *   orm_sql_type_get_name           (OrmSqlType     *self,
                                                 OrmDialectType  dialect_type);
OrmValue *      orm_sql_type_bind_processor     (OrmSqlType     *self,
                                                 const OrmValue *value);
OrmValue *      orm_sql_type_result_processor   (OrmSqlType     *self,
                                                 const OrmValue *value);
gint            orm_sql_type_compare_values     (OrmSqlType     *self,
                                                 const OrmValue *a,
                                                 const OrmValue *b);

/* Properties */
gboolean        orm_sql_type_get_nullable       (OrmSqlType *self);
void            orm_sql_type_set_nullable       (OrmSqlType *self,
                                                 gboolean    nullable);

/* GType to SQL type mapping */
OrmSqlType *    orm_sql_type_for_gtype          (GType gtype);

G_END_DECLS

#endif /* ORM_SQL_TYPE_H */
