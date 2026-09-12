/* orm-engine-private.h
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

#ifndef ORM_ENGINE_PRIVATE_H
#define ORM_ENGINE_PRIVATE_H

#include <glib-object.h>
#include <gio/gio.h>
#include "orm-connection.h"
#include "orm-result.h"
#include "orm-row-stream.h"
#include "orm-worker.h"
#include "../driver/orm-driver-connection.h"
#include "../driver/orm-driver-result.h"

G_BEGIN_DECLS

/*
 * Shared between the engine-layer translation units and OrmMigrator.
 * These are not installed and carry no API stability promise; they exist
 * so OrmConnection, OrmResult, OrmTransaction and OrmMigrator can
 * cooperate without exposing the driver plumbing to callers.
 */

G_GNUC_INTERNAL
OrmResult * orm_result_new_for_driver (OrmConnection   *connection,
                                       OrmDriverResult *driver_result);

G_GNUC_INTERNAL
OrmRowStream * orm_row_stream_new_for_driver (OrmConnection   *connection,
                                              OrmDriverResult *driver_result);

/*
 * The asynchronous plumbing, used by the inspector and the row stream so
 * their work lands on the same worker thread as the connection's own.
 * Going through here rather than pushing to the worker directly is what
 * keeps the state signal and the cancellation wiring in one place.
 */

G_GNUC_INTERNAL
void orm_connection_submit_async (OrmConnection    *self,
                                  GTask            *task,
                                  OrmWorkerJobFunc  run,
                                  gpointer          data,
                                  GDestroyNotify    data_free);

G_GNUC_INTERNAL
gboolean orm_connection_task_may_run (GTask *task);

G_GNUC_INTERNAL
gboolean orm_connection_task_may_return (GTask *task);

G_GNUC_INTERNAL
void orm_connection_run_confined (OrmConnection     *self,
                                  OrmWorkerSyncFunc  func,
                                  gpointer           data);

G_GNUC_INTERNAL
void orm_connection_set_owner_context (OrmConnection *self,
                                       GMainContext  *context);

G_GNUC_INTERNAL
void orm_connection_set_in_transaction (OrmConnection *self,
                                        gboolean       in_transaction);

G_GNUC_INTERNAL
OrmDriverConnection * orm_connection_get_driver_connection (OrmConnection *self);

G_GNUC_INTERNAL
OrmDialectType orm_connection_get_dialect_type (OrmConnection *self);

G_END_DECLS

#endif /* ORM_ENGINE_PRIVATE_H */
