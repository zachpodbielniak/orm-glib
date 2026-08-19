/* orm-query.h
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

#ifndef ORM_QUERY_H
#define ORM_QUERY_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-value.h"
#include "../core/orm-enums.h"

G_BEGIN_DECLS

#define ORM_TYPE_QUERY (orm_query_get_type ())

G_DECLARE_FINAL_TYPE (OrmQuery, orm_query, ORM, QUERY, GObject)

/* Forward declarations */
typedef struct _OrmSession OrmSession;

/* OrmCompareOp is defined in orm-enums.h */

/**
 * OrmSortOrder:
 * @ORM_SORT_ASC: Ascending order
 * @ORM_SORT_DESC: Descending order
 *
 * Sort order for query results.
 */
typedef enum {
    ORM_SORT_ASC,
    ORM_SORT_DESC
} OrmSortOrder;

GType orm_sort_order_get_type (void) G_GNUC_CONST;

#define ORM_TYPE_SORT_ORDER (orm_sort_order_get_type ())

/**
 * OrmQuery:
 *
 * High-level query builder for ORM objects.
 * Provides fluent interface for filtering, ordering, and limiting results.
 * Returns OrmSerializable objects through the session's identity map.
 */

/*
 * orm_query_new:
 * @session: The session to query through
 * @gtype: The GType to query
 *
 * Creates a new query for the given type.
 *
 * Returns: (transfer full): A new #OrmQuery
 */
OrmQuery *      orm_query_new                   (OrmSession *session,
                                                 GType       gtype);

/*
 * orm_query_filter:
 * @self: An #OrmQuery
 * @property_name: The property to filter on
 * @op: The comparison operator
 * @value: The value to compare against
 *
 * Adds a filter condition to the query.
 * Returns self for method chaining.
 *
 * Returns: (transfer none): The query
 */
OrmQuery *      orm_query_filter                (OrmQuery    *self,
                                                 const gchar *property_name,
                                                 OrmCompareOp  op,
                                                 OrmValue    *value);

/*
 * orm_query_filter_by:
 * @self: An #OrmQuery
 * @property_name: The property to filter on
 * @value: The value to compare (equality)
 *
 * Adds an equality filter condition.
 * Shorthand for orm_query_filter(query, name, ORM_OP_EQ, value).
 *
 * Returns: (transfer none): The query
 */
OrmQuery *      orm_query_filter_by             (OrmQuery    *self,
                                                 const gchar *property_name,
                                                 OrmValue    *value);

/*
 * orm_query_order_by:
 * @self: An #OrmQuery
 * @property_name: The property to order by
 * @order: The sort order
 *
 * Adds an ordering to the query.
 * Returns self for method chaining.
 *
 * Returns: (transfer none): The query
 */
OrmQuery *      orm_query_order_by              (OrmQuery     *self,
                                                 const gchar  *property_name,
                                                 OrmSortOrder  order);

/*
 * orm_query_limit:
 * @self: An #OrmQuery
 * @limit: Maximum number of results
 *
 * Sets the maximum number of results to return.
 *
 * Returns: (transfer none): The query
 */
OrmQuery *      orm_query_limit                 (OrmQuery *self,
                                                 gint      limit);

/*
 * orm_query_offset:
 * @self: An #OrmQuery
 * @offset: Number of results to skip
 *
 * Sets the number of results to skip.
 *
 * Returns: (transfer none): The query
 */
OrmQuery *      orm_query_offset                (OrmQuery *self,
                                                 gint      offset);

/*
 * orm_query_all:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Executes the query and returns all matching objects.
 *
 * Returns: (transfer full) (element-type GObject) (nullable): List of objects
 */
GList *         orm_query_all                   (OrmQuery  *self,
                                                 GError   **error);

/*
 * orm_query_first:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Executes the query and returns the first matching object.
 *
 * Returns: (transfer full) (nullable): The first object, or %NULL
 */
GObject *       orm_query_first                 (OrmQuery  *self,
                                                 GError   **error);

/*
 * orm_query_one:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Executes the query and returns exactly one object.
 * Sets an error if no object or multiple objects are found.
 *
 * Returns: (transfer full) (nullable): The single object
 */
GObject *       orm_query_one                   (OrmQuery  *self,
                                                 GError   **error);

/*
 * orm_query_one_or_none:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Executes the query and returns one object or none.
 * Sets an error if multiple objects are found.
 *
 * Returns: (transfer full) (nullable): The object or %NULL
 */
GObject *       orm_query_one_or_none           (OrmQuery  *self,
                                                 GError   **error);

/*
 * orm_query_count:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Returns the count of matching objects.
 *
 * Returns: The count, or -1 on error
 */
gint64          orm_query_count                 (OrmQuery  *self,
                                                 GError   **error);

/*
 * orm_query_exists:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Checks if any matching objects exist.
 *
 * Returns: %TRUE if at least one object matches
 */
gboolean        orm_query_exists                (OrmQuery  *self,
                                                 GError   **error);

/*
 * orm_query_delete:
 * @self: An #OrmQuery
 * @error: Return location for error
 *
 * Deletes all matching objects.
 *
 * Returns: Number of deleted objects, or -1 on error
 */
gint64          orm_query_delete                (OrmQuery  *self,
                                                 GError   **error);

/*
 * orm_query_get_sql:
 * @self: An #OrmQuery
 *
 * Gets the generated SQL for debugging.
 *
 * Returns: (transfer full): The SQL string
 */
gchar *         orm_query_get_sql               (OrmQuery *self);

G_END_DECLS

#endif /* ORM_QUERY_H */
