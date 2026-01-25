/* orm-engine.h
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

#ifndef ORM_ENGINE_H
#define ORM_ENGINE_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-enums.h"
#include "../dialect/orm-dialect.h"

G_BEGIN_DECLS

#define ORM_TYPE_ENGINE (orm_engine_get_type ())

G_DECLARE_FINAL_TYPE (OrmEngine, orm_engine, ORM, ENGINE, GObject)

/* Forward declarations */
typedef struct _OrmConnection OrmConnection;

/*
 * OrmEngine:
 *
 * The database engine factory. Creates connections to databases
 * based on connection URLs.
 *
 * Supported URL formats:
 *   sqlite:///path/to/database.db
 *   sqlite:///:memory:
 *   postgresql://user:pass@host:port/database
 *   mysql://user:pass@host:port/database
 *
 * The engine holds the dialect and connection configuration,
 * and can create multiple connections.
 */

/*
 * orm_engine_new:
 * @url: Database connection URL
 * @error: Return location for error
 *
 * Creates a new database engine from a connection URL.
 *
 * Returns: (transfer full) (nullable): A new #OrmEngine, or %NULL on error
 */
OrmEngine * orm_engine_new (const gchar  *url,
                            GError      **error);

/*
 * orm_engine_new_sqlite:
 * @path: Path to SQLite database file, or ":memory:"
 * @error: Return location for error
 *
 * Creates a new SQLite database engine.
 *
 * Returns: (transfer full) (nullable): A new #OrmEngine, or %NULL on error
 */
OrmEngine * orm_engine_new_sqlite (const gchar  *path,
                                   GError      **error);

/*
 * orm_engine_get_dialect:
 * @self: An #OrmEngine
 *
 * Gets the dialect for this engine.
 *
 * Returns: (transfer none): The dialect
 */
OrmDialect * orm_engine_get_dialect (OrmEngine *self);

/*
 * orm_engine_get_dialect_type:
 * @self: An #OrmEngine
 *
 * Gets the dialect type for this engine.
 *
 * Returns: The dialect type
 */
OrmDialectType orm_engine_get_dialect_type (OrmEngine *self);

/*
 * orm_engine_get_url:
 * @self: An #OrmEngine
 *
 * Gets the connection URL.
 *
 * Returns: (transfer none): The URL
 */
const gchar * orm_engine_get_url (OrmEngine *self);

/*
 * orm_engine_connect:
 * @self: An #OrmEngine
 * @error: Return location for error
 *
 * Creates a new connection to the database.
 *
 * Returns: (transfer full) (nullable): A new #OrmConnection, or %NULL on error
 */
OrmConnection * orm_engine_connect (OrmEngine  *self,
                                    GError    **error);

/*
 * orm_engine_execute:
 * @self: An #OrmEngine
 * @sql: SQL statement to execute
 * @error: Return location for error
 *
 * Executes a SQL statement using a temporary connection.
 * For queries that don't return results (CREATE, INSERT, etc.)
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean orm_engine_execute (OrmEngine    *self,
                             const gchar  *sql,
                             GError      **error);

/*
 * orm_engine_create_all:
 * @self: An #OrmEngine
 * @metadata: Schema metadata containing tables to create
 * @error: Return location for error
 *
 * Creates all tables defined in the metadata.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean orm_engine_create_all (OrmEngine    *self,
                                gpointer      metadata,
                                GError      **error);

/*
 * orm_engine_drop_all:
 * @self: An #OrmEngine
 * @metadata: Schema metadata containing tables to drop
 * @error: Return location for error
 *
 * Drops all tables defined in the metadata.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean orm_engine_drop_all (OrmEngine    *self,
                              gpointer      metadata,
                              GError      **error);

G_END_DECLS

#endif /* ORM_ENGINE_H */
