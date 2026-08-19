/* orm-property.h
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

#ifndef ORM_PROPERTY_H
#define ORM_PROPERTY_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-value.h"
#include "../types/orm-sql-type.h"

G_BEGIN_DECLS

#define ORM_TYPE_PROPERTY (orm_property_get_type ())

G_DECLARE_FINAL_TYPE (OrmProperty, orm_property, ORM, PROPERTY, GObject)

/**
 * OrmPropertyFlags:
 * @ORM_PROPERTY_NONE: No special flags
 * @ORM_PROPERTY_PRIMARY_KEY: This property is the primary key
 * @ORM_PROPERTY_NULLABLE: This property allows NULL values
 * @ORM_PROPERTY_UNIQUE: This property must have unique values
 * @ORM_PROPERTY_AUTO_INCREMENT: This property auto-increments (for integers)
 * @ORM_PROPERTY_READ_ONLY: This property is read-only (not persisted on update)
 * @ORM_PROPERTY_DEFERRED: This property is loaded lazily
 *
 * Flags for property mapping configuration.
 */
typedef enum {
    ORM_PROPERTY_NONE           = 0,
    ORM_PROPERTY_PRIMARY_KEY    = 1 << 0,
    ORM_PROPERTY_NULLABLE       = 1 << 1,
    ORM_PROPERTY_UNIQUE         = 1 << 2,
    ORM_PROPERTY_AUTO_INCREMENT = 1 << 3,
    ORM_PROPERTY_READ_ONLY      = 1 << 4,
    ORM_PROPERTY_DEFERRED       = 1 << 5
} OrmPropertyFlags;

GType orm_property_flags_get_type (void) G_GNUC_CONST;

#define ORM_TYPE_PROPERTY_FLAGS (orm_property_flags_get_type ())

/**
 * OrmProperty:
 *
 * Maps a GObject property to a database column.
 * Contains the property name, column name, SQL type, and mapping options.
 */

/*
 * orm_property_new:
 * @property_name: The GObject property name
 * @column_name: (nullable): The database column name (defaults to property_name)
 * @sql_type: (transfer none): The SQL type for this column
 * @flags: Property flags
 *
 * Creates a new property mapping.
 *
 * Returns: (transfer full): A new #OrmProperty
 */
OrmProperty *   orm_property_new                (const gchar      *property_name,
                                                 const gchar      *column_name,
                                                 OrmSqlType       *sql_type,
                                                 OrmPropertyFlags  flags);

/*
 * orm_property_new_from_pspec:
 * @pspec: The property specification
 * @column_name: (nullable): The database column name (defaults to property name)
 * @flags: Property flags
 *
 * Creates a new property mapping from a GParamSpec.
 * The SQL type is inferred from the GParamSpec type.
 *
 * Returns: (transfer full): A new #OrmProperty
 */
OrmProperty *   orm_property_new_from_pspec     (GParamSpec       *pspec,
                                                 const gchar      *column_name,
                                                 OrmPropertyFlags  flags);

/*
 * orm_property_get_property_name:
 * @self: An #OrmProperty
 *
 * Gets the GObject property name.
 *
 * Returns: (transfer none): The property name
 */
const gchar *   orm_property_get_property_name  (OrmProperty *self);

/*
 * orm_property_get_column_name:
 * @self: An #OrmProperty
 *
 * Gets the database column name.
 *
 * Returns: (transfer none): The column name
 */
const gchar *   orm_property_get_column_name    (OrmProperty *self);

/*
 * orm_property_get_sql_type:
 * @self: An #OrmProperty
 *
 * Gets the SQL type for this column.
 *
 * Returns: (transfer none): The SQL type
 */
OrmSqlType *    orm_property_get_sql_type       (OrmProperty *self);

/*
 * orm_property_get_flags:
 * @self: An #OrmProperty
 *
 * Gets the property flags.
 *
 * Returns: The property flags
 */
OrmPropertyFlags orm_property_get_flags         (OrmProperty *self);

/*
 * orm_property_is_primary_key:
 * @self: An #OrmProperty
 *
 * Checks if this property is a primary key.
 *
 * Returns: %TRUE if this is a primary key
 */
gboolean        orm_property_is_primary_key     (OrmProperty *self);

/*
 * orm_property_is_nullable:
 * @self: An #OrmProperty
 *
 * Checks if this property allows NULL values.
 *
 * Returns: %TRUE if nullable
 */
gboolean        orm_property_is_nullable        (OrmProperty *self);

/*
 * orm_property_is_unique:
 * @self: An #OrmProperty
 *
 * Checks if this property must have unique values.
 *
 * Returns: %TRUE if unique
 */
gboolean        orm_property_is_unique          (OrmProperty *self);

/*
 * orm_property_is_auto_increment:
 * @self: An #OrmProperty
 *
 * Checks if this property auto-increments.
 *
 * Returns: %TRUE if auto-increment
 */
gboolean        orm_property_is_auto_increment  (OrmProperty *self);

/*
 * orm_property_set_default_value:
 * @self: An #OrmProperty
 * @default_value: (nullable) (transfer none): The default value
 *
 * Sets the default value for this property.
 */
void            orm_property_set_default_value  (OrmProperty *self,
                                                 OrmValue    *default_value);

/*
 * orm_property_get_default_value:
 * @self: An #OrmProperty
 *
 * Gets the default value for this property.
 *
 * Returns: (transfer none) (nullable): The default value
 */
OrmValue *      orm_property_get_default_value  (OrmProperty *self);

/*
 * orm_property_to_column:
 * @self: An #OrmProperty
 *
 * Creates an OrmColumn from this property mapping.
 *
 * Returns: (transfer full): A new #OrmColumn
 */
struct _OrmColumn * orm_property_to_column      (OrmProperty *self);

G_END_DECLS

#endif /* ORM_PROPERTY_H */
