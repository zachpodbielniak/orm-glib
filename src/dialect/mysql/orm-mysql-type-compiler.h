/* orm-mysql-type-compiler.h
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

#ifndef ORM_MYSQL_TYPE_COMPILER_H
#define ORM_MYSQL_TYPE_COMPILER_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../orm-type-compiler.h"

G_BEGIN_DECLS

#define ORM_TYPE_MYSQL_TYPE_COMPILER (orm_mysql_type_compiler_get_type ())

G_DECLARE_FINAL_TYPE (OrmMysqlTypeCompiler, orm_mysql_type_compiler, ORM, MYSQL_TYPE_COMPILER, GObject)

/**
 * OrmMysqlTypeCompiler:
 *
 * MySQL type compiler implementation.
 *
 * MySQL type mappings:
 * - INT: 32-bit signed integer (-2147483648 to 2147483647)
 * - BIGINT: 64-bit signed integer
 * - SMALLINT: 16-bit signed integer
 * - TINYINT: 8-bit signed integer
 * - TINYINT(1): Used for BOOLEAN values
 * - VARCHAR(n): Variable-length string with max length (1-65535)
 * - TEXT: Variable-length string (up to 65535 bytes)
 * - MEDIUMTEXT: Variable-length string (up to 16MB)
 * - LONGTEXT: Variable-length string (up to 4GB)
 * - FLOAT: 32-bit floating point
 * - DOUBLE: 64-bit floating point
 * - DATETIME: Date and time (without timezone)
 * - TIMESTAMP: Date and time (with automatic conversion to UTC)
 * - BLOB: Binary data (up to 65535 bytes)
 * - MEDIUMBLOB: Binary data (up to 16MB)
 * - LONGBLOB: Binary data (up to 4GB)
 */

OrmMysqlTypeCompiler * orm_mysql_type_compiler_new (void);

G_END_DECLS

#endif /* ORM_MYSQL_TYPE_COMPILER_H */
