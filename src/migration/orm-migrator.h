/* orm-migrator.h
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

#ifndef ORM_MIGRATOR_H
#define ORM_MIGRATOR_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include "../engine/orm-connection.h"
#include "../dialect/orm-dialect.h"

G_BEGIN_DECLS

/**
 * OrmMigration:
 *
 * An opaque boxed value describing one schema version.
 *
 * Versions are strictly increasing positive `gint64`s. The SHA-256 of
 * the up SQL (or of @up_text for a callback migration) is recorded in
 * `schema_migrations` and rechecked on every later run. Never edit an
 * applied migration; append a new version instead.
 */
typedef struct _OrmMigration OrmMigration;

/**
 * OrmMigrationFunc:
 * @connection: The exclusively held connection
 * @dialect: The connection's dialect
 * @error: (nullable): Return location for a #GError
 *
 * Execute statements without managing transactions or migration
 * bookkeeping. Return %FALSE and set @error on failure. Do not
 * COMMIT, ROLLBACK, or touch the migration lock tables.
 *
 * Returns: %TRUE on success
 */
typedef gboolean (*OrmMigrationFunc) (OrmConnection  *connection,
                                      OrmDialect     *dialect,
                                      GError        **error);

#define ORM_TYPE_MIGRATION (orm_migration_get_type ())
GType           orm_migration_get_type     (void) G_GNUC_CONST;

/**
 * orm_migration_new:
 * @version: Positive version, ordered strictly increasingly in the list
 * @name: Nonempty name stored in history
 * @up: Nonempty SQL statement; hashed with SHA-256
 * @down: (nullable): Reverse SQL, or %NULL if irreversible
 *
 * Creates a single-statement SQL migration.
 *
 * Returns: (transfer full): A boxed #OrmMigration
 */
OrmMigration *  orm_migration_new          (gint64       version,
                                            const gchar *name,
                                            const gchar *up,
                                            const gchar *down);

/**
 * orm_migration_new_callback:
 * @version: Positive version
 * @name: Nonempty name stored in history
 * @up_text: Stable, nonempty source/description, hashed instead of a
 *   function address; change this whenever the implementation changes
 * @up: (scope forever): Forward operation
 * @down: (scope forever) (nullable): Reverse operation, or %NULL if
 *   irreversible
 *
 * Creates a callback migration for multi-statement or portable DDL.
 *
 * Returns: (transfer full): A boxed #OrmMigration
 */
OrmMigration *  orm_migration_new_callback (gint64            version,
                                            const gchar      *name,
                                            const gchar      *up_text,
                                            OrmMigrationFunc  up,
                                            OrmMigrationFunc  down);

/**
 * orm_migration_copy:
 * @self: A migration
 *
 * Returns: (transfer full): An independent copy
 */
OrmMigration *  orm_migration_copy         (const OrmMigration *self);

/**
 * orm_migration_free:
 * @self: (nullable): Migration to release
 */
void            orm_migration_free         (OrmMigration *self);

/**
 * orm_migration_get_version:
 * @self: A migration
 *
 * Returns: The version
 */
gint64          orm_migration_get_version  (const OrmMigration *self);

/**
 * orm_migration_get_name:
 * @self: A migration
 *
 * Returns: (transfer none): The name
 */
const gchar *   orm_migration_get_name     (const OrmMigration *self);

/**
 * orm_migration_get_checksum:
 * @self: A migration
 *
 * Returns: (transfer none): Lower-case SHA-256 hex of the up SQL or
 *   callback @up_text
 */
const gchar *   orm_migration_get_checksum (const OrmMigration *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (OrmMigration, orm_migration_free)

#define ORM_TYPE_MIGRATOR (orm_migrator_get_type ())
G_DECLARE_FINAL_TYPE (OrmMigrator, orm_migrator, ORM, MIGRATOR, GObject)

/**
 * orm_migrator_new:
 * @connection: Idle connection, used exclusively without an outer
 *   transaction for the life of each up/down/status call
 * @migrations: (array length=n_migrations): Complete ordered set
 * @n_migrations: Number of migrations
 * @error: (nullable): Return location for a #GError
 *
 * Copies @migrations and retains @connection. Never executes SQL.
 *
 * Returns: (transfer full) (nullable): A migrator, or %NULL on error
 */
OrmMigrator *   orm_migrator_new     (OrmConnection          *connection,
                                      OrmMigration * const   *migrations,
                                      guint                   n_migrations,
                                      GError                **error);

/**
 * orm_migrator_status:
 * @self: A migrator
 * @applied: (out) (transfer full) (element-type gint64): Applied versions
 * @pending: (out) (transfer full) (element-type gint64): Pending versions
 * @error: (nullable): Return location for a #GError
 *
 * Validates every applied checksum and name, and creates the
 * bookkeeping table if needed, without running a migration.
 *
 * Returns: %TRUE on success; arrays are ascending and %NULL on failure
 */
gboolean        orm_migrator_status  (OrmMigrator  *self,
                                      GArray      **applied,
                                      GArray      **pending,
                                      GError      **error);

/**
 * orm_migrator_up:
 * @self: A migrator
 * @target: Version to reach, or zero for latest
 * @error: (nullable): Return location for a #GError
 *
 * Applies pending migrations up to @target. Each successful step stays
 * committed if a later step fails.
 *
 * Returns: %TRUE on success
 */
gboolean        orm_migrator_up      (OrmMigrator  *self,
                                      gint64        target,
                                      GError      **error);

/**
 * orm_migrator_down:
 * @self: A migrator
 * @target: Version to keep, or zero to remove every applied migration
 * @error: (nullable): Return location for a #GError
 *
 * Rolls back applied migrations down to @target. A missing down
 * operation is an error when that step is reached.
 *
 * Returns: %TRUE on success
 */
gboolean        orm_migrator_down    (OrmMigrator  *self,
                                      gint64        target,
                                      GError      **error);

G_END_DECLS

#endif /* ORM_MIGRATOR_H */
