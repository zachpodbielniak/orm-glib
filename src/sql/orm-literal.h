/* orm-literal.h
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

#ifndef ORM_LITERAL_H
#define ORM_LITERAL_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-expression.h"
#include "../core/orm-value.h"

G_BEGIN_DECLS

#define ORM_TYPE_LITERAL (orm_literal_get_type ())

G_DECLARE_FINAL_TYPE (OrmLiteral, orm_literal, ORM, LITERAL, OrmExpression)

/*
 * OrmLiteral:
 *
 * Represents a literal value in SQL expressions. The value is wrapped
 * in an OrmValue which tracks its type.
 *
 * When compiled with parameterized queries, literals are replaced with
 * parameter placeholders and the values are collected.
 */

/*
 * orm_literal_new:
 * @value: (transfer full): The value
 *
 * Creates a new literal from an OrmValue.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral * orm_literal_new (OrmValue *value);

/*
 * orm_literal_new_null:
 *
 * Creates a new NULL literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral * orm_literal_new_null (void);

/*
 * orm_literal_new_integer:
 * @value: Integer value
 *
 * Creates a new integer literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral * orm_literal_new_integer (gint64 value);

/*
 * orm_literal_new_float:
 * @value: Float value
 *
 * Creates a new floating point literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral * orm_literal_new_float (gdouble value);

/*
 * orm_literal_new_string:
 * @value: String value
 *
 * Creates a new string literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral * orm_literal_new_string (const gchar *value);

/*
 * orm_literal_new_boolean:
 * @value: Boolean value
 *
 * Creates a new boolean literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral * orm_literal_new_boolean (gboolean value);

/*
 * orm_literal_new_datetime:
 * @value: DateTime value
 *
 * Creates a new datetime literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral * orm_literal_new_datetime (GDateTime *value);

/*
 * orm_literal_new_blob:
 * @value: Blob value
 *
 * Creates a new blob literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral * orm_literal_new_blob (GBytes *value);

/*
 * orm_literal_get_value:
 * @self: A #OrmLiteral
 *
 * Gets the underlying OrmValue.
 *
 * Returns: (transfer none): The value
 */
OrmValue * orm_literal_get_value (OrmLiteral *self);

/*
 * orm_literal_get_value_type:
 * @self: A #OrmLiteral
 *
 * Gets the type of the value.
 *
 * Returns: The value type
 */
OrmValueType orm_literal_get_value_type (OrmLiteral *self);

G_END_DECLS

#endif /* ORM_LITERAL_H */
