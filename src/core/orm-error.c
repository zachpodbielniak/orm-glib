/* orm-error.c
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

#include "orm-error.h"
#include <glib-object.h>

/**
 * orm_error_quark:
 *
 * Gets the error quark for orm-glib errors.
 *
 * Returns: The error quark for #OrmError
 */
GQuark
orm_error_quark (void)
{
    return g_quark_from_static_string ("orm-error-quark");
}

/*
 * GType registration for OrmError enumeration.
 * Registers error codes with the GObject type system for introspection.
 */
GType
orm_error_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_ERROR_INVALID_URL,
              "ORM_ERROR_INVALID_URL",
              "invalid-url" },
            { ORM_ERROR_CONNECTION_FAILED,
              "ORM_ERROR_CONNECTION_FAILED",
              "connection-failed" },
            { ORM_ERROR_CONNECTION_CLOSED,
              "ORM_ERROR_CONNECTION_CLOSED",
              "connection-closed" },
            { ORM_ERROR_QUERY_FAILED,
              "ORM_ERROR_QUERY_FAILED",
              "query-failed" },
            { ORM_ERROR_TRANSACTION_FAILED,
              "ORM_ERROR_TRANSACTION_FAILED",
              "transaction-failed" },
            { ORM_ERROR_CONSTRAINT_VIOLATION,
              "ORM_ERROR_CONSTRAINT_VIOLATION",
              "constraint-violation" },
            { ORM_ERROR_SERIALIZATION_FAILED,
              "ORM_ERROR_SERIALIZATION_FAILED",
              "serialization-failed" },
            { ORM_ERROR_DESERIALIZATION_FAILED,
              "ORM_ERROR_DESERIALIZATION_FAILED",
              "deserialization-failed" },
            { ORM_ERROR_PROPERTY_NOT_FOUND,
              "ORM_ERROR_PROPERTY_NOT_FOUND",
              "property-not-found" },
            { ORM_ERROR_TYPE_MISMATCH,
              "ORM_ERROR_TYPE_MISMATCH",
              "type-mismatch" },
            { ORM_ERROR_INVALID_OPERATION,
              "ORM_ERROR_INVALID_OPERATION",
              "invalid-operation" },
            { ORM_ERROR_NOT_FOUND,
              "ORM_ERROR_NOT_FOUND",
              "not-found" },
            { ORM_ERROR_ALREADY_EXISTS,
              "ORM_ERROR_ALREADY_EXISTS",
              "already-exists" },
            { ORM_ERROR_DIALECT_NOT_SUPPORTED,
              "ORM_ERROR_DIALECT_NOT_SUPPORTED",
              "dialect-not-supported" },
            { ORM_ERROR_DRIVER_NOT_AVAILABLE,
              "ORM_ERROR_DRIVER_NOT_AVAILABLE",
              "driver-not-available" },
            { ORM_ERROR_SCHEMA_ERROR,
              "ORM_ERROR_SCHEMA_ERROR",
              "schema-error" },
            { ORM_ERROR_MAPPER_ERROR,
              "ORM_ERROR_MAPPER_ERROR",
              "mapper-error" },
            { ORM_ERROR_SESSION_ERROR,
              "ORM_ERROR_SESSION_ERROR",
              "session-error" },
            { ORM_ERROR_INTEGRITY_ERROR,
              "ORM_ERROR_INTEGRITY_ERROR",
              "integrity-error" },
            { ORM_ERROR_NOT_IMPLEMENTED,
              "ORM_ERROR_NOT_IMPLEMENTED",
              "not-implemented" },
            { ORM_ERROR_NOT_SUPPORTED,
              "ORM_ERROR_NOT_SUPPORTED",
              "not-supported" },
            { ORM_ERROR_PREPARE,
              "ORM_ERROR_PREPARE",
              "prepare" },
            { ORM_ERROR_BIND,
              "ORM_ERROR_BIND",
              "bind" },
            { ORM_ERROR_CONNECTION,
              "ORM_ERROR_CONNECTION",
              "connection" },
            { ORM_ERROR_EXECUTE,
              "ORM_ERROR_EXECUTE",
              "execute" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmError", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}
