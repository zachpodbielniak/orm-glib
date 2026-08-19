/* orm-session.h
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

#ifndef ORM_SESSION_H
#define ORM_SESSION_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>
#include "../engine/orm-engine.h"
#include "../engine/orm-connection.h"
#include "orm-mapper.h"
#include "orm-identity-map.h"
#include "orm-serializable.h"

G_BEGIN_DECLS

#define ORM_TYPE_SESSION (orm_session_get_type ())

G_DECLARE_FINAL_TYPE (OrmSession, orm_session, ORM, SESSION, GObject)

/* Forward declaration */
typedef struct _OrmQuery OrmQuery;

/**
 * OrmObjectState:
 * @ORM_OBJECT_TRANSIENT: Object not associated with session
 * @ORM_OBJECT_PENDING: Object is new, pending INSERT
 * @ORM_OBJECT_PERSISTENT: Object is attached and persistent
 * @ORM_OBJECT_DIRTY: Object has uncommitted changes
 * @ORM_OBJECT_DELETED: Object is marked for deletion
 * @ORM_OBJECT_DETACHED: Object was attached but is now detached
 *
 * States an object can be in relative to a session.
 */
typedef enum {
    ORM_OBJECT_TRANSIENT,
    ORM_OBJECT_PENDING,
    ORM_OBJECT_PERSISTENT,
    ORM_OBJECT_DIRTY,
    ORM_OBJECT_DELETED,
    ORM_OBJECT_DETACHED
} OrmObjectState;

GType orm_object_state_get_type (void) G_GNUC_CONST;

#define ORM_TYPE_OBJECT_STATE (orm_object_state_get_type ())

/**
 * OrmSession:
 *
 * Implements the Unit of Work pattern.
 * Tracks new, dirty, and deleted objects, manages the identity map,
 * and coordinates persistence operations through the connection.
 */

/*
 * orm_session_new:
 * @engine: The database engine
 *
 * Creates a new session for the given engine.
 * Opens a connection automatically.
 *
 * Returns: (transfer full): A new #OrmSession
 */
OrmSession *        orm_session_new                     (OrmEngine *engine);

/*
 * orm_session_new_with_connection:
 * @connection: An existing connection
 *
 * Creates a new session using an existing connection.
 *
 * Returns: (transfer full): A new #OrmSession
 */
OrmSession *        orm_session_new_with_connection     (OrmConnection *connection);

/*
 * orm_session_get_connection:
 * @self: An #OrmSession
 *
 * Gets the underlying connection.
 *
 * Returns: (transfer none): The connection
 */
OrmConnection *     orm_session_get_connection          (OrmSession *self);

/*
 * orm_session_register_mapper:
 * @self: An #OrmSession
 * @mapper: The mapper to register
 *
 * Registers a mapper with the session.
 * Required before working with objects of that type.
 */
void                orm_session_register_mapper         (OrmSession *self,
                                                         OrmMapper  *mapper);

/*
 * orm_session_get_mapper:
 * @self: An #OrmSession
 * @gtype: The GType to look up
 *
 * Gets the mapper for a GType.
 *
 * Returns: (transfer none) (nullable): The mapper
 */
OrmMapper *         orm_session_get_mapper              (OrmSession *self,
                                                         GType       gtype);

/*
 * orm_session_add:
 * @self: An #OrmSession
 * @object: The object to add
 *
 * Adds a new object to the session (pending INSERT).
 * The object must implement OrmSerializable.
 */
void                orm_session_add                     (OrmSession *self,
                                                         GObject    *object);

/*
 * orm_session_add_all:
 * @self: An #OrmSession
 * @objects: (element-type GObject): List of objects to add
 *
 * Adds multiple objects to the session.
 */
void                orm_session_add_all                 (OrmSession *self,
                                                         GList      *objects);

/*
 * orm_session_delete:
 * @self: An #OrmSession
 * @object: The object to delete
 *
 * Marks an object for deletion (pending DELETE).
 */
void                orm_session_delete                  (OrmSession *self,
                                                         GObject    *object);

/*
 * orm_session_expunge:
 * @self: An #OrmSession
 * @object: The object to remove
 *
 * Removes an object from the session without deleting from database.
 * The object becomes detached.
 */
void                orm_session_expunge                 (OrmSession *self,
                                                         GObject    *object);

/*
 * orm_session_get_object_state:
 * @self: An #OrmSession
 * @object: The object to check
 *
 * Gets the state of an object in this session.
 *
 * Returns: The object state
 */
OrmObjectState      orm_session_get_object_state        (OrmSession *self,
                                                         GObject    *object);

/*
 * orm_session_is_dirty:
 * @self: An #OrmSession
 *
 * Checks if the session has uncommitted changes.
 *
 * Returns: %TRUE if there are uncommitted changes
 */
gboolean            orm_session_is_dirty                (OrmSession *self);

/*
 * orm_session_flush:
 * @self: An #OrmSession
 * @error: Return location for error
 *
 * Flushes pending changes to the database without committing.
 * Executes INSERT, UPDATE, and DELETE statements.
 *
 * Returns: %TRUE on success
 */
gboolean            orm_session_flush                   (OrmSession  *self,
                                                         GError     **error);

/*
 * orm_session_commit:
 * @self: An #OrmSession
 * @error: Return location for error
 *
 * Commits all pending changes to the database.
 * Calls flush() and then commits the transaction.
 *
 * Returns: %TRUE on success
 */
gboolean            orm_session_commit                  (OrmSession  *self,
                                                         GError     **error);

/*
 * orm_session_rollback:
 * @self: An #OrmSession
 *
 * Rolls back all pending changes.
 * Clears the session state and expires all objects.
 */
void                orm_session_rollback                (OrmSession *self);

/*
 * orm_session_refresh:
 * @self: An #OrmSession
 * @object: The object to refresh
 * @error: Return location for error
 *
 * Refreshes an object from the database.
 *
 * Returns: %TRUE on success
 */
gboolean            orm_session_refresh                 (OrmSession  *self,
                                                         GObject     *object,
                                                         GError     **error);

/*
 * orm_session_get:
 * @self: An #OrmSession
 * @gtype: The GType of the object
 * @primary_key: The primary key value
 * @error: Return location for error
 *
 * Gets an object by primary key.
 * Returns from identity map if available, otherwise loads from database.
 *
 * Returns: (transfer full) (nullable): The object, or %NULL if not found
 */
GObject *           orm_session_get                     (OrmSession  *self,
                                                         GType        gtype,
                                                         OrmValue    *primary_key,
                                                         GError     **error);

/*
 * orm_session_query:
 * @self: An #OrmSession
 * @gtype: The GType to query
 *
 * Creates a query for the given type.
 *
 * Returns: (transfer full): A new #OrmQuery
 */
OrmQuery *          orm_session_query                   (OrmSession *self,
                                                         GType       gtype);

/*
 * orm_session_execute:
 * @self: An #OrmSession
 * @sql: Raw SQL to execute
 * @error: Return location for error
 *
 * Executes raw SQL through the session.
 *
 * Returns: %TRUE on success
 */
gboolean            orm_session_execute                 (OrmSession  *self,
                                                         const gchar *sql,
                                                         GError     **error);

/*
 * orm_session_close:
 * @self: An #OrmSession
 *
 * Closes the session and releases resources.
 * Any uncommitted changes are rolled back.
 */
void                orm_session_close                   (OrmSession *self);

/*
 * orm_session_is_closed:
 * @self: An #OrmSession
 *
 * Checks if the session is closed.
 *
 * Returns: %TRUE if closed
 */
gboolean            orm_session_is_closed               (OrmSession *self);

G_END_DECLS

#endif /* ORM_SESSION_H */
