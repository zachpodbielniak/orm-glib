/* orm-blob.h
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

#ifndef ORM_BLOB_H
#define ORM_BLOB_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-sql-type.h"

G_BEGIN_DECLS

#define ORM_TYPE_BLOB_TYPE (orm_blob_type_get_type ())

G_DECLARE_FINAL_TYPE (OrmBlobType, orm_blob_type, ORM, BLOB_TYPE, OrmSqlType)

/*
 * orm_blob_type_new:
 *
 * Creates a new binary blob SQL type. This maps to BLOB in SQLite,
 * BYTEA in PostgreSQL, and BLOB in MySQL.
 *
 * Use this type for storing binary data such as images, files, or
 * serialized objects.
 *
 * Note: Named OrmBlobType to maintain naming consistency.
 *
 * Returns: (transfer full): A new #OrmBlobType
 */
OrmBlobType *   orm_blob_type_new   (void);

G_END_DECLS

#endif /* ORM_BLOB_H */
