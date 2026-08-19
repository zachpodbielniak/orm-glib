/* orm-json-exporter.h
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

#ifndef ORM_JSON_EXPORTER_H
#define ORM_JSON_EXPORTER_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-enums.h"
#include "orm-exporter.h"

G_BEGIN_DECLS

#define ORM_TYPE_JSON_EXPORTER (orm_json_exporter_get_type ())

G_DECLARE_FINAL_TYPE (OrmJsonExporter, orm_json_exporter, ORM, JSON_EXPORTER, OrmExporter)

/*
 * OrmJsonExporter:
 *
 * Writes a result set as JSON, either as one object per row keyed by
 * column name or as one array per row -- see #OrmJsonLayout.
 *
 * SQL carries three things JSON has no spelling for, and each is
 * translated rather than emitted raw: a blob becomes Base64, a datetime
 * becomes an ISO 8601 string, and a NaN or infinity becomes null, because
 * writing those tokens produces a document no parser will accept.
 */

/*
 * orm_json_exporter_new:
 *
 * Creates a JSON exporter emitting a compact array of objects.
 *
 * Returns: (transfer full): A new #OrmJsonExporter
 */
OrmJsonExporter * orm_json_exporter_new (void);

G_END_DECLS

#endif /* ORM_JSON_EXPORTER_H */
