/* orm.h
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

#ifndef ORM_H
#define ORM_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#define ORM_INSIDE

/* Version information */
#include "orm-version.h"

/* Core types and utilities */
#include "core/orm-types.h"
#include "core/orm-enums.h"
#include "core/orm-error.h"
#include "core/orm-value.h"

/* SQL type system */
#include "types/orm-sql-type.h"
#include "types/orm-integer.h"
#include "types/orm-string.h"
#include "types/orm-text.h"
#include "types/orm-boolean.h"
#include "types/orm-float.h"
#include "types/orm-datetime.h"
#include "types/orm-blob.h"

/* Schema definitions */
#include "schema/orm-metadata.h"
#include "schema/orm-table.h"
#include "schema/orm-column.h"
#include "schema/orm-primary-key.h"
#include "schema/orm-foreign-key.h"
#include "schema/orm-index.h"

/* Dialect interface */
#include "dialect/orm-dialect.h"
#include "dialect/orm-type-compiler.h"
#include "dialect/orm-ddl-compiler.h"

/* SQLite dialect */
#ifdef ORM_ENABLE_SQLITE
#include "dialect/sqlite/orm-sqlite-dialect.h"
#include "dialect/sqlite/orm-sqlite-type-compiler.h"
#include "dialect/sqlite/orm-sqlite-ddl-compiler.h"
#endif

/* PostgreSQL dialect */
#ifdef ORM_ENABLE_POSTGRES
#include "dialect/postgres/orm-postgres-dialect.h"
#include "dialect/postgres/orm-postgres-type-compiler.h"
#include "dialect/postgres/orm-postgres-ddl-compiler.h"
#endif

/* MySQL dialect */
#ifdef ORM_ENABLE_MYSQL
#include "dialect/mysql/orm-mysql-dialect.h"
#include "dialect/mysql/orm-mysql-type-compiler.h"
#include "dialect/mysql/orm-mysql-ddl-compiler.h"
#endif

/* SQL expression language */
#include "sql/orm-expression.h"
#include "sql/orm-literal.h"
#include "sql/orm-binary-expression.h"
#include "sql/orm-column-element.h"
#include "sql/orm-table-clause.h"
#include "sql/orm-select.h"
#include "sql/orm-insert.h"
#include "sql/orm-update.h"
#include "sql/orm-delete.h"

/* Engine layer */
/* Driver layer */
#include "driver/orm-driver-result.h"
#include "driver/orm-driver-connection.h"
#include "driver/orm-driver.h"
#include "driver/orm-driver-registry.h"

#include "engine/orm-row.h"
#include "engine/orm-result.h"
#include "engine/orm-connection.h"
#include "engine/orm-transaction.h"
#include "engine/orm-engine.h"


/* ORM layer */
#include "orm/orm-serializable.h"
#include "orm/orm-property.h"
#include "orm/orm-relationship.h"
#include "orm/orm-mapper.h"
#include "orm/orm-identity-map.h"
#include "orm/orm-session.h"
#include "orm/orm-query.h"

#undef ORM_INSIDE

#endif /* ORM_H */
