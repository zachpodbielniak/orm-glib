/* orm-mysql-dialect.h
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

#ifndef ORM_MYSQL_DIALECT_H
#define ORM_MYSQL_DIALECT_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../orm-dialect.h"

G_BEGIN_DECLS

#define ORM_TYPE_MYSQL_DIALECT (orm_mysql_dialect_get_type ())

G_DECLARE_FINAL_TYPE (OrmMysqlDialect, orm_mysql_dialect, ORM, MYSQL_DIALECT, GObject)

/**
 * OrmMysqlDialect:
 *
 * MySQL/MariaDB dialect implementation.
 *
 * MySQL characteristics:
 * - Uses ` (backtick) for identifier quoting
 * - Uses ' for string quoting
 * - Uses ? for parameter placeholders
 * - Does not support RETURNING clause (MySQL 8.0.21+ has limited support)
 * - Supports databases (similar to schemas)
 * - Does not support sequences (uses AUTO_INCREMENT instead)
 * - No native BOOLEAN type (uses TINYINT(1))
 * - Uses VARCHAR, CHAR, TEXT, MEDIUMTEXT, LONGTEXT for strings
 * - Uses FLOAT, DOUBLE for floats
 * - Uses BLOB, MEDIUMBLOB, LONGBLOB for binary data
 * - Uses DATETIME, TIMESTAMP for datetime
 * - Requires storage engine specification (InnoDB, MyISAM, etc.)
 */

OrmMysqlDialect *  orm_mysql_dialect_new  (void);

G_END_DECLS

#endif /* ORM_MYSQL_DIALECT_H */
