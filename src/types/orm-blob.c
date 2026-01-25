/* orm-blob.c
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

#include "orm-blob.h"
#include "../core/orm-enums.h"

/*
 * OrmBlobType - Binary large object type.
 * Maps to: SQLite BLOB, PostgreSQL BYTEA, MySQL BLOB
 */
struct _OrmBlobType
{
    OrmSqlType parent_instance;
};

G_DEFINE_TYPE (OrmBlobType, orm_blob_type, ORM_TYPE_SQL_TYPE)

/*
 * Returns the SQL type name for the target dialect.
 */
static const gchar *
orm_blob_type_get_name (OrmSqlType     *self,
                        OrmDialectType  dialect_type)
{
    (void) self;

    switch (dialect_type)
    {
    case ORM_DIALECT_SQLITE:
        return "BLOB";
    case ORM_DIALECT_POSTGRES:
        /* PostgreSQL uses BYTEA for binary data */
        return "BYTEA";
    case ORM_DIALECT_MYSQL:
        return "BLOB";
    default:
        return "BLOB";
    }
}

static void
orm_blob_type_class_init (OrmBlobTypeClass *klass)
{
    OrmSqlTypeClass *type_class = ORM_SQL_TYPE_CLASS (klass);

    type_class->get_name = orm_blob_type_get_name;
}

static void
orm_blob_type_init (OrmBlobType *self)
{
    (void) self;
}

/**
 * orm_blob_type_new:
 *
 * Creates a new binary blob SQL type.
 *
 * Returns: (transfer full): A new #OrmBlobType
 */
OrmBlobType *
orm_blob_type_new (void)
{
    return g_object_new (ORM_TYPE_BLOB_TYPE, NULL);
}
