/* orm-sqlite-type-compiler.h
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

#ifndef ORM_SQLITE_TYPE_COMPILER_H
#define ORM_SQLITE_TYPE_COMPILER_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../orm-type-compiler.h"

G_BEGIN_DECLS

#define ORM_TYPE_SQLITE_TYPE_COMPILER (orm_sqlite_type_compiler_get_type ())

G_DECLARE_FINAL_TYPE (OrmSqliteTypeCompiler, orm_sqlite_type_compiler, ORM, SQLITE_TYPE_COMPILER, GObject)

/**
 * OrmSqliteTypeCompiler:
 *
 * SQLite type compiler implementation.
 *
 * SQLite type affinity rules:
 * - INTEGER: INT, INTEGER, TINYINT, SMALLINT, MEDIUMINT, BIGINT, etc.
 * - TEXT: CHAR, VARCHAR, TEXT, CLOB
 * - REAL: REAL, DOUBLE, FLOAT
 * - BLOB: BLOB
 * - NUMERIC: Everything else (we don't use this)
 */

OrmSqliteTypeCompiler * orm_sqlite_type_compiler_new (void);

G_END_DECLS

#endif /* ORM_SQLITE_TYPE_COMPILER_H */
