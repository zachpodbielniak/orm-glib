# orm-glib Documentation

orm-glib is a GObject/GLib ORM library for C, inspired by SQLAlchemy. It provides a type-safe way to persist GObjects to relational databases.

## Features

- **Multiple Database Support**: SQLite, PostgreSQL, MySQL/MariaDB
- **GObject Integration**: Serialize GObjects directly to database rows
- **Type Safety**: SQL types map to GLib types
- **Query Builder**: Fluent API for building type-safe queries
- **Session Management**: Unit of Work pattern with identity map
- **Schema Generation**: Generate DDL from GObject definitions
- **Export**: Write result sets out as CSV or JSON
- **Async**: `GTask`-based queries with `GCancellable`, incremental row streaming, and connection state signals
- **GObject Introspection**: Full GIR support for language bindings

## Quick Example

```c
/* Create engine and session */
g_autoptr(OrmEngine) engine = orm_engine_new ("sqlite:///app.db", &error);
g_autoptr(OrmConnection) conn = orm_engine_connect (engine, &error);
g_autoptr(OrmSession) session = orm_session_new_with_connection (conn);

/* Create and persist an object */
g_autoptr(User) user = g_object_new (TYPE_USER,
    "name", "Alice",
    "email", "alice@example.com",
    NULL);
orm_session_add (session, G_OBJECT (user));
orm_session_commit (session, &error);

/* Query objects */
g_autoptr(OrmQuery) query = orm_session_query (session, TYPE_USER);
g_autoptr(OrmValue) active = orm_value_new_boolean (TRUE);
orm_query_filter_by (query, "active", active);
GList *users = orm_query_all (query, &error);
```

## Documentation

- [Getting Started](getting-started.md) - Installation and first steps
- [Architecture](architecture.md) - Library design and components
- [Dialects](dialects.md) - Database-specific SQL and type mapping
- [Drivers](drivers.md) - Backend I/O, the scheme registry, and adding a database
- [Export](export.md) - Writing a result set out as CSV or JSON
- [Async](async.md) - Running queries off the calling thread, cancelling them, and connection signals
- [OrmSerializable](serializable.md) - Making GObjects persistable

## Building

```bash
# Basic build (SQLite only)
make

# With PostgreSQL support
make ENABLE_POSTGRES=1

# With MySQL support
make ENABLE_MYSQL=1

# Run tests
make test

# Generate GIR
make gir

# Install
make install PREFIX=/usr/local
```

## License

AGPL-3.0-or-later
