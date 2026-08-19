/* orm-postgres-inspector.h
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

#ifndef ORM_POSTGRES_INSPECTOR_H
#define ORM_POSTGRES_INSPECTOR_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../orm-inspector.h"

G_BEGIN_DECLS

#define ORM_TYPE_POSTGRES_INSPECTOR (orm_postgres_inspector_get_type ())

G_DECLARE_FINAL_TYPE (OrmPostgresInspector, orm_postgres_inspector, ORM, POSTGRES_INSPECTOR, OrmInspector)

/*
 * orm_postgres_inspector_new:
 * @connection: An open #OrmConnection to a PostgreSQL database
 *
 * Creates the PostgreSQL schema inspector.
 *
 * Returns: (transfer full): A new #OrmPostgresInspector
 */
OrmPostgresInspector * orm_postgres_inspector_new (OrmConnection *connection);

G_END_DECLS

#endif /* ORM_POSTGRES_INSPECTOR_H */
