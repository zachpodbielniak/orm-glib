/* orm-identity-map.h
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

#ifndef ORM_IDENTITY_MAP_H
#define ORM_IDENTITY_MAP_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../core/orm-value.h"

G_BEGIN_DECLS

#define ORM_TYPE_IDENTITY_MAP (orm_identity_map_get_type ())

G_DECLARE_FINAL_TYPE (OrmIdentityMap, orm_identity_map, ORM, IDENTITY_MAP, GObject)

/**
 * OrmIdentityMap:
 *
 * Tracks object identity within a session.
 * Ensures that only one instance of each database row exists in memory,
 * keyed by (GType, primary_key). This is the Identity Map pattern.
 */

/**
 * orm_identity_map_new:
 *
 * Creates a new identity map.
 *
 * Returns: (transfer full): A new #OrmIdentityMap
 */
OrmIdentityMap *    orm_identity_map_new                (void);

/**
 * orm_identity_map_add:
 * @self: An #OrmIdentityMap
 * @gtype: The GType of the object
 * @primary_key: The primary key value
 * @object: The object instance
 *
 * Adds an object to the identity map.
 * The map takes a reference to the object.
 */
void                orm_identity_map_add                (OrmIdentityMap *self,
                                                         GType           gtype,
                                                         OrmValue       *primary_key,
                                                         GObject        *object);

/**
 * orm_identity_map_get:
 * @self: An #OrmIdentityMap
 * @gtype: The GType of the object
 * @primary_key: The primary key value
 *
 * Gets an object from the identity map.
 *
 * Returns: (transfer none) (nullable): The object, or %NULL if not found
 */
GObject *           orm_identity_map_get                (OrmIdentityMap *self,
                                                         GType           gtype,
                                                         OrmValue       *primary_key);

/**
 * orm_identity_map_contains:
 * @self: An #OrmIdentityMap
 * @gtype: The GType of the object
 * @primary_key: The primary key value
 *
 * Checks if an object is in the identity map.
 *
 * Returns: %TRUE if the object exists
 */
gboolean            orm_identity_map_contains           (OrmIdentityMap *self,
                                                         GType           gtype,
                                                         OrmValue       *primary_key);

/**
 * orm_identity_map_remove:
 * @self: An #OrmIdentityMap
 * @gtype: The GType of the object
 * @primary_key: The primary key value
 *
 * Removes an object from the identity map.
 *
 * Returns: %TRUE if the object was removed
 */
gboolean            orm_identity_map_remove             (OrmIdentityMap *self,
                                                         GType           gtype,
                                                         OrmValue       *primary_key);

/**
 * orm_identity_map_remove_object:
 * @self: An #OrmIdentityMap
 * @object: The object to remove
 *
 * Removes an object from the identity map by reference.
 *
 * Returns: %TRUE if the object was removed
 */
gboolean            orm_identity_map_remove_object      (OrmIdentityMap *self,
                                                         GObject        *object);

/**
 * orm_identity_map_clear:
 * @self: An #OrmIdentityMap
 *
 * Removes all objects from the identity map.
 */
void                orm_identity_map_clear              (OrmIdentityMap *self);

/**
 * orm_identity_map_get_all:
 * @self: An #OrmIdentityMap
 * @gtype: The GType to filter by (or G_TYPE_NONE for all)
 *
 * Gets all objects in the identity map of the given type.
 *
 * Returns: (transfer container) (element-type GObject): List of objects
 */
GList *             orm_identity_map_get_all            (OrmIdentityMap *self,
                                                         GType           gtype);

/**
 * orm_identity_map_size:
 * @self: An #OrmIdentityMap
 *
 * Gets the number of objects in the identity map.
 *
 * Returns: The number of objects
 */
guint               orm_identity_map_size               (OrmIdentityMap *self);

G_END_DECLS

#endif /* ORM_IDENTITY_MAP_H */
