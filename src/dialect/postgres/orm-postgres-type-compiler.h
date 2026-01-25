/* orm-postgres-type-compiler.h
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

#ifndef ORM_POSTGRES_TYPE_COMPILER_H
#define ORM_POSTGRES_TYPE_COMPILER_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../orm-type-compiler.h"

G_BEGIN_DECLS

#define ORM_TYPE_POSTGRES_TYPE_COMPILER (orm_postgres_type_compiler_get_type ())

G_DECLARE_FINAL_TYPE (OrmPostgresTypeCompiler, orm_postgres_type_compiler, ORM, POSTGRES_TYPE_COMPILER, GObject)

/**
 * OrmPostgresTypeCompiler:
 *
 * PostgreSQL type compiler implementation.
 *
 * PostgreSQL type mappings:
 * - INTEGER: 32-bit signed integer
 * - BIGINT: 64-bit signed integer
 * - SMALLINT: 16-bit signed integer
 * - SERIAL: Auto-incrementing 32-bit integer
 * - BIGSERIAL: Auto-incrementing 64-bit integer
 * - BOOLEAN: True/false values
 * - VARCHAR(n): Variable-length string with limit
 * - TEXT: Unlimited variable-length string
 * - REAL: 32-bit floating point
 * - DOUBLE PRECISION: 64-bit floating point
 * - TIMESTAMP: Date and time without timezone
 * - TIMESTAMPTZ: Date and time with timezone
 * - BYTEA: Binary data
 */

OrmPostgresTypeCompiler * orm_postgres_type_compiler_new (void);

G_END_DECLS

#endif /* ORM_POSTGRES_TYPE_COMPILER_H */
