/* orm-column.h
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

#ifndef ORM_COLUMN_H
#define ORM_COLUMN_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-types.h"

G_BEGIN_DECLS

#define ORM_TYPE_COLUMN (orm_column_get_type ())

G_DECLARE_FINAL_TYPE (OrmColumn, orm_column, ORM, COLUMN, GObject)

/**
 * orm_column_new:
 * @name: The column name
 * @type: The SQL type for this column
 *
 * Creates a new column definition.
 *
 * Returns: (transfer full): A new #OrmColumn
 */
OrmColumn *     orm_column_new              (const gchar *name,
                                             OrmSqlType  *type);

/* Property accessors */
const gchar *   orm_column_get_name         (OrmColumn *self);
OrmSqlType *    orm_column_get_sql_type     (OrmColumn *self);
OrmTable *      orm_column_get_table        (OrmColumn *self);

gboolean        orm_column_get_primary_key  (OrmColumn *self);
void            orm_column_set_primary_key  (OrmColumn *self,
                                             gboolean   primary_key);

gboolean        orm_column_get_nullable     (OrmColumn *self);
void            orm_column_set_nullable     (OrmColumn *self,
                                             gboolean   nullable);

gboolean        orm_column_get_unique       (OrmColumn *self);
void            orm_column_set_unique       (OrmColumn *self,
                                             gboolean   unique);

gboolean        orm_column_get_autoincrement (OrmColumn *self);
void            orm_column_set_autoincrement (OrmColumn *self,
                                              gboolean   autoincrement);

const gchar *   orm_column_get_default      (OrmColumn *self);
void            orm_column_set_default      (OrmColumn *self,
                                             const gchar *default_value);

/* Internal use - set by OrmTable */
void            orm_column_set_table        (OrmColumn *self,
                                             OrmTable  *table);

G_END_DECLS

#endif /* ORM_COLUMN_H */
