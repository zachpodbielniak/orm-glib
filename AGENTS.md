# orm-glib Development Guide

## Project Overview

orm-glib is a GObject/GLib ORM library for C, inspired by SQLAlchemy. It provides:
- GObject serialization to databases (SQLite, PostgreSQL, MySQL/MariaDB)
- SQL expression language for building type-safe queries
- Schema definition and migration support
- Session management with identity mapping (Unit of Work pattern)

## Build System

### Quick Start
```bash
# Build library
make

# Build with debug symbols
make DEBUG=1

# Run tests
make test

# Build with address sanitizer
make DEBUG=1 ASAN=1 test
```

### Dependencies
Required packages (Fedora):
```bash
sudo dnf install glib2-devel sqlite-devel gcc make
```

Optional packages:
```bash
# PostgreSQL support
sudo dnf install libpq-devel

# MySQL support
sudo dnf install mysql-devel
```

### Build Options
- `DEBUG=1` - Enable debug build with symbols
- `ASAN=1` - Enable address sanitizer
- `UBSAN=1` - Enable undefined behavior sanitizer
- `ENABLE_SQLITE=1` - Enable SQLite support (default)
- `ENABLE_POSTGRES=1` - Enable PostgreSQL support
- `ENABLE_MYSQL=1` - Enable MySQL support
- `PREFIX=/path` - Installation prefix

## Code Style

This project follows gnu89 C with GLib/GObject conventions:

### Naming
- Macros/defines: `UPPERCASE_SNAKE_CASE`
- Types/classes: `PascalCase` (e.g., `OrmTable`)
- Functions/variables: `lowercase_snake_case`
- Prefix all public symbols with `orm_`

### Comments
- Use `/* comment */` for all comments (never `//`)
- Document all public functions with GObject Introspection compatible comments
- Include `@param`, `@returns`, `(transfer ...)`, `(nullable)` annotations

### Memory Management
- Use `g_autoptr()` for automatic cleanup
- Use `g_steal_pointer()` when transferring ownership
- Follow GLib reference counting conventions

### Example Function
```c
/**
 * orm_table_new:
 * @name: The table name
 * @metadata: (nullable): The metadata container
 *
 * Creates a new table definition.
 *
 * Returns: (transfer full): A new #OrmTable
 */
OrmTable *
orm_table_new (const gchar *name,
               OrmMetadata *metadata)
{
    g_return_val_if_fail (name != NULL, NULL);

    return g_object_new (ORM_TYPE_TABLE,
                         "name", name,
                         "metadata", metadata,
                         NULL);
}
```

## Project Structure

```
src/
├── core/           # Core types (OrmValue, errors, enums)
├── types/          # SQL type system (OrmSqlType hierarchy)
├── schema/         # Schema definitions (OrmTable, OrmColumn)
├── engine/         # Database connections (OrmEngine, OrmConnection)
├── migration/      # Explicit versioned schema runner (OrmMigrator)
├── dialect/        # Database-specific implementations
├── sql/            # SQL expression language
├── orm/            # ORM layer (OrmSession, OrmMapper)
└── export/         # Result sets out as text (OrmExporter: CSV, JSON)
```

## Implementation Status

### Phase 1 (Complete)
- [x] Core types and utilities
- [x] SQL type system
- [x] Schema definitions
- [x] Build system

### Phase 2 (Complete)
- [x] Dialect interface
- [x] SQLite dialect
- [x] Basic tests

### Phase 3 (Complete)
- [x] SQL expression language
- [x] SELECT/INSERT/UPDATE/DELETE builders

### Phase 4 (Complete)
- [x] Engine layer
- [x] Connection management
- [x] Transaction support (including savepoints and isolation levels)

### Phase 5 (Complete)
- [x] OrmSerializable interface
- [x] Mapper and session
- [x] ORM queries

### Phase 6 (Complete)
- [x] PostgreSQL dialect
- [x] MySQL dialect

### Phase 7 (Complete)
- [x] GObject introspection
- [x] Documentation
- [x] Examples

### Phase 8 (Complete)
- [x] Async layer: `GTask` API on engine, connection and inspector
- [x] One serialized worker thread per connection, shared with the
      synchronous API so the two cannot race
- [x] `GCancellable` wired to the driver `interrupt` vfunc
- [x] `OrmRowStream` for incremental row delivery
- [x] `OrmConnection` "state-changed" and "notice" signals

### Phase 9 (Complete)
- [x] Explicit versioned migrations (`OrmMigration` / `OrmMigrator`)
- [x] Per-step transactions, checksummed history, cooperative locking

### Not implemented yet

These are the gaps a caller notices, listed so nobody goes looking for an
API that is not there:

- **Schema introspection.** `orm_metadata_reflect()`, `create_all()` and
  `drop_all()` are stubs that warn. Use the engine-level
  `orm_engine_create_all()` / `orm_engine_drop_all()`, which are real.
  There is no API to list tables, columns, indexes or foreign keys.
- **Result column types.** `OrmResult` exposes column names and count only;
  there is no declared-type metadata.
- **Server-side streaming on PostgreSQL and MySQL.**
  `ORM_QUERY_FLAGS_STREAMING` and `OrmRowStream` work on all three
  backends, but only SQLite genuinely streams; `PQsetSingleRowMode` and
  `mysql_use_result` are not wired up, so on those two the whole result
  is still materialized before the first batch.
- **Connection pooling.** Every `orm_engine_connect()` opens a new
  connection, and `orm_engine_execute()` opens and closes one per call.
  One connection means one worker thread, so parallelism means opening
  more connections yourself -- see `docs/async.md`.
- **Automatic schema diffs.** `OrmMigrator` runs explicit SQL or callback
  steps you write. There is no model-diff generator, and the DDL compiler
  still has no `ALTER TABLE`.
- **Text serialization of objects.** `OrmSerializable` means
  object-to-row, not object-to-JSON. Result sets *can* be written out --
  see `src/export/` and `docs/export.md` for the `OrmExporter` family
  (CSV, JSON) -- but there is no mapped-object-to-text path.

## Build matrix

The backend flags are independent, and all four combinations must build
warning-free -- `-Werror` is on, so an unused static helper left behind by
a disabled backend is a hard failure, not a nit:

```bash
make clean && make lib                                        # SQLite only (default)
make clean && make lib ENABLE_SQLITE=0 ENABLE_POSTGRES=1
make clean && make lib ENABLE_SQLITE=0 ENABLE_MYSQL=1
make clean && make lib ENABLE_POSTGRES=1 ENABLE_MYSQL=1
```

Two traps this catches, both of which have bitten:

1. **Guard helpers by the backend that uses them.** A `static` function
   used only inside `#ifdef ORM_ENABLE_POSTGRES` must itself be inside that
   guard, or the default SQLite-only build fails on
   `-Werror=unused-function`.
2. **Keep per-backend includes as siblings, never nested.** Nesting the
   PostgreSQL include inside the SQLite guard makes
   `ENABLE_SQLITE=0 ENABLE_POSTGRES=1` fail on a missing declaration.

Header dependencies are tracked: the Makefile generates `.d` files and
`-include`s them for the library objects, so editing a header rebuilds the
modules that include it. Changing a *build flag* is what `.d` files cannot
see -- `make clean` before switching backend flags, or objects compiled
under the previous set linger.

## Testing

Tests use GLib's GTest framework:
```bash
# Run all tests
make test

# Run with verbose output
make test VERBOSE=1

# Run specific test
LD_LIBRARY_PATH=build build/tests/test-schema
```

## Key Design Decisions

1. **GObject-based**: All major types are GObjects for introspection and bindings
2. **SQLAlchemy-inspired**: Core/ORM separation, Unit of Work pattern
3. **Type-safe**: Strong typing through GObject type system
4. **Dialect abstraction**: Database-specific behavior isolated in dialect classes
5. **gnu89 standard**: Maximum compatibility with older systems
