/* orm-postgres-driver.h
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

#ifndef ORM_POSTGRES_DRIVER_H
#define ORM_POSTGRES_DRIVER_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../orm-driver.h"

G_BEGIN_DECLS

#define ORM_TYPE_POSTGRES_DRIVER (orm_postgres_driver_get_type ())

G_DECLARE_FINAL_TYPE (OrmPostgresDriver, orm_postgres_driver, ORM, POSTGRES_DRIVER, OrmDriver)

/*
 * orm_postgres_driver_new:
 *
 * Creates the PostgreSQL driver.
 *
 * Returns: (transfer full): A new #OrmPostgresDriver
 */
OrmPostgresDriver * orm_postgres_driver_new (void);

G_END_DECLS

#endif /* ORM_POSTGRES_DRIVER_H */
