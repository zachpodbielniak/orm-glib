/* orm-types.h
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

#ifndef ORM_TYPES_H
#define ORM_TYPES_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/*
 * Forward declarations for all ORM types.
 * This allows headers to reference types without circular dependencies.
 */

/* Core types */
typedef struct _OrmValue            OrmValue;

/* Engine types */
typedef struct _OrmEngine           OrmEngine;
typedef struct _OrmConnection       OrmConnection;
typedef struct _OrmTransaction      OrmTransaction;
typedef struct _OrmResult           OrmResult;
typedef struct _OrmRow              OrmRow;

/* Dialect types */
typedef struct _OrmDialect          OrmDialect;          /* Interface */
typedef struct _OrmTypeCompiler     OrmTypeCompiler;     /* Interface */
typedef struct _OrmSqlCompiler      OrmSqlCompiler;      /* Interface */
typedef struct _OrmDdlCompiler      OrmDdlCompiler;      /* Interface */
typedef struct _OrmSqliteDialect    OrmSqliteDialect;
typedef struct _OrmPostgresDialect  OrmPostgresDialect;
typedef struct _OrmMysqlDialect     OrmMysqlDialect;

/* SQL expression types */
typedef struct _OrmExpression       OrmExpression;       /* Derivable */
typedef struct _OrmColumnElement    OrmColumnElement;
typedef struct _OrmTableClause      OrmTableClause;
typedef struct _OrmBinaryExpression OrmBinaryExpression;
typedef struct _OrmLiteral          OrmLiteral;
typedef struct _OrmSelect           OrmSelect;
typedef struct _OrmInsert           OrmInsert;
typedef struct _OrmUpdate           OrmUpdate;
typedef struct _OrmDelete           OrmDelete;

/* Schema types */
typedef struct _OrmMetadata         OrmMetadata;
typedef struct _OrmTable            OrmTable;
typedef struct _OrmColumn           OrmColumn;
typedef struct _OrmPrimaryKey       OrmPrimaryKey;
typedef struct _OrmForeignKey       OrmForeignKey;
typedef struct _OrmIndex            OrmIndex;
typedef struct _OrmConstraint       OrmConstraint;       /* Derivable */

/* SQL type types */
typedef struct _OrmSqlType          OrmSqlType;          /* Derivable */
typedef struct _OrmInteger          OrmInteger;
typedef struct _OrmBigInt           OrmBigInt;
typedef struct _OrmString           OrmString;
typedef struct _OrmText             OrmText;
typedef struct _OrmBoolean          OrmBoolean;
typedef struct _OrmFloat            OrmFloat;
typedef struct _OrmDouble           OrmDouble;
typedef struct _OrmDateTime         OrmDateTime;
typedef struct _OrmBlob             OrmBlob;

/* ORM types */
typedef struct _OrmSerializable     OrmSerializable;     /* Interface */
typedef struct _OrmMapper           OrmMapper;
typedef struct _OrmProperty         OrmProperty;
typedef struct _OrmRelationship     OrmRelationship;
typedef struct _OrmSession          OrmSession;
typedef struct _OrmIdentityMap      OrmIdentityMap;
typedef struct _OrmQuery            OrmQuery;

G_END_DECLS

#endif /* ORM_TYPES_H */
