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
#include "orm-connection.h"
#include "orm-result.h"
#include "../driver/orm-driver-connection.h"
#include "../driver/orm-driver-result.h"

G_BEGIN_DECLS

/*
 * Shared between the engine-layer translation units and nothing else.
 * These are not installed and carry no API stability promise; they exist
 * so OrmConnection, OrmResult and OrmTransaction can cooperate without
 * exposing the driver plumbing to callers.
 */

G_GNUC_INTERNAL
OrmResult * orm_result_new_for_driver (OrmConnection   *connection,
                                       OrmDriverResult *driver_result);

G_GNUC_INTERNAL
void orm_connection_set_in_transaction (OrmConnection *self,
                                        gboolean       in_transaction);

G_GNUC_INTERNAL
OrmDriverConnection * orm_connection_get_driver_connection (OrmConnection *self);

G_GNUC_INTERNAL
OrmDialectType orm_connection_get_dialect_type (OrmConnection *self);

G_END_DECLS

#endif /* ORM_ENGINE_PRIVATE_H */
