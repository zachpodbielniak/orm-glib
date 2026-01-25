/* orm-sqlite-dialect.h
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

#ifndef ORM_SQLITE_DIALECT_H
#define ORM_SQLITE_DIALECT_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../orm-dialect.h"

G_BEGIN_DECLS

#define ORM_TYPE_SQLITE_DIALECT (orm_sqlite_dialect_get_type ())

G_DECLARE_FINAL_TYPE (OrmSqliteDialect, orm_sqlite_dialect, ORM, SQLITE_DIALECT, GObject)

/**
 * OrmSqliteDialect:
 *
 * SQLite dialect implementation.
 *
 * SQLite characteristics:
 * - Uses " for identifier quoting (SQL standard)
 * - Uses ' for string quoting
 * - Uses ? for parameter placeholders
 * - Supports RETURNING (SQLite 3.35+)
 * - Does not support schemas
 * - Does not support sequences (uses AUTOINCREMENT instead)
 * - Does not have native boolean type (uses INTEGER 0/1)
 * - Uses TEXT for all string types
 * - Uses REAL for floating point
 * - Uses BLOB for binary data
 */

OrmSqliteDialect *  orm_sqlite_dialect_new  (void);

G_END_DECLS

#endif /* ORM_SQLITE_DIALECT_H */
