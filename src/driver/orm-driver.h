/* orm-driver.h
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

#ifndef ORM_DRIVER_H
#define ORM_DRIVER_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-enums.h"
#include "../dialect/orm-dialect.h"
#include "orm-driver-connection.h"

G_BEGIN_DECLS

#define ORM_TYPE_DRIVER (orm_driver_get_type ())

G_DECLARE_DERIVABLE_TYPE (OrmDriver, orm_driver, ORM, DRIVER, GObject)

/* Forward declaration: the engine carries the connection parameters. */
typedef struct _OrmEngine OrmEngine;

/*
 * OrmDriverClass:
 * @get_name: Short identifier, e.g. "sqlite"
 * @get_schemes: %NULL-terminated URL schemes this driver claims
 * @get_dialect_type: The #OrmDialectType this driver generates SQL for
 * @create_dialect: A new dialect instance for SQL generation
 * @open: Connect, using the parameters carried by an #OrmEngine
 *
 * A database backend: everything orm-glib needs in order to talk to one
 * kind of database.
 *
 * A driver is a stateless factory. It is registered once, looked up by
 * URL scheme, and asked to produce dialects and connections; per-database
 * state belongs to the #OrmDriverConnection it opens. That is what makes
 * a single registered instance safe to share.
 *
 * Adding a backend out of tree means subclassing this, subclassing
 * #OrmDriverConnection and #OrmDriverResult, and calling
 * orm_driver_registry_register(). Nothing in the engine, connection or
 * result layers needs to change.
 */
struct _OrmDriverClass
{
    GObjectClass parent_class;

    const gchar *         (*get_name)         (OrmDriver *self);
    const gchar * const * (*get_schemes)      (OrmDriver *self);
    OrmDialectType        (*get_dialect_type) (OrmDriver *self);
    OrmDialect *          (*create_dialect)   (OrmDriver *self);
    OrmDriverConnection * (*open)             (OrmDriver  *self,
                                               OrmEngine  *engine,
                                               GError    **error);

    /*< private >*/
    gpointer _reserved[8];
};

/*
 * orm_driver_get_name:
 * @self: An #OrmDriver
 *
 * Returns: (transfer none): The driver's short name
 */
const gchar * orm_driver_get_name (OrmDriver *self);

/*
 * orm_driver_get_schemes:
 * @self: An #OrmDriver
 *
 * Gets the URL schemes this driver claims.
 *
 * Returns: (transfer none) (array zero-terminated=1): The schemes
 */
const gchar * const * orm_driver_get_schemes (OrmDriver *self);

/*
 * orm_driver_get_dialect_type:
 * @self: An #OrmDriver
 *
 * Returns: The #OrmDialectType this driver generates SQL for
 */
OrmDialectType orm_driver_get_dialect_type (OrmDriver *self);

/*
 * orm_driver_create_dialect:
 * @self: An #OrmDriver
 *
 * Returns: (transfer full) (nullable): A new #OrmDialect
 */
OrmDialect * orm_driver_create_dialect (OrmDriver *self);

/*
 * orm_driver_open:
 * @self: An #OrmDriver
 * @engine: The engine carrying the connection parameters
 * @error: Return location for error
 *
 * Opens a connection to the database @engine describes.
 *
 * Returns: (transfer full) (nullable): A new #OrmDriverConnection
 */
OrmDriverConnection * orm_driver_open (OrmDriver  *self,
                                       OrmEngine  *engine,
                                       GError    **error);

G_END_DECLS

#endif /* ORM_DRIVER_H */
