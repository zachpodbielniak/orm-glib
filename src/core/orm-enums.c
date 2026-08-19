/* orm-enums.c
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

#include "orm-enums.h"
#include <glib-object.h>

/*
 * GType registration for OrmDialectType enumeration.
 * Registers the enum values with GObject type system for introspection.
 */
GType
orm_dialect_type_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_DIALECT_SQLITE, "ORM_DIALECT_SQLITE", "sqlite" },
            { ORM_DIALECT_POSTGRES, "ORM_DIALECT_POSTGRES", "postgres" },
            { ORM_DIALECT_MYSQL, "ORM_DIALECT_MYSQL", "mysql" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmDialectType", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * GType registration for OrmValueType enumeration.
 * Defines the types of values that can be stored in OrmValue.
 */
GType
orm_value_type_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_VALUE_NULL, "ORM_VALUE_NULL", "null" },
            { ORM_VALUE_INTEGER, "ORM_VALUE_INTEGER", "integer" },
            { ORM_VALUE_FLOAT, "ORM_VALUE_FLOAT", "float" },
            { ORM_VALUE_STRING, "ORM_VALUE_STRING", "string" },
            { ORM_VALUE_BLOB, "ORM_VALUE_BLOB", "blob" },
            { ORM_VALUE_BOOLEAN, "ORM_VALUE_BOOLEAN", "boolean" },
            { ORM_VALUE_DATETIME, "ORM_VALUE_DATETIME", "datetime" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmValueType", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * GType registration for OrmCompareOp enumeration.
 * Defines comparison operators for query filters.
 */
GType
orm_compare_op_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_OP_EQ, "ORM_OP_EQ", "eq" },
            { ORM_OP_NE, "ORM_OP_NE", "ne" },
            { ORM_OP_LT, "ORM_OP_LT", "lt" },
            { ORM_OP_LE, "ORM_OP_LE", "le" },
            { ORM_OP_GT, "ORM_OP_GT", "gt" },
            { ORM_OP_GE, "ORM_OP_GE", "ge" },
            { ORM_OP_LIKE, "ORM_OP_LIKE", "like" },
            { ORM_OP_ILIKE, "ORM_OP_ILIKE", "ilike" },
            { ORM_OP_IN, "ORM_OP_IN", "in" },
            { ORM_OP_NOT_IN, "ORM_OP_NOT_IN", "not-in" },
            { ORM_OP_IS_NULL, "ORM_OP_IS_NULL", "is-null" },
            { ORM_OP_IS_NOT_NULL, "ORM_OP_IS_NOT_NULL", "is-not-null" },
            { ORM_OP_BETWEEN, "ORM_OP_BETWEEN", "between" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmCompareOp", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * GType registration for OrmLogicalOp enumeration.
 * Defines logical operators for combining expressions.
 */
GType
orm_logical_op_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_LOGICAL_AND, "ORM_LOGICAL_AND", "and" },
            { ORM_LOGICAL_OR, "ORM_LOGICAL_OR", "or" },
            { ORM_LOGICAL_NOT, "ORM_LOGICAL_NOT", "not" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmLogicalOp", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * GType registration for OrmOrderDirection enumeration.
 * Defines sort order direction for ORDER BY clauses.
 */
GType
orm_order_direction_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_ORDER_ASC, "ORM_ORDER_ASC", "asc" },
            { ORM_ORDER_DESC, "ORM_ORDER_DESC", "desc" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmOrderDirection", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * GType registration for OrmJoinType enumeration.
 * Defines types of SQL JOIN operations.
 */
GType
orm_join_type_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_JOIN_INNER, "ORM_JOIN_INNER", "inner" },
            { ORM_JOIN_LEFT, "ORM_JOIN_LEFT", "left" },
            { ORM_JOIN_RIGHT, "ORM_JOIN_RIGHT", "right" },
            { ORM_JOIN_FULL, "ORM_JOIN_FULL", "full" },
            { ORM_JOIN_CROSS, "ORM_JOIN_CROSS", "cross" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmJoinType", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * GType registration for OrmConstraintType enumeration.
 * Defines types of table constraints.
 */
GType
orm_constraint_type_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_CONSTRAINT_PRIMARY_KEY, "ORM_CONSTRAINT_PRIMARY_KEY", "primary-key" },
            { ORM_CONSTRAINT_FOREIGN_KEY, "ORM_CONSTRAINT_FOREIGN_KEY", "foreign-key" },
            { ORM_CONSTRAINT_UNIQUE, "ORM_CONSTRAINT_UNIQUE", "unique" },
            { ORM_CONSTRAINT_CHECK, "ORM_CONSTRAINT_CHECK", "check" },
            { ORM_CONSTRAINT_NOT_NULL, "ORM_CONSTRAINT_NOT_NULL", "not-null" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmConstraintType", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * GType registration for OrmForeignKeyAction enumeration.
 * Defines referential actions for foreign keys.
 */
GType
orm_foreign_key_action_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_FK_NO_ACTION, "ORM_FK_NO_ACTION", "no-action" },
            { ORM_FK_RESTRICT, "ORM_FK_RESTRICT", "restrict" },
            { ORM_FK_CASCADE, "ORM_FK_CASCADE", "cascade" },
            { ORM_FK_SET_NULL, "ORM_FK_SET_NULL", "set-null" },
            { ORM_FK_SET_DEFAULT, "ORM_FK_SET_DEFAULT", "set-default" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmForeignKeyAction", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * GType registration for OrmRelationshipType enumeration.
 * Defines types of ORM relationships between objects.
 */
GType
orm_relationship_type_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_RELATIONSHIP_ONE_TO_ONE, "ORM_RELATIONSHIP_ONE_TO_ONE", "one-to-one" },
            { ORM_RELATIONSHIP_ONE_TO_MANY, "ORM_RELATIONSHIP_ONE_TO_MANY", "one-to-many" },
            { ORM_RELATIONSHIP_MANY_TO_ONE, "ORM_RELATIONSHIP_MANY_TO_ONE", "many-to-one" },
            { ORM_RELATIONSHIP_MANY_TO_MANY, "ORM_RELATIONSHIP_MANY_TO_MANY", "many-to-many" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmRelationshipType", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * GType registration for OrmSessionState enumeration.
 * Defines states of objects tracked by an OrmSession.
 */
GType
orm_session_state_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_SESSION_NEW, "ORM_SESSION_NEW", "new" },
            { ORM_SESSION_PERSISTENT, "ORM_SESSION_PERSISTENT", "persistent" },
            { ORM_SESSION_DIRTY, "ORM_SESSION_DIRTY", "dirty" },
            { ORM_SESSION_DELETED, "ORM_SESSION_DELETED", "deleted" },
            { ORM_SESSION_DETACHED, "ORM_SESSION_DETACHED", "detached" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmSessionState", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * GType registration for OrmIsolationLevel enumeration.
 * Defines transaction isolation levels.
 */
GType
orm_isolation_level_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_ISOLATION_READ_UNCOMMITTED, "ORM_ISOLATION_READ_UNCOMMITTED", "read-uncommitted" },
            { ORM_ISOLATION_READ_COMMITTED, "ORM_ISOLATION_READ_COMMITTED", "read-committed" },
            { ORM_ISOLATION_REPEATABLE_READ, "ORM_ISOLATION_REPEATABLE_READ", "repeatable-read" },
            { ORM_ISOLATION_SERIALIZABLE, "ORM_ISOLATION_SERIALIZABLE", "serializable" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmIsolationLevel", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * GType registration for OrmQueryFlags.  A bitmask, hence flags rather
 * than enum registration.
 */
GType
orm_query_flags_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GFlagsValue values[] = {
            { ORM_QUERY_FLAGS_NONE, "ORM_QUERY_FLAGS_NONE", "none" },
            { ORM_QUERY_FLAGS_STREAMING, "ORM_QUERY_FLAGS_STREAMING", "streaming" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_flags_register_static ("OrmQueryFlags", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}
