/* orm-driver.c
 *
 * Copyright 2025 Zach Podbielniak
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

#include "orm-driver.h"
#include "../core/orm-error.h"

/*
 * OrmDriver - abstract database backend.
 *
 * Pure forwarding; every method is the subclass's to answer.  The base
 * class deliberately holds no state, because a driver is a shared
 * stateless factory -- see the class documentation.
 */

G_DEFINE_ABSTRACT_TYPE (OrmDriver, orm_driver, G_TYPE_OBJECT)

static void
orm_driver_class_init (OrmDriverClass *klass)
{
}

static void
orm_driver_init (OrmDriver *self)
{
}

/**
 * orm_driver_get_name:
 * @self: An #OrmDriver
 *
 * Gets the driver's short name, such as "sqlite".
 *
 * Returns: (transfer none): The name
 */
const gchar *
orm_driver_get_name (OrmDriver *self)
{
    OrmDriverClass *klass;

    g_return_val_if_fail (ORM_IS_DRIVER (self), NULL);

    klass = ORM_DRIVER_GET_CLASS (self);
    g_return_val_if_fail (klass->get_name != NULL, NULL);

    return klass->get_name (self);
}

/**
 * orm_driver_get_schemes:
 * @self: An #OrmDriver
 *
 * Gets the URL schemes this driver claims, such as "postgresql" and
 * "postgres".
 *
 * Returns: (transfer none) (array zero-terminated=1): The schemes
 */
const gchar * const *
orm_driver_get_schemes (OrmDriver *self)
{
    OrmDriverClass *klass;

    g_return_val_if_fail (ORM_IS_DRIVER (self), NULL);

    klass = ORM_DRIVER_GET_CLASS (self);
    g_return_val_if_fail (klass->get_schemes != NULL, NULL);

    return klass->get_schemes (self);
}

/**
 * orm_driver_get_dialect_type:
 * @self: An #OrmDriver
 *
 * Gets the dialect type this driver generates SQL for.
 *
 * Returns: The #OrmDialectType
 */
OrmDialectType
orm_driver_get_dialect_type (OrmDriver *self)
{
    OrmDriverClass *klass;

    g_return_val_if_fail (ORM_IS_DRIVER (self), ORM_DIALECT_SQLITE);

    klass = ORM_DRIVER_GET_CLASS (self);
    g_return_val_if_fail (klass->get_dialect_type != NULL, ORM_DIALECT_SQLITE);

    return klass->get_dialect_type (self);
}

/**
 * orm_driver_create_dialect:
 * @self: An #OrmDriver
 *
 * Creates a dialect instance for SQL generation.
 *
 * Returns: (transfer full) (nullable): A new #OrmDialect
 */
OrmDialect *
orm_driver_create_dialect (OrmDriver *self)
{
    OrmDriverClass *klass;

    g_return_val_if_fail (ORM_IS_DRIVER (self), NULL);

    klass = ORM_DRIVER_GET_CLASS (self);
    g_return_val_if_fail (klass->create_dialect != NULL, NULL);

    return klass->create_dialect (self);
}

/**
 * orm_driver_open:
 * @self: An #OrmDriver
 * @engine: The engine carrying the connection parameters
 * @error: Return location for error
 *
 * Opens a connection to the database @engine describes.
 *
 * Returns: (transfer full) (nullable): A new #OrmDriverConnection
 */
OrmDriverConnection *
orm_driver_open (OrmDriver  *self,
                 OrmEngine  *engine,
                 GError    **error)
{
    OrmDriverClass *klass;

    g_return_val_if_fail (ORM_IS_DRIVER (self), NULL);
    g_return_val_if_fail (engine != NULL, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    klass = ORM_DRIVER_GET_CLASS (self);
    g_return_val_if_fail (klass->open != NULL, NULL);

    return klass->open (self, engine, error);
}
