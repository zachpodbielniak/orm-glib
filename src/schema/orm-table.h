/* orm-table.h
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

#ifndef ORM_TABLE_H
#define ORM_TABLE_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-types.h"

G_BEGIN_DECLS

#define ORM_TYPE_TABLE (orm_table_get_type ())

G_DECLARE_FINAL_TYPE (OrmTable, orm_table, ORM, TABLE, GObject)

/**
 * orm_table_new:
 * @name: The table name
 * @metadata: (nullable): The metadata container
 *
 * Creates a new table definition.
 *
 * Returns: (transfer full): A new #OrmTable
 */
OrmTable *      orm_table_new               (const gchar *name,
                                             OrmMetadata *metadata);

/* Property accessors */
const gchar *   orm_table_get_name          (OrmTable *self);
const gchar *   orm_table_get_schema        (OrmTable *self);
void            orm_table_set_schema        (OrmTable    *self,
                                             const gchar *schema);
OrmMetadata *   orm_table_get_metadata      (OrmTable *self);

/* Column management */
void            orm_table_add_column        (OrmTable  *self,
                                             OrmColumn *column);
OrmColumn *     orm_table_get_column        (OrmTable    *self,
                                             const gchar *name);
GList *         orm_table_get_columns       (OrmTable *self);
guint           orm_table_get_column_count  (OrmTable *self);

/* Primary key */
void            orm_table_set_primary_key   (OrmTable      *self,
                                             OrmPrimaryKey *primary_key);
OrmPrimaryKey * orm_table_get_primary_key   (OrmTable *self);

/* Foreign keys */
void            orm_table_add_foreign_key   (OrmTable      *self,
                                             OrmForeignKey *foreign_key);
GList *         orm_table_get_foreign_keys  (OrmTable *self);

/* Indexes */
void            orm_table_add_index         (OrmTable *self,
                                             OrmIndex *index);
GList *         orm_table_get_indexes       (OrmTable *self);

/* Convenience for adding columns with fluent API */
OrmColumn *     orm_table_add_column_full   (OrmTable    *self,
                                             const gchar *name,
                                             OrmSqlType  *type,
                                             gboolean     primary_key,
                                             gboolean     nullable,
                                             gboolean     unique,
                                             gboolean     autoincrement,
                                             const gchar *default_value);

/* Get fully qualified name (schema.table) */
gchar *         orm_table_get_full_name     (OrmTable *self);

G_END_DECLS

#endif /* ORM_TABLE_H */
