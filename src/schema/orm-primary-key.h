/* orm-primary-key.h
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

#ifndef ORM_PRIMARY_KEY_H
#define ORM_PRIMARY_KEY_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-types.h"

G_BEGIN_DECLS

#define ORM_TYPE_PRIMARY_KEY (orm_primary_key_get_type ())

G_DECLARE_FINAL_TYPE (OrmPrimaryKey, orm_primary_key, ORM, PRIMARY_KEY, GObject)

/**
 * orm_primary_key_new:
 * @name: (nullable): The constraint name
 *
 * Creates a new primary key constraint.
 *
 * Returns: (transfer full): A new #OrmPrimaryKey
 */
OrmPrimaryKey * orm_primary_key_new             (const gchar *name);

/**
 * orm_primary_key_new_with_columns:
 * @name: (nullable): The constraint name
 * @...: NULL-terminated list of column names
 *
 * Creates a new primary key constraint with the specified columns.
 *
 * Returns: (transfer full): A new #OrmPrimaryKey
 */
OrmPrimaryKey * orm_primary_key_new_with_columns (const gchar *name,
                                                  ...) G_GNUC_NULL_TERMINATED;

/* Property accessors */
const gchar *   orm_primary_key_get_name        (OrmPrimaryKey *self);
void            orm_primary_key_set_name        (OrmPrimaryKey *self,
                                                 const gchar   *name);

/* Column management */
void            orm_primary_key_add_column      (OrmPrimaryKey *self,
                                                 const gchar   *column_name);
GList *         orm_primary_key_get_columns     (OrmPrimaryKey *self);
guint           orm_primary_key_get_column_count (OrmPrimaryKey *self);
gboolean        orm_primary_key_has_column      (OrmPrimaryKey *self,
                                                 const gchar   *column_name);

G_END_DECLS

#endif /* ORM_PRIMARY_KEY_H */
