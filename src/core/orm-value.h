/* orm-value.h
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

#ifndef ORM_VALUE_H
#define ORM_VALUE_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-enums.h"

G_BEGIN_DECLS

#define ORM_TYPE_VALUE (orm_value_get_type ())

/**
 * OrmValue:
 *
 * Opaque boxed type for database values.
 */
typedef struct _OrmValue OrmValue;

/*
 * OrmValue:
 *
 * A boxed type that wraps database values. This provides a type-safe
 * container for values that can be stored in or retrieved from a database.
 *
 * OrmValue supports the following types:
 * - NULL values
 * - Integers (gint64)
 * - Floating point numbers (gdouble)
 * - Strings (gchar *)
 * - Binary data (GBytes *)
 * - Booleans (gboolean)
 * - DateTime (GDateTime *)
 *
 * The value type can be queried with orm_value_get_value_type().
 */

GType           orm_value_get_type          (void) G_GNUC_CONST;

/* Constructors */
OrmValue *      orm_value_new_null          (void);
OrmValue *      orm_value_new_integer       (gint64          value);
OrmValue *      orm_value_new_float         (gdouble         value);
OrmValue *      orm_value_new_string        (const gchar    *value);
OrmValue *      orm_value_new_blob          (GBytes         *value);
OrmValue *      orm_value_new_boolean       (gboolean        value);
OrmValue *      orm_value_new_datetime      (GDateTime      *value);

/* Copy and free */
OrmValue *      orm_value_copy              (const OrmValue *value);
void            orm_value_free              (OrmValue       *value);

/* Type checking */
OrmValueType    orm_value_get_value_type    (const OrmValue *value);
gboolean        orm_value_is_null           (const OrmValue *value);

/* Getters */
gint64          orm_value_get_integer       (const OrmValue *value);
gdouble         orm_value_get_float         (const OrmValue *value);
const gchar *   orm_value_get_string        (const OrmValue *value);
GBytes *        orm_value_get_blob          (const OrmValue *value);
gboolean        orm_value_get_boolean       (const OrmValue *value);
GDateTime *     orm_value_get_datetime      (const OrmValue *value);

/* Conversion to GValue */
gboolean        orm_value_to_gvalue         (const OrmValue *value,
                                             GValue         *gvalue);
gboolean        orm_value_to_gvalue_with_type (const OrmValue *value,
                                               GValue         *gvalue,
                                               GType           target_type);
OrmValue *      orm_value_from_gvalue       (const GValue   *gvalue);

/* String representation */
gchar *         orm_value_to_string         (const OrmValue *value);

/* Comparison */
gint            orm_value_compare           (const OrmValue *a,
                                             const OrmValue *b);
gboolean        orm_value_equal             (const OrmValue *a,
                                             const OrmValue *b);
guint           orm_value_hash              (const OrmValue *value);

/* Autoptr support */
G_DEFINE_AUTOPTR_CLEANUP_FUNC (OrmValue, orm_value_free)

G_END_DECLS

#endif /* ORM_VALUE_H */
