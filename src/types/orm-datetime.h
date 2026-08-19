/* orm-datetime.h
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

#ifndef ORM_DATETIME_H
#define ORM_DATETIME_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-sql-type.h"

G_BEGIN_DECLS

#define ORM_TYPE_DATETIME_TYPE (orm_datetime_type_get_type ())

G_DECLARE_FINAL_TYPE (OrmDateTimeType, orm_datetime_type, ORM, DATETIME_TYPE, OrmSqlType)

/*
 * orm_datetime_type_new:
 *
 * Creates a new datetime SQL type. This maps to TEXT (ISO8601) in SQLite,
 * TIMESTAMP in PostgreSQL, and DATETIME in MySQL.
 *
 * The datetime values are stored as GDateTime and converted to/from
 * the appropriate database format automatically.
 *
 * Returns: (transfer full): A new #OrmDateTimeType
 */
OrmDateTimeType *   orm_datetime_type_new   (void);

G_END_DECLS

#endif /* ORM_DATETIME_H */
