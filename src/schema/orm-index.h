/* orm-index.h
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

#ifndef ORM_INDEX_H
#define ORM_INDEX_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-types.h"

G_BEGIN_DECLS

#define ORM_TYPE_INDEX (orm_index_get_type ())

G_DECLARE_FINAL_TYPE (OrmIndex, orm_index, ORM, INDEX, GObject)

/**
 * orm_index_new:
 * @name: The index name
 *
 * Creates a new index definition.
 *
 * Returns: (transfer full): A new #OrmIndex
 */
OrmIndex *      orm_index_new               (const gchar *name);

/**
 * orm_index_new_with_columns:
 * @name: The index name
 * @...: NULL-terminated list of column names
 *
 * Creates a new index with the specified columns.
 *
 * Returns: (transfer full): A new #OrmIndex
 */
OrmIndex *      orm_index_new_with_columns  (const gchar *name,
                                             ...) G_GNUC_NULL_TERMINATED;

/* Property accessors */
const gchar *   orm_index_get_name          (OrmIndex *self);
gboolean        orm_index_get_unique        (OrmIndex *self);
void            orm_index_set_unique        (OrmIndex *self,
                                             gboolean  unique);

/* Column management */
void            orm_index_add_column        (OrmIndex    *self,
                                             const gchar *column_name);
GList *         orm_index_get_columns       (OrmIndex *self);
guint           orm_index_get_column_count  (OrmIndex *self);

G_END_DECLS

#endif /* ORM_INDEX_H */
