/* orm-float.h
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

#ifndef ORM_FLOAT_H
#define ORM_FLOAT_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-sql-type.h"

G_BEGIN_DECLS

#define ORM_TYPE_FLOAT_TYPE (orm_float_type_get_type ())

G_DECLARE_FINAL_TYPE (OrmFloatType, orm_float_type, ORM, FLOAT_TYPE, OrmSqlType)

/*
 * orm_float_type_new:
 *
 * Creates a new single-precision float SQL type. This maps to REAL in
 * SQLite, REAL in PostgreSQL, and FLOAT in MySQL.
 *
 * Note: Named OrmFloatType to avoid collision with float.
 *
 * Returns: (transfer full): A new #OrmFloatType
 */
OrmFloatType *      orm_float_type_new      (void);

#define ORM_TYPE_DOUBLE_TYPE (orm_double_type_get_type ())

G_DECLARE_FINAL_TYPE (OrmDoubleType, orm_double_type, ORM, DOUBLE_TYPE, OrmSqlType)

/*
 * orm_double_type_new:
 *
 * Creates a new double-precision float SQL type. This maps to REAL in
 * SQLite, DOUBLE PRECISION in PostgreSQL, and DOUBLE in MySQL.
 *
 * Note: Named OrmDoubleType to avoid collision with double.
 *
 * Returns: (transfer full): A new #OrmDoubleType
 */
OrmDoubleType *     orm_double_type_new     (void);

G_END_DECLS

#endif /* ORM_FLOAT_H */
