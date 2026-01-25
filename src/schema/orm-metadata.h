/* orm-metadata.h
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

#ifndef ORM_METADATA_H
#define ORM_METADATA_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-types.h"

G_BEGIN_DECLS

#define ORM_TYPE_METADATA (orm_metadata_get_type ())

G_DECLARE_FINAL_TYPE (OrmMetadata, orm_metadata, ORM, METADATA, GObject)

/**
 * orm_metadata_new:
 *
 * Creates a new metadata container for table definitions.
 * Metadata serves as a registry for all table definitions in an
 * application.
 *
 * Returns: (transfer full): A new #OrmMetadata
 */
OrmMetadata *   orm_metadata_new            (void);

/* Table management */
void            orm_metadata_add_table      (OrmMetadata *self,
                                             OrmTable    *table);
OrmTable *      orm_metadata_get_table      (OrmMetadata *self,
                                             const gchar *name);
GList *         orm_metadata_get_tables     (OrmMetadata *self);
guint           orm_metadata_get_table_count (OrmMetadata *self);
gboolean        orm_metadata_has_table      (OrmMetadata *self,
                                             const gchar *name);
void            orm_metadata_remove_table   (OrmMetadata *self,
                                             const gchar *name);

/* DDL operations */
void            orm_metadata_create_all     (OrmMetadata   *self,
                                             OrmConnection *connection,
                                             GError       **error);
void            orm_metadata_drop_all       (OrmMetadata   *self,
                                             OrmConnection *connection,
                                             GError       **error);

/* Reflection - load schema from database */
void            orm_metadata_reflect        (OrmMetadata   *self,
                                             OrmConnection *connection,
                                             GError       **error);

/* Clear all table definitions */
void            orm_metadata_clear          (OrmMetadata *self);

G_END_DECLS

#endif /* ORM_METADATA_H */
