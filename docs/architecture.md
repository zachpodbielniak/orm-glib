# orm-glib Architecture

This document describes the internal architecture and design patterns used in orm-glib.

## Overview

orm-glib is organized into several layers, each with distinct responsibilities:

```
┌─────────────────────────────────────────────────────────────┐
│                      ORM Layer                              │
│  (OrmSession, OrmQuery, OrmMapper, OrmSerializable)         │
├─────────────────────────────────────────────────────────────┤
│                    SQL Expression Layer                     │
│  (OrmSelect, OrmInsert, OrmUpdate, OrmDelete, OrmExpression)│
├─────────────────────────────────────────────────────────────┤
│                      Schema Layer                           │
│  (OrmTable, OrmColumn, OrmForeignKey, OrmIndex, OrmMetadata)│
├─────────────────────────────────────────────────────────────┤
│                      Dialect Layer                          │
│  (OrmDialect, OrmTypeCompiler, OrmDdlCompiler)              │
├─────────────────────────────────────────────────────────────┤
│                      Engine Layer                           │
│  (OrmEngine, OrmConnection, OrmTransaction, OrmResult)      │
├─────────────────────────────────────────────────────────────┤
│                      Core Layer                             │
│  (OrmValue, OrmError, OrmEnums, SQL Types)                  │
└─────────────────────────────────────────────────────────────┘
```

## Core Layer

### OrmValue

`OrmValue` is the fundamental value wrapper for database data. It provides a uniform interface for handling different SQL types.

```c
typedef enum {
    ORM_VALUE_TYPE_NULL,
    ORM_VALUE_TYPE_INTEGER,
    ORM_VALUE_TYPE_FLOAT,
    ORM_VALUE_TYPE_STRING,
    ORM_VALUE_TYPE_BOOLEAN,
    ORM_VALUE_TYPE_DATETIME,
    ORM_VALUE_TYPE_BLOB
} OrmValueType;
```

### SQL Types

The `src/types/` directory contains OrmSqlType subclasses that define how GLib types map to SQL column types:

| Class | GType | SQLite | PostgreSQL | MySQL |
|-------|-------|--------|------------|-------|
| OrmInteger | G_TYPE_INT64 | INTEGER | BIGINT | BIGINT |
| OrmString | G_TYPE_STRING | TEXT | VARCHAR | VARCHAR |
| OrmText | G_TYPE_STRING | TEXT | TEXT | TEXT |
| OrmBoolean | G_TYPE_BOOLEAN | INTEGER | BOOLEAN | TINYINT(1) |
| OrmFloat | G_TYPE_DOUBLE | REAL | DOUBLE | DOUBLE |
| OrmDateTime | G_TYPE_DATE_TIME | TEXT | TIMESTAMP | DATETIME |
| OrmBlob | G_TYPE_BYTES | BLOB | BYTEA | BLOB |

## Engine Layer

### OrmEngine

The entry point for database connections. Parses connection URLs and creates the appropriate dialect.

```c
OrmEngine *engine = orm_engine_new ("sqlite:///app.db", &error);
OrmDialect *dialect = orm_engine_get_dialect (engine);
OrmConnection *conn = orm_engine_connect (engine, &error);
```

**Responsibilities:**
- Parse connection URL (driver, host, port, database, credentials)
- Instantiate the correct dialect
- Create connections

### OrmConnection

Wraps the native database connection handle and provides a uniform interface.

```c
gboolean orm_connection_execute (OrmConnection *conn, const gchar *sql, GError **error);
OrmResult *orm_connection_query (OrmConnection *conn, const gchar *sql, GError **error);
OrmTransaction *orm_connection_begin (OrmConnection *conn, GError **error);
```

### OrmTransaction

Manages transaction state (begin, commit, rollback).

```c
OrmTransaction *tx = orm_connection_begin (conn, &error);
/* ... operations ... */
orm_transaction_commit (tx, &error);
/* or: orm_transaction_rollback (tx, &error); */
```

### OrmResult / OrmRow

Query results are returned as `OrmResult`, which provides iteration over rows:

```c
OrmResult *result = orm_connection_query (conn, "SELECT * FROM users", &error);
OrmRow *row;
while ((row = orm_result_fetch_row (result)) != NULL)
{
    OrmValue *val = orm_row_get_value (row, "name");
    g_print ("Name: %s\n", orm_value_get_string (val));
    g_object_unref (row);
}
```

## Dialect Layer

### OrmDialect Interface

Dialects encapsulate database-specific behavior:

```c
struct _OrmDialectInterface
{
    GTypeInterface g_iface;

    const gchar *   (*get_name)           (OrmDialect *dialect);
    const gchar *   (*get_driver)         (OrmDialect *dialect);
    gboolean        (*supports_returning) (OrmDialect *dialect);
    OrmTypeCompiler * (*get_type_compiler)  (OrmDialect *dialect);
    OrmDdlCompiler *  (*get_ddl_compiler)   (OrmDialect *dialect);
};
```

### OrmTypeCompiler

Converts OrmSqlType instances to SQL type strings:

```c
/* OrmInteger -> "INTEGER" (SQLite) or "BIGINT" (PostgreSQL) */
gchar *type_str = orm_type_compiler_compile (compiler, sql_type);
```

### OrmDdlCompiler

Generates DDL statements (CREATE TABLE, ALTER TABLE, etc.):

```c
gchar *sql = orm_ddl_compiler_compile_create_table (ddl, table, if_not_exists);
/* Result: CREATE TABLE IF NOT EXISTS users (...) */
```

## Schema Layer

### OrmTable

Represents a database table schema:

```c
OrmTable *table = orm_table_new ("users");
orm_table_add_column (table, column);
orm_table_set_primary_key (table, pk);
orm_table_add_foreign_key (table, fk);
orm_table_add_index (table, idx);
```

### OrmColumn

Represents a table column:

```c
OrmColumn *col = orm_column_new ("email", ORM_TYPE_STRING);
orm_column_set_nullable (col, FALSE);
orm_column_set_unique (col, TRUE);
orm_column_set_default (col, "'unknown'");
```

### OrmForeignKey

Defines foreign key constraints:

```c
OrmForeignKey *fk = orm_foreign_key_new ("fk_posts_author", "authors");
orm_foreign_key_add_column (fk, "author_id", "id");
orm_foreign_key_set_on_delete (fk, ORM_FK_CASCADE);
orm_foreign_key_set_on_update (fk, ORM_FK_SET_NULL);
```

## SQL Expression Layer

### OrmExpression

Base class for SQL expressions. All SQL building blocks inherit from this.

### Statement Builders

- **OrmSelect**: SELECT queries with WHERE, ORDER BY, LIMIT, OFFSET
- **OrmInsert**: INSERT statements
- **OrmUpdate**: UPDATE statements with WHERE clause
- **OrmDelete**: DELETE statements with WHERE clause

```c
OrmSelect *select = orm_select_new ();
orm_select_add_column (select, "name");
orm_select_from (select, "users");
orm_select_where (select, where_expr);
orm_select_order_by (select, "name", FALSE);
orm_select_limit (select, 10);

gchar *sql = orm_select_compile (select, dialect);
/* Result: SELECT name FROM users WHERE ... ORDER BY name LIMIT 10 */
```

## ORM Layer

### OrmSerializable Interface

The key interface that GObjects implement to be persistable:

```c
struct _OrmSerializableInterface
{
    GTypeInterface g_iface;

    /* Serialize property to database value */
    OrmValue *    (*serialize_property)   (OrmSerializable *self,
                                           const gchar     *property_name,
                                           const GValue    *value,
                                           GParamSpec      *pspec);

    /* Deserialize database value to property */
    gboolean      (*deserialize_property) (OrmSerializable *self,
                                           const gchar     *property_name,
                                           GValue          *value,
                                           GParamSpec      *pspec,
                                           OrmValue        *db_value);

    /* Table metadata */
    const gchar * (*get_table_name)       (OrmSerializable *self);
    const gchar * (*get_primary_key)      (OrmSerializable *self);
};
```

### OrmMapper

Maps a GType to a database table. Automatically created from OrmSerializable types:

```c
OrmMapper *mapper = orm_mapper_new_from_serializable (MY_TYPE_USER);
OrmTable *table = orm_mapper_to_table (mapper);
```

The mapper inspects GObject properties and creates corresponding columns.

### OrmSession

Implements the Unit of Work pattern. Tracks object state and batches database operations:

```c
OrmSession *session = orm_session_new_with_connection (conn);
orm_session_register_mapper (session, mapper);

/* Track new object */
orm_session_add (session, obj);

/* Track deletion */
orm_session_delete (session, obj);

/* Flush all changes to database */
orm_session_commit (session, &error);
```

**Object States:**
- **Transient**: Not associated with session
- **Pending**: Added to session, not yet committed
- **Persistent**: Committed to database, tracked by identity map
- **Deleted**: Marked for deletion, pending commit

### OrmIdentityMap

Ensures each database row maps to exactly one GObject instance:

```c
/* Internal to OrmSession */
GObject *obj = orm_identity_map_get (map, TYPE_USER, primary_key_value);
if (obj == NULL)
{
    obj = /* load from database */;
    orm_identity_map_add (map, obj, primary_key_value);
}
```

### OrmQuery

High-level query builder that works with GTypes:

```c
OrmQuery *query = orm_session_query (session, MY_TYPE_USER);

/* Equality filter */
OrmValue *val = orm_value_new_string ("alice@example.com");
orm_query_filter_by (query, "email", val);

/* Comparison filter */
OrmValue *age = orm_value_new_integer (18);
orm_query_filter (query, "age", ORM_OP_GTE, age);

/* Ordering and pagination */
orm_query_order_by (query, "created_at", TRUE);  /* DESC */
orm_query_limit (query, 20);
orm_query_offset (query, 40);

/* Execute */
GList *results = orm_query_all (query, &error);
GObject *first = orm_query_first (query, &error);
gint64 count = orm_query_count (query, &error);
```

## Design Patterns

### Factory Pattern

`OrmEngine` acts as a factory for connections and selects the appropriate dialect based on the connection URL.

### Unit of Work

`OrmSession` tracks all changes to objects and commits them as a single transaction.

### Identity Map

Ensures object identity - the same database row always maps to the same GObject instance within a session.

### Repository Pattern

`OrmQuery` provides a collection-like interface for querying entities.

### Strategy Pattern

Dialects implement strategy pattern - different SQL generation strategies for different databases.

## Thread Safety

orm-glib is **not thread-safe** by default. Each thread should have its own:
- `OrmSession` instance
- `OrmConnection` instance

The `OrmEngine` can be shared across threads for creating new connections.

## Memory Management

orm-glib uses GLib's reference counting. All returned objects should be unreferenced when done:

```c
g_autoptr(OrmSession) session = orm_session_new_with_connection (conn);
g_autoptr(OrmQuery) query = orm_session_query (session, MY_TYPE);
g_autoptr(GList) results = orm_query_all (query, &error);
/* ... use results ... */
g_list_free_full (results, g_object_unref);
```

Use `g_autoptr()` for automatic cleanup where possible.
