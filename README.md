# orm-glib

A GObject/GLib ORM library for C, inspired by SQLAlchemy.

## Overview

orm-glib provides an Object-Relational Mapping layer for GLib-based applications, enabling GObject serialization to databases. It supports SQLite, PostgreSQL, and MySQL/MariaDB through a unified API.

## Features

- **GObject Integration**: All types are GObjects, enabling introspection and language bindings
- **Type System**: Full SQL type hierarchy mapping GLib types to database types
- **Schema Definition**: Define tables, columns, constraints, and indexes in code
- **Expression Language**: Build type-safe SQL queries programmatically
- **Session Management**: Unit of Work pattern with identity mapping
- **Multiple Dialects**: Support for SQLite, PostgreSQL, and MySQL
- **Migrations**: Explicit, versioned schema changes with checksummed history

## Requirements

- GLib 2.56+
- GObject 2.56+
- GIO 2.56+
- SQLite 3 (optional, for SQLite support)
- libpq (optional, for PostgreSQL support)
- MySQL client library (optional, for MySQL support)

## Building

```bash
# Build the library
make

# Run tests
make test

# Install
sudo make install
```

### Build Options

```bash
# Debug build
make DEBUG=1

# With sanitizers
make DEBUG=1 ASAN=1

# Enable PostgreSQL support
make ENABLE_POSTGRES=1

# Custom installation prefix
make PREFIX=/usr/local install
```

## Quick Start

### Define a Schema

```c
#include <orm.h>

/* Create metadata container */
g_autoptr(OrmMetadata) metadata = orm_metadata_new ();

/* Define a table */
g_autoptr(OrmTable) users = orm_table_new ("users", metadata);

/* Add columns */
g_autoptr(OrmInteger) int_type = orm_integer_new ();
g_autoptr(OrmString) str_type = orm_string_new (255);

orm_table_add_column_full (users, "id", ORM_SQL_TYPE (int_type),
                           TRUE,   /* primary_key */
                           FALSE,  /* nullable */
                           FALSE,  /* unique */
                           TRUE,   /* autoincrement */
                           NULL);  /* default */

orm_table_add_column_full (users, "name", ORM_SQL_TYPE (str_type),
                           FALSE, FALSE, FALSE, FALSE, NULL);
```

### Create and Query Objects (Coming Soon)

```c
/* Create engine and session */
g_autoptr(OrmEngine) engine = orm_engine_new ("sqlite:///app.db", &error);
g_autoptr(OrmSession) session = orm_session_new (engine);

/* Create and persist object */
g_autoptr(User) user = g_object_new (TYPE_USER,
    "name", "John",
    "email", "john@example.com",
    NULL);
orm_session_add (session, G_OBJECT (user));
orm_session_commit (session, &error);

/* Query objects */
g_autoptr(OrmQuery) query = orm_session_query (session, TYPE_USER);
orm_query_filter (query, "active", ORM_OP_EQ, orm_value_new_boolean (TRUE));
GList *users = orm_query_all (query, &error);
```

## Type Mapping

| GLib Type | ORM Type | SQLite | PostgreSQL | MySQL |
|-----------|----------|--------|------------|-------|
| gint | OrmInteger | INTEGER | INTEGER | INT |
| gint64 | OrmBigInt | INTEGER | BIGINT | BIGINT |
| gboolean | OrmBooleanType | INTEGER | BOOLEAN | TINYINT(1) |
| gchar* | OrmString | TEXT | VARCHAR | VARCHAR |
| GBytes* | OrmBlobType | BLOB | BYTEA | BLOB |
| gdouble | OrmDoubleType | REAL | DOUBLE | DOUBLE |
| GDateTime* | OrmDateTimeType | TEXT | TIMESTAMP | DATETIME |

## Documentation

See the [docs/](docs/) directory for detailed documentation:

- [Architecture Overview](docs/architecture.md)
- [Getting Started Guide](docs/getting-started.md)
- [API Reference](docs/api/)

## License

This library is licensed under the GNU Affero General Public License v3.0 or later (AGPL-3.0-or-later).

See [LICENSE](LICENSE) for the full license text.

## Contributing

Contributions are welcome! Please ensure your code follows the project's coding style (gnu89 C with GLib conventions) and includes appropriate tests.
