/* orm-string.h
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

#ifndef ORM_STRING_H
#define ORM_STRING_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-sql-type.h"

G_BEGIN_DECLS

#define ORM_TYPE_STRING (orm_string_get_type ())

G_DECLARE_FINAL_TYPE (OrmString, orm_string, ORM, STRING, OrmSqlType)

/**
 * orm_string_new:
 * @length: Maximum length of the string (0 for unlimited)
 *
 * Creates a new variable-length string SQL type. This maps to TEXT in
 * SQLite, VARCHAR in PostgreSQL and MySQL. If @length is specified,
 * it will be used as the maximum length for databases that support it.
 *
 * Returns: (transfer full): A new #OrmString
 */
OrmString *     orm_string_new          (guint length);

/**
 * orm_string_get_length:
 * @self: An #OrmString
 *
 * Gets the maximum length of the string.
 *
 * Returns: The maximum length, or 0 for unlimited
 */
guint           orm_string_get_length   (OrmString *self);

G_END_DECLS

#endif /* ORM_STRING_H */
