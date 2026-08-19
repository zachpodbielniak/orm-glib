/* orm-foreign-key.h
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

#ifndef ORM_FOREIGN_KEY_H
#define ORM_FOREIGN_KEY_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-types.h"
#include "../core/orm-enums.h"

G_BEGIN_DECLS

#define ORM_TYPE_FOREIGN_KEY (orm_foreign_key_get_type ())

G_DECLARE_FINAL_TYPE (OrmForeignKey, orm_foreign_key, ORM, FOREIGN_KEY, GObject)

/*
 * orm_foreign_key_new:
 * @name: (nullable): The constraint name
 * @ref_table: The referenced table name
 *
 * Creates a new foreign key constraint.
 *
 * Returns: (transfer full): A new #OrmForeignKey
 */
OrmForeignKey * orm_foreign_key_new             (const gchar *name,
                                                 const gchar *ref_table);

/* Property accessors */
const gchar *   orm_foreign_key_get_name        (OrmForeignKey *self);
void            orm_foreign_key_set_name        (OrmForeignKey *self,
                                                 const gchar   *name);
const gchar *   orm_foreign_key_get_ref_table   (OrmForeignKey *self);

/* Column management */
void            orm_foreign_key_add_column      (OrmForeignKey *self,
                                                 const gchar   *local_column,
                                                 const gchar   *ref_column);
GList *         orm_foreign_key_get_local_columns  (OrmForeignKey *self);
GList *         orm_foreign_key_get_ref_columns    (OrmForeignKey *self);
guint           orm_foreign_key_get_column_count   (OrmForeignKey *self);

/* Referential actions */
OrmForeignKeyAction orm_foreign_key_get_on_delete  (OrmForeignKey *self);
void            orm_foreign_key_set_on_delete   (OrmForeignKey      *self,
                                                 OrmForeignKeyAction action);
OrmForeignKeyAction orm_foreign_key_get_on_update  (OrmForeignKey *self);
void            orm_foreign_key_set_on_update   (OrmForeignKey      *self,
                                                 OrmForeignKeyAction action);

G_END_DECLS

#endif /* ORM_FOREIGN_KEY_H */
