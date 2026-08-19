/* orm-text.h
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

#ifndef ORM_TEXT_H
#define ORM_TEXT_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-sql-type.h"

G_BEGIN_DECLS

#define ORM_TYPE_TEXT (orm_text_get_type ())

G_DECLARE_FINAL_TYPE (OrmText, orm_text, ORM, TEXT, OrmSqlType)

/*
 * orm_text_new:
 *
 * Creates a new text SQL type for large strings. This maps to TEXT
 * in all supported databases. Use this when you need to store large
 * amounts of text that may exceed VARCHAR limits.
 *
 * Returns: (transfer full): A new #OrmText
 */
OrmText *   orm_text_new    (void);

G_END_DECLS

#endif /* ORM_TEXT_H */
