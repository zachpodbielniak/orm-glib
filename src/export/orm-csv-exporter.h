/* orm-csv-exporter.h
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

#ifndef ORM_CSV_EXPORTER_H
#define ORM_CSV_EXPORTER_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "orm-exporter.h"

G_BEGIN_DECLS

#define ORM_TYPE_CSV_EXPORTER (orm_csv_exporter_get_type ())

G_DECLARE_FINAL_TYPE (OrmCsvExporter, orm_csv_exporter, ORM, CSV_EXPORTER, OrmExporter)

/*
 * OrmCsvExporter:
 *
 * Writes a result set as RFC 4180 delimiter-separated text.
 *
 * The defaults produce plain CSV -- comma separated, quotes doubled, a
 * header line, LF endings -- and the properties cover the variants the
 * format never standardized: tab separation, CRLF for Windows tools, a
 * sentinel for NULL that an empty string cannot express, and quoting
 * everything for consumers that guess types from unquoted fields.
 */

/*
 * orm_csv_exporter_new:
 *
 * Creates a CSV exporter with RFC 4180 defaults.
 *
 * Returns: (transfer full): A new #OrmCsvExporter
 */
OrmCsvExporter * orm_csv_exporter_new (void);

G_END_DECLS

#endif /* ORM_CSV_EXPORTER_H */
