/* orm-schema-info.h
 *
 * Copyright 2025 Zach Podbielniak
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

#ifndef ORM_SCHEMA_INFO_H
#define ORM_SCHEMA_INFO_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-enums.h"

G_BEGIN_DECLS

/*
 * The records an OrmInspector reports.
 *
 * Four small boxed types describing what is in a database, kept in one
 * file because they are one vocabulary: they are always used together,
 * they share a shape (immutable, ref-counted, constructed once and only
 * read afterwards), and splitting sixty lines apiece across eight files
 * would make the family harder to read rather than easier.
 *
 * These are deliberately NOT the schema-definition classes (#OrmTable,
 * #OrmColumn and friends). Those describe a schema you are building in
 * order to emit DDL from it, and carry #OrmSqlType instances and a
 * parent #OrmMetadata to do it. These describe a schema that already
 * exists, in the terms the server used, including things the definition
 * classes cannot express -- a view, a backend type name with no
 * #OrmSqlType equivalent, a column default given as a SQL expression.
 * Reflection maps these onto those; a database browser wants these.
 *
 * Every record is immutable after construction, so copying is a ref.
 */

/**
 * OrmRelationKind:
 * @ORM_RELATION_TABLE: An ordinary table
 * @ORM_RELATION_VIEW: A view
 *
 * What kind of relation an #OrmTableInfo describes.
 */
typedef enum {
    ORM_RELATION_TABLE,
    ORM_RELATION_VIEW
} OrmRelationKind;

GType orm_relation_kind_get_type (void) G_GNUC_CONST;

#define ORM_TYPE_RELATION_KIND (orm_relation_kind_get_type ())

/* ---------------------------------------------------------------- */

typedef struct _OrmTableInfo OrmTableInfo;

#define ORM_TYPE_TABLE_INFO (orm_table_info_get_type ())

GType orm_table_info_get_type (void) G_GNUC_CONST;

/*
 * orm_table_info_new:
 * @name: The relation name
 * @schema: (nullable): The schema it lives in
 * @kind: Table or view
 *
 * Returns: (transfer full): A new #OrmTableInfo
 */
OrmTableInfo * orm_table_info_new (const gchar     *name,
                                   const gchar     *schema,
                                   OrmRelationKind  kind);

OrmTableInfo *  orm_table_info_ref        (OrmTableInfo *self);
void            orm_table_info_unref      (OrmTableInfo *self);
const gchar *   orm_table_info_get_name   (OrmTableInfo *self);
const gchar *   orm_table_info_get_schema (OrmTableInfo *self);
OrmRelationKind orm_table_info_get_kind   (OrmTableInfo *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (OrmTableInfo, orm_table_info_unref)

/* ---------------------------------------------------------------- */

typedef struct _OrmColumnInfo OrmColumnInfo;

#define ORM_TYPE_COLUMN_INFO (orm_column_info_get_type ())

GType orm_column_info_get_type (void) G_GNUC_CONST;

/*
 * orm_column_info_new:
 * @name: The column name
 * @type_name: (nullable): The backend's name for the declared type
 * @value_type: The #OrmValueType values in this column decode to
 * @nullable: Whether the column accepts NULL
 * @default_value: (nullable): The default, as SQL text
 * @primary_key: Whether the column is part of the primary key
 * @autoincrement: Whether the server generates the value
 * @ordinal: Zero-based position in the table
 *
 * Returns: (transfer full): A new #OrmColumnInfo
 */
OrmColumnInfo * orm_column_info_new (const gchar  *name,
                                     const gchar  *type_name,
                                     OrmValueType  value_type,
                                     gboolean      nullable,
                                     const gchar  *default_value,
                                     gboolean      primary_key,
                                     gboolean      autoincrement,
                                     gint          ordinal);

OrmColumnInfo * orm_column_info_ref               (OrmColumnInfo *self);
void            orm_column_info_unref             (OrmColumnInfo *self);
const gchar *   orm_column_info_get_name          (OrmColumnInfo *self);
const gchar *   orm_column_info_get_type_name     (OrmColumnInfo *self);
OrmValueType    orm_column_info_get_value_type    (OrmColumnInfo *self);
gboolean        orm_column_info_get_nullable      (OrmColumnInfo *self);
const gchar *   orm_column_info_get_default_value (OrmColumnInfo *self);
gboolean        orm_column_info_get_primary_key   (OrmColumnInfo *self);
gboolean        orm_column_info_get_autoincrement (OrmColumnInfo *self);
gint            orm_column_info_get_ordinal       (OrmColumnInfo *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (OrmColumnInfo, orm_column_info_unref)

/* ---------------------------------------------------------------- */

typedef struct _OrmIndexInfo OrmIndexInfo;

#define ORM_TYPE_INDEX_INFO (orm_index_info_get_type ())

GType orm_index_info_get_type (void) G_GNUC_CONST;

/*
 * orm_index_info_new:
 * @name: The index name
 * @unique: Whether the index enforces uniqueness
 * @columns: (array zero-terminated=1): The indexed columns, in order
 *
 * Returns: (transfer full): A new #OrmIndexInfo
 */
OrmIndexInfo * orm_index_info_new (const gchar        *name,
                                   gboolean            unique,
                                   const gchar *const *columns);

OrmIndexInfo *        orm_index_info_ref         (OrmIndexInfo *self);
void                  orm_index_info_unref       (OrmIndexInfo *self);
const gchar *         orm_index_info_get_name    (OrmIndexInfo *self);
gboolean              orm_index_info_get_unique  (OrmIndexInfo *self);
const gchar * const * orm_index_info_get_columns (OrmIndexInfo *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (OrmIndexInfo, orm_index_info_unref)

/* ---------------------------------------------------------------- */

typedef struct _OrmForeignKeyInfo OrmForeignKeyInfo;

#define ORM_TYPE_FOREIGN_KEY_INFO (orm_foreign_key_info_get_type ())

GType orm_foreign_key_info_get_type (void) G_GNUC_CONST;

/*
 * orm_foreign_key_info_new:
 * @name: (nullable): The constraint name, where the backend gives one
 * @columns: (array zero-terminated=1): The referring columns
 * @ref_table: The referenced table
 * @ref_schema: (nullable): The referenced table's schema
 * @ref_columns: (array zero-terminated=1): The referenced columns
 * @on_delete: What happens to referring rows when a referenced row goes
 * @on_update: What happens when a referenced key changes
 *
 * Returns: (transfer full): A new #OrmForeignKeyInfo
 */
OrmForeignKeyInfo * orm_foreign_key_info_new (const gchar         *name,
                                              const gchar *const  *columns,
                                              const gchar         *ref_table,
                                              const gchar         *ref_schema,
                                              const gchar *const  *ref_columns,
                                              OrmForeignKeyAction  on_delete,
                                              OrmForeignKeyAction  on_update);

OrmForeignKeyInfo *   orm_foreign_key_info_ref             (OrmForeignKeyInfo *self);
void                  orm_foreign_key_info_unref           (OrmForeignKeyInfo *self);
const gchar *         orm_foreign_key_info_get_name        (OrmForeignKeyInfo *self);
const gchar * const * orm_foreign_key_info_get_columns     (OrmForeignKeyInfo *self);
const gchar *         orm_foreign_key_info_get_ref_table   (OrmForeignKeyInfo *self);
const gchar *         orm_foreign_key_info_get_ref_schema  (OrmForeignKeyInfo *self);
const gchar * const * orm_foreign_key_info_get_ref_columns (OrmForeignKeyInfo *self);
OrmForeignKeyAction   orm_foreign_key_info_get_on_delete   (OrmForeignKeyInfo *self);
OrmForeignKeyAction   orm_foreign_key_info_get_on_update   (OrmForeignKeyInfo *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (OrmForeignKeyInfo, orm_foreign_key_info_unref)

G_END_DECLS

#endif /* ORM_SCHEMA_INFO_H */
