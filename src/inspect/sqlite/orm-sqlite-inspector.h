/* orm-sqlite-inspector.h
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

#ifndef ORM_SQLITE_INSPECTOR_H
#define ORM_SQLITE_INSPECTOR_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../orm-inspector.h"

G_BEGIN_DECLS

#define ORM_TYPE_SQLITE_INSPECTOR (orm_sqlite_inspector_get_type ())

G_DECLARE_FINAL_TYPE (OrmSqliteInspector, orm_sqlite_inspector,
                      ORM, SQLITE_INSPECTOR, OrmInspector)

/*
 * orm_sqlite_inspector_new:
 * @connection: An open #OrmConnection to a SQLite database
 *
 * Creates an inspector that reads a SQLite schema from `sqlite_master`
 * and the introspection PRAGMAs.
 *
 * Returns: (transfer full): A new #OrmSqliteInspector
 */
OrmSqliteInspector * orm_sqlite_inspector_new (OrmConnection *connection);

G_END_DECLS

#endif /* ORM_SQLITE_INSPECTOR_H */
