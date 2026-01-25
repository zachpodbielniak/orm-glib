/* orm-postgres-dialect.h
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

#ifndef ORM_POSTGRES_DIALECT_H
#define ORM_POSTGRES_DIALECT_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../orm-dialect.h"

G_BEGIN_DECLS

#define ORM_TYPE_POSTGRES_DIALECT (orm_postgres_dialect_get_type ())

G_DECLARE_FINAL_TYPE (OrmPostgresDialect, orm_postgres_dialect, ORM, POSTGRES_DIALECT, GObject)

/**
 * OrmPostgresDialect:
 *
 * PostgreSQL dialect implementation.
 *
 * PostgreSQL characteristics:
 * - Uses " for identifier quoting (SQL standard)
 * - Uses ' for string quoting (with E'...' for escape sequences)
 * - Uses $1, $2, etc. for parameter placeholders
 * - Supports RETURNING clause
 * - Supports schemas (namespaces)
 * - Supports sequences (SERIAL, BIGSERIAL, or CREATE SEQUENCE)
 * - Has native BOOLEAN type
 * - Uses TEXT, VARCHAR, CHAR for strings
 * - Uses REAL, DOUBLE PRECISION for floats
 * - Uses BYTEA for binary data
 * - Uses TIMESTAMP, TIMESTAMPTZ for datetime
 * - Supports arrays, JSON, JSONB, UUID, and other advanced types
 */

OrmPostgresDialect *  orm_postgres_dialect_new  (void);

G_END_DECLS

#endif /* ORM_POSTGRES_DIALECT_H */
