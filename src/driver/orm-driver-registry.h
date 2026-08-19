/* orm-driver-registry.h
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

#ifndef ORM_DRIVER_REGISTRY_H
#define ORM_DRIVER_REGISTRY_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-driver.h"

G_BEGIN_DECLS

#define ORM_TYPE_DRIVER_REGISTRY (orm_driver_registry_get_type ())

G_DECLARE_FINAL_TYPE (OrmDriverRegistry, orm_driver_registry, ORM, DRIVER_REGISTRY, GObject)

/*
 * orm_driver_registry_get_default:
 *
 * Gets the process-wide driver registry, creating it and registering the
 * built-in drivers on first use.
 *
 * Returns: (transfer none): The registry
 */
OrmDriverRegistry * orm_driver_registry_get_default (void);

/*
 * orm_driver_registry_register:
 * @self: An #OrmDriverRegistry
 * @driver: (transfer none): The driver to add
 * @error: Return location for error
 *
 * Registers @driver under every scheme it claims.  Fails with
 * %ORM_ERROR_INVALID_OPERATION if any of those schemes is already taken,
 * and registers none of them in that case.
 *
 * Returns: %TRUE on success
 */
gboolean orm_driver_registry_register (OrmDriverRegistry  *self,
                                       OrmDriver          *driver,
                                       GError            **error);

/*
 * orm_driver_registry_lookup:
 * @self: An #OrmDriverRegistry
 * @scheme: A URL scheme, e.g. "postgresql"
 *
 * Returns: (transfer none) (nullable): The driver claiming @scheme
 */
OrmDriver * orm_driver_registry_lookup (OrmDriverRegistry *self,
                                        const gchar       *scheme);

/*
 * orm_driver_registry_lookup_dialect:
 * @self: An #OrmDriverRegistry
 * @dialect_type: The dialect type
 *
 * Returns: (transfer none) (nullable): The driver for @dialect_type
 */
OrmDriver * orm_driver_registry_lookup_dialect (OrmDriverRegistry *self,
                                                OrmDialectType     dialect_type);

/*
 * orm_driver_registry_list:
 * @self: An #OrmDriverRegistry
 *
 * Lists the registered drivers, each appearing once however many schemes
 * it claims.
 *
 * Returns: (transfer container) (element-type OrmDriver): The drivers
 */
GPtrArray * orm_driver_registry_list (OrmDriverRegistry *self);

/*
 * orm_driver_registry_list_schemes:
 * @self: An #OrmDriverRegistry
 *
 * Lists every registered URL scheme, sorted.
 *
 * Returns: (transfer full) (array zero-terminated=1): The schemes
 */
gchar ** orm_driver_registry_list_schemes (OrmDriverRegistry *self);

G_END_DECLS

#endif /* ORM_DRIVER_REGISTRY_H */
