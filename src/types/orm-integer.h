/* orm-integer.h
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

#ifndef ORM_INTEGER_H
#define ORM_INTEGER_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-sql-type.h"

G_BEGIN_DECLS

#define ORM_TYPE_INTEGER (orm_integer_get_type ())

G_DECLARE_FINAL_TYPE (OrmInteger, orm_integer, ORM, INTEGER, OrmSqlType)

/**
 * orm_integer_new:
 *
 * Creates a new integer SQL type. This maps to INTEGER in SQLite,
 * INTEGER in PostgreSQL, and INT in MySQL.
 *
 * Returns: (transfer full): A new #OrmInteger
 */
OrmInteger *    orm_integer_new     (void);

#define ORM_TYPE_BIGINT (orm_bigint_get_type ())

G_DECLARE_FINAL_TYPE (OrmBigInt, orm_bigint, ORM, BIGINT, OrmSqlType)

/**
 * orm_bigint_new:
 *
 * Creates a new bigint SQL type for 64-bit integers. This maps to
 * INTEGER in SQLite, BIGINT in PostgreSQL, and BIGINT in MySQL.
 *
 * Returns: (transfer full): A new #OrmBigInt
 */
OrmBigInt *     orm_bigint_new      (void);

G_END_DECLS

#endif /* ORM_INTEGER_H */
