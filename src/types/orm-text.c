/* orm-text.c
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

#include "orm-text.h"
#include "../core/orm-enums.h"

/*
 * OrmText - Unlimited length text type.
 * Maps to: SQLite TEXT, PostgreSQL TEXT, MySQL TEXT
 */
struct _OrmText
{
    OrmSqlType parent_instance;
};

G_DEFINE_TYPE (OrmText, orm_text, ORM_TYPE_SQL_TYPE)

/*
 * Returns the SQL type name for the target dialect.
 * All dialects use TEXT for this type.
 */
static const gchar *
orm_text_get_name (OrmSqlType     *self,
                   OrmDialectType  dialect_type)
{
    (void) self;
    (void) dialect_type;

    /* TEXT is universal across all supported databases */
    return "TEXT";
}

static void
orm_text_class_init (OrmTextClass *klass)
{
    OrmSqlTypeClass *type_class = ORM_SQL_TYPE_CLASS (klass);

    type_class->get_name = orm_text_get_name;
}

static void
orm_text_init (OrmText *self)
{
    (void) self;
}

/**
 * orm_text_new:
 *
 * Creates a new text SQL type for large strings.
 *
 * Returns: (transfer full): A new #OrmText
 */
OrmText *
orm_text_new (void)
{
    return g_object_new (ORM_TYPE_TEXT, NULL);
}
