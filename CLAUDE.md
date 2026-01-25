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
├── dialect/        # Database-specific implementations
├── sql/            # SQL expression language
└── orm/            # ORM layer (OrmSession, OrmMapper)
```

## Implementation Status

### Phase 1 (Complete)
- [x] Core types and utilities
- [x] SQL type system
- [x] Schema definitions
- [x] Build system

### Phase 2 (Pending)
- [ ] Dialect interface
- [ ] SQLite dialect
- [ ] Basic tests

### Phase 3 (Pending)
- [ ] SQL expression language
- [ ] SELECT/INSERT/UPDATE/DELETE builders

### Phase 4 (Pending)
- [ ] Engine layer
- [ ] Connection management
- [ ] Transaction support

### Phase 5 (Pending)
- [ ] OrmSerializable interface
- [ ] Mapper and session
- [ ] ORM queries

### Phase 6 (Pending)
- [ ] PostgreSQL dialect
- [ ] MySQL dialect

### Phase 7 (Pending)
- [ ] GObject introspection
- [ ] Documentation
- [ ] Examples

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
