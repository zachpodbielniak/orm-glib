/* orm-enums.h
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

#ifndef ORM_ENUMS_H
#define ORM_ENUMS_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * OrmDialectType:
 * @ORM_DIALECT_SQLITE: SQLite database
 * @ORM_DIALECT_POSTGRES: PostgreSQL database
 * @ORM_DIALECT_MYSQL: MySQL/MariaDB database
 *
 * Supported database dialects.
 */
typedef enum {
    ORM_DIALECT_SQLITE,
    ORM_DIALECT_POSTGRES,
    ORM_DIALECT_MYSQL
} OrmDialectType;

/**
 * OrmValueType:
 * @ORM_VALUE_NULL: NULL value
 * @ORM_VALUE_INTEGER: Integer value (gint64)
 * @ORM_VALUE_FLOAT: Floating point value (gdouble)
 * @ORM_VALUE_STRING: String value (gchar *)
 * @ORM_VALUE_BLOB: Binary data (GBytes *)
 * @ORM_VALUE_BOOLEAN: Boolean value (gboolean)
 * @ORM_VALUE_DATETIME: DateTime value (GDateTime *)
 *
 * Types of values that can be stored in an OrmValue.
 */
typedef enum {
    ORM_VALUE_NULL,
    ORM_VALUE_INTEGER,
    ORM_VALUE_FLOAT,
    ORM_VALUE_STRING,
    ORM_VALUE_BLOB,
    ORM_VALUE_BOOLEAN,
    ORM_VALUE_DATETIME
} OrmValueType;

/**
 * OrmCompareOp:
 * @ORM_OP_EQ: Equal (=)
 * @ORM_OP_NE: Not equal (!=)
 * @ORM_OP_LT: Less than (<)
 * @ORM_OP_LE: Less than or equal (<=)
 * @ORM_OP_GT: Greater than (>)
 * @ORM_OP_GE: Greater than or equal (>=)
 * @ORM_OP_LIKE: LIKE pattern match
 * @ORM_OP_ILIKE: Case-insensitive LIKE (PostgreSQL)
 * @ORM_OP_IN: IN list
 * @ORM_OP_NOT_IN: NOT IN list
 * @ORM_OP_IS_NULL: IS NULL
 * @ORM_OP_IS_NOT_NULL: IS NOT NULL
 * @ORM_OP_BETWEEN: BETWEEN range
 *
 * Comparison operators for filter expressions.
 */
typedef enum {
    ORM_OP_EQ,
    ORM_OP_NE,
    ORM_OP_LT,
    ORM_OP_LE,
    ORM_OP_GT,
    ORM_OP_GE,
    ORM_OP_LIKE,
    ORM_OP_ILIKE,
    ORM_OP_IN,
    ORM_OP_NOT_IN,
    ORM_OP_IS_NULL,
    ORM_OP_IS_NOT_NULL,
    ORM_OP_BETWEEN
} OrmCompareOp;

/**
 * OrmLogicalOp:
 * @ORM_LOGICAL_AND: Logical AND
 * @ORM_LOGICAL_OR: Logical OR
 * @ORM_LOGICAL_NOT: Logical NOT
 *
 * Logical operators for combining expressions.
 */
typedef enum {
    ORM_LOGICAL_AND,
    ORM_LOGICAL_OR,
    ORM_LOGICAL_NOT
} OrmLogicalOp;

/**
 * OrmOrderDirection:
 * @ORM_ORDER_ASC: Ascending order
 * @ORM_ORDER_DESC: Descending order
 *
 * Sort order direction.
 */
typedef enum {
    ORM_ORDER_ASC,
    ORM_ORDER_DESC
} OrmOrderDirection;

/**
 * OrmJoinType:
 * @ORM_JOIN_INNER: INNER JOIN
 * @ORM_JOIN_LEFT: LEFT OUTER JOIN
 * @ORM_JOIN_RIGHT: RIGHT OUTER JOIN
 * @ORM_JOIN_FULL: FULL OUTER JOIN
 * @ORM_JOIN_CROSS: CROSS JOIN
 *
 * Types of SQL joins.
 */
typedef enum {
    ORM_JOIN_INNER,
    ORM_JOIN_LEFT,
    ORM_JOIN_RIGHT,
    ORM_JOIN_FULL,
    ORM_JOIN_CROSS
} OrmJoinType;

/**
 * OrmConstraintType:
 * @ORM_CONSTRAINT_PRIMARY_KEY: Primary key constraint
 * @ORM_CONSTRAINT_FOREIGN_KEY: Foreign key constraint
 * @ORM_CONSTRAINT_UNIQUE: Unique constraint
 * @ORM_CONSTRAINT_CHECK: Check constraint
 * @ORM_CONSTRAINT_NOT_NULL: Not null constraint
 *
 * Types of table constraints.
 */
typedef enum {
    ORM_CONSTRAINT_PRIMARY_KEY,
    ORM_CONSTRAINT_FOREIGN_KEY,
    ORM_CONSTRAINT_UNIQUE,
    ORM_CONSTRAINT_CHECK,
    ORM_CONSTRAINT_NOT_NULL
} OrmConstraintType;

/**
 * OrmForeignKeyAction:
 * @ORM_FK_NO_ACTION: No action on delete/update
 * @ORM_FK_RESTRICT: Restrict delete/update
 * @ORM_FK_CASCADE: Cascade delete/update
 * @ORM_FK_SET_NULL: Set to NULL on delete/update
 * @ORM_FK_SET_DEFAULT: Set to default on delete/update
 *
 * Foreign key referential actions.
 */
typedef enum {
    ORM_FK_NO_ACTION,
    ORM_FK_RESTRICT,
    ORM_FK_CASCADE,
    ORM_FK_SET_NULL,
    ORM_FK_SET_DEFAULT
} OrmForeignKeyAction;

/**
 * OrmRelationshipType:
 * @ORM_RELATIONSHIP_ONE_TO_ONE: One-to-one relationship
 * @ORM_RELATIONSHIP_ONE_TO_MANY: One-to-many relationship
 * @ORM_RELATIONSHIP_MANY_TO_ONE: Many-to-one relationship
 * @ORM_RELATIONSHIP_MANY_TO_MANY: Many-to-many relationship
 *
 * Types of ORM relationships between objects.
 */
typedef enum {
    ORM_RELATIONSHIP_ONE_TO_ONE,
    ORM_RELATIONSHIP_ONE_TO_MANY,
    ORM_RELATIONSHIP_MANY_TO_ONE,
    ORM_RELATIONSHIP_MANY_TO_MANY
} OrmRelationshipType;

/**
 * OrmSessionState:
 * @ORM_SESSION_NEW: Object is new and not persisted
 * @ORM_SESSION_PERSISTENT: Object is persisted and tracked
 * @ORM_SESSION_DIRTY: Object has pending changes
 * @ORM_SESSION_DELETED: Object is marked for deletion
 * @ORM_SESSION_DETACHED: Object is detached from session
 *
 * States of objects tracked by an OrmSession.
 */
typedef enum {
    ORM_SESSION_NEW,
    ORM_SESSION_PERSISTENT,
    ORM_SESSION_DIRTY,
    ORM_SESSION_DELETED,
    ORM_SESSION_DETACHED
} OrmSessionState;

/**
 * OrmIsolationLevel:
 * @ORM_ISOLATION_READ_UNCOMMITTED: Read uncommitted isolation
 * @ORM_ISOLATION_READ_COMMITTED: Read committed isolation
 * @ORM_ISOLATION_REPEATABLE_READ: Repeatable read isolation
 * @ORM_ISOLATION_SERIALIZABLE: Serializable isolation
 *
 * Transaction isolation levels.
 */
typedef enum {
    ORM_ISOLATION_READ_UNCOMMITTED,
    ORM_ISOLATION_READ_COMMITTED,
    ORM_ISOLATION_REPEATABLE_READ,
    ORM_ISOLATION_SERIALIZABLE
} OrmIsolationLevel;

/**
 * OrmConnectionState:
 * @ORM_CONNECTION_CLOSED: Not connected, and not going to be
 * @ORM_CONNECTION_CONNECTING: Opening the backend connection
 * @ORM_CONNECTION_IDLE: Open, with nothing in flight
 * @ORM_CONNECTION_BUSY: Open, running an asynchronous operation
 *
 * The lifecycle of an #OrmConnection, as its "state-changed" signal
 * reports it.
 *
 * The distinction that earns this enum is %ORM_CONNECTION_IDLE against
 * %ORM_CONNECTION_BUSY: a connection runs one operation at a time, so a
 * user interface that lets a second query be started while the first is
 * still running has only queued it, and saying so is the difference
 * between a responsive application and one that looks hung.
 */
typedef enum {
    ORM_CONNECTION_CLOSED,
    ORM_CONNECTION_CONNECTING,
    ORM_CONNECTION_IDLE,
    ORM_CONNECTION_BUSY
} OrmConnectionState;

/**
 * OrmQueryFlags:
 * @ORM_QUERY_FLAGS_NONE: No special handling
 * @ORM_QUERY_FLAGS_STREAMING: Ask the backend to deliver rows incrementally
 *
 * Options for how a query's results are produced.
 *
 * Streaming is a request, not a guarantee. SQLite always streams because
 * stepping a statement is how it works at all; PostgreSQL and MySQL
 * materialize the whole result client-side unless asked otherwise, and
 * asking costs something -- a streaming result holds the connection busy
 * until it is drained or closed. So the flag is opt-in, for the caller
 * who is about to read a million rows and does not want them all in
 * memory first.
 */
typedef enum {
    ORM_QUERY_FLAGS_NONE      = 0,
    ORM_QUERY_FLAGS_STREAMING = 1 << 0
} OrmQueryFlags;

/**
 * OrmJsonLayout:
 * @ORM_JSON_LAYOUT_ARRAY_OF_OBJECTS: One object per row, keyed by column name
 * @ORM_JSON_LAYOUT_ARRAY_OF_ARRAYS: One array per row, in column order
 *
 * How a JSON export arranges a result set.
 *
 * Objects are what an HTTP client or a JavaScript consumer expects, and
 * survive a column being added or reordered. Arrays repeat the column
 * names once instead of once per row, which on a wide result is most of
 * the file -- so they are the choice for bulk data, at the cost of the
 * reader having to carry the column list alongside.
 */
typedef enum {
    ORM_JSON_LAYOUT_ARRAY_OF_OBJECTS,
    ORM_JSON_LAYOUT_ARRAY_OF_ARRAYS
} OrmJsonLayout;

/* GType registration functions */
GType orm_dialect_type_get_type (void) G_GNUC_CONST;
GType orm_value_type_get_type (void) G_GNUC_CONST;
GType orm_compare_op_get_type (void) G_GNUC_CONST;
GType orm_logical_op_get_type (void) G_GNUC_CONST;
GType orm_order_direction_get_type (void) G_GNUC_CONST;
GType orm_join_type_get_type (void) G_GNUC_CONST;
GType orm_constraint_type_get_type (void) G_GNUC_CONST;
GType orm_foreign_key_action_get_type (void) G_GNUC_CONST;
GType orm_relationship_type_get_type (void) G_GNUC_CONST;
GType orm_session_state_get_type (void) G_GNUC_CONST;
GType orm_isolation_level_get_type (void) G_GNUC_CONST;
GType orm_connection_state_get_type (void) G_GNUC_CONST;
GType orm_query_flags_get_type (void) G_GNUC_CONST;
GType orm_json_layout_get_type (void) G_GNUC_CONST;

#define ORM_TYPE_DIALECT_TYPE (orm_dialect_type_get_type ())
#define ORM_TYPE_VALUE_TYPE (orm_value_type_get_type ())
#define ORM_TYPE_COMPARE_OP (orm_compare_op_get_type ())
#define ORM_TYPE_LOGICAL_OP (orm_logical_op_get_type ())
#define ORM_TYPE_ORDER_DIRECTION (orm_order_direction_get_type ())
#define ORM_TYPE_JOIN_TYPE (orm_join_type_get_type ())
#define ORM_TYPE_CONSTRAINT_TYPE (orm_constraint_type_get_type ())
#define ORM_TYPE_FOREIGN_KEY_ACTION (orm_foreign_key_action_get_type ())
#define ORM_TYPE_RELATIONSHIP_TYPE (orm_relationship_type_get_type ())
#define ORM_TYPE_SESSION_STATE (orm_session_state_get_type ())
#define ORM_TYPE_ISOLATION_LEVEL (orm_isolation_level_get_type ())
#define ORM_TYPE_CONNECTION_STATE (orm_connection_state_get_type ())
#define ORM_TYPE_QUERY_FLAGS (orm_query_flags_get_type ())
#define ORM_TYPE_JSON_LAYOUT (orm_json_layout_get_type ())

G_END_DECLS

#endif /* ORM_ENUMS_H */
