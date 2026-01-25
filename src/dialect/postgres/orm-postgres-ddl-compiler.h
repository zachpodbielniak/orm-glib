/* orm-postgres-ddl-compiler.h
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

#ifndef ORM_POSTGRES_DDL_COMPILER_H
#define ORM_POSTGRES_DDL_COMPILER_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../orm-ddl-compiler.h"

G_BEGIN_DECLS

#define ORM_TYPE_POSTGRES_DDL_COMPILER (orm_postgres_ddl_compiler_get_type ())

G_DECLARE_FINAL_TYPE (OrmPostgresDdlCompiler, orm_postgres_ddl_compiler, ORM, POSTGRES_DDL_COMPILER, GObject)

/**
 * OrmPostgresDdlCompiler:
 *
 * PostgreSQL DDL compiler implementation.
 *
 * PostgreSQL DDL characteristics:
 * - CREATE TABLE with IF NOT EXISTS
 * - DROP TABLE with IF EXISTS and CASCADE
 * - ALTER TABLE for schema modifications
 * - CREATE INDEX with CONCURRENTLY option
 * - CREATE SCHEMA for namespaces
 * - SERIAL/BIGSERIAL for auto-increment columns
 * - Full foreign key constraint support
 * - CHECK constraints
 * - UNIQUE constraints (as table-level or column-level)
 */

OrmPostgresDdlCompiler * orm_postgres_ddl_compiler_new (void);

G_END_DECLS

#endif /* ORM_POSTGRES_DDL_COMPILER_H */
