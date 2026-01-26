/* orm-error.h
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

#ifndef ORM_ERROR_H
#define ORM_ERROR_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * ORM_ERROR:
 *
 * Error domain for orm-glib errors. Errors in this domain will be from
 * the #OrmError enumeration. See #GError for more information on error
 * domains.
 */
#define ORM_ERROR (orm_error_quark ())

/**
 * OrmError:
 * @ORM_ERROR_INVALID_URL: Invalid database URL format
 * @ORM_ERROR_CONNECTION_FAILED: Failed to connect to database
 * @ORM_ERROR_CONNECTION_CLOSED: Connection is closed
 * @ORM_ERROR_QUERY_FAILED: Query execution failed
 * @ORM_ERROR_TRANSACTION_FAILED: Transaction operation failed
 * @ORM_ERROR_CONSTRAINT_VIOLATION: Database constraint violated
 * @ORM_ERROR_SERIALIZATION_FAILED: Object serialization failed
 * @ORM_ERROR_DESERIALIZATION_FAILED: Object deserialization failed
 * @ORM_ERROR_PROPERTY_NOT_FOUND: Property not found on object
 * @ORM_ERROR_TYPE_MISMATCH: Type mismatch during conversion
 * @ORM_ERROR_INVALID_OPERATION: Invalid operation for current state
 * @ORM_ERROR_NOT_FOUND: Object not found in database
 * @ORM_ERROR_ALREADY_EXISTS: Object already exists
 * @ORM_ERROR_DIALECT_NOT_SUPPORTED: Database dialect not supported
 * @ORM_ERROR_DRIVER_NOT_AVAILABLE: Database driver not available
 * @ORM_ERROR_SCHEMA_ERROR: Schema definition error
 * @ORM_ERROR_MAPPER_ERROR: Mapper configuration error
 * @ORM_ERROR_SESSION_ERROR: Session state error
 * @ORM_ERROR_INTEGRITY_ERROR: Data integrity error
 * @ORM_ERROR_NOT_IMPLEMENTED: Feature not implemented
 * @ORM_ERROR_NOT_SUPPORTED: Operation not supported
 * @ORM_ERROR_PREPARE: Statement preparation failed
 * @ORM_ERROR_BIND: Parameter binding failed
 * @ORM_ERROR_CONNECTION: Connection error
 * @ORM_ERROR_EXECUTE: Statement execution failed
 *
 * Error codes returned by orm-glib functions.
 */
typedef enum {
    ORM_ERROR_INVALID_URL,
    ORM_ERROR_CONNECTION_FAILED,
    ORM_ERROR_CONNECTION_CLOSED,
    ORM_ERROR_QUERY_FAILED,
    ORM_ERROR_TRANSACTION_FAILED,
    ORM_ERROR_CONSTRAINT_VIOLATION,
    ORM_ERROR_SERIALIZATION_FAILED,
    ORM_ERROR_DESERIALIZATION_FAILED,
    ORM_ERROR_PROPERTY_NOT_FOUND,
    ORM_ERROR_TYPE_MISMATCH,
    ORM_ERROR_INVALID_OPERATION,
    ORM_ERROR_NOT_FOUND,
    ORM_ERROR_ALREADY_EXISTS,
    ORM_ERROR_DIALECT_NOT_SUPPORTED,
    ORM_ERROR_DRIVER_NOT_AVAILABLE,
    ORM_ERROR_SCHEMA_ERROR,
    ORM_ERROR_MAPPER_ERROR,
    ORM_ERROR_SESSION_ERROR,
    ORM_ERROR_INTEGRITY_ERROR,
    ORM_ERROR_NOT_IMPLEMENTED,
    ORM_ERROR_NOT_SUPPORTED,
    ORM_ERROR_PREPARE,
    ORM_ERROR_BIND,
    ORM_ERROR_CONNECTION,
    ORM_ERROR_EXECUTE
} OrmError;

GQuark  orm_error_quark (void) G_GNUC_CONST;
GType   orm_error_get_type (void) G_GNUC_CONST;

#define ORM_TYPE_ERROR (orm_error_get_type ())

G_END_DECLS

#endif /* ORM_ERROR_H */
