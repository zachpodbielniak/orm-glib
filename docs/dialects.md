# Database Dialects

orm-glib supports multiple database backends through its dialect system. Each dialect handles database-specific SQL generation and type mapping.

## Supported Databases

| Database | Dialect Class | Build Flag | Required Library |
|----------|---------------|------------|------------------|
| SQLite | OrmSqliteDialect | ENABLE_SQLITE=1 (default) | sqlite3 |
| PostgreSQL | OrmPostgresDialect | ENABLE_POSTGRES=1 | libpq |
| MySQL/MariaDB | OrmMysqlDialect | ENABLE_MYSQL=1 | mysqlclient |

## Building with Specific Dialects

```bash
# SQLite only (default)
make

# SQLite + PostgreSQL
make ENABLE_POSTGRES=1

# SQLite + MySQL
make ENABLE_MYSQL=1

# All dialects
make ENABLE_POSTGRES=1 ENABLE_MYSQL=1
```

## Connection URLs

### SQLite

```
sqlite:///path/to/database.db
sqlite:///:memory:
```

**Examples:**
```c
/* Absolute path */
orm_engine_new ("sqlite:///var/data/app.db", &error);

/* Relative path */
orm_engine_new ("sqlite:///./local.db", &error);

/* In-memory database */
orm_engine_new ("sqlite:///:memory:", &error);
```

### PostgreSQL

```
postgresql://[user[:password]@][host][:port]/database
postgres://[user[:password]@][host][:port]/database
```

**Examples:**
```c
/* Full connection string */
orm_engine_new ("postgresql://myuser:secret@localhost:5432/mydb", &error);

/* Using defaults (localhost:5432) */
orm_engine_new ("postgresql://myuser@/mydb", &error);

/* Unix socket connection */
orm_engine_new ("postgresql://myuser@/mydb?host=/var/run/postgresql", &error);
```

### MySQL/MariaDB

```
mysql://[user[:password]@][host][:port]/database
```

**Examples:**
```c
/* Full connection string */
orm_engine_new ("mysql://root:password@localhost:3306/mydb", &error);

/* Using defaults */
orm_engine_new ("mysql://root@localhost/mydb", &error);
```

## Type Mapping

### SQLite

| ORM Type | SQLite Type | Notes |
|----------|-------------|-------|
| OrmInteger | INTEGER | 64-bit signed integer |
| OrmString | TEXT | UTF-8 text |
| OrmText | TEXT | Same as OrmString |
| OrmBoolean | INTEGER | 0 = FALSE, 1 = TRUE |
| OrmFloat | REAL | 64-bit IEEE floating point |
| OrmDateTime | TEXT | ISO8601 format |
| OrmBlob | BLOB | Binary data |

**SQLite-specific notes:**
- SQLite uses dynamic typing; types are affinities
- Boolean values stored as 0/1 integers
- DateTime stored as ISO8601 text strings
- Foreign keys must be enabled per-connection:
  ```c
  orm_connection_execute (conn, "PRAGMA foreign_keys = ON", &error);
  ```

### PostgreSQL

| ORM Type | PostgreSQL Type | Notes |
|----------|-----------------|-------|
| OrmInteger | BIGINT | 64-bit signed integer |
| OrmString | VARCHAR(255) | Default length 255 |
| OrmText | TEXT | Unlimited text |
| OrmBoolean | BOOLEAN | Native boolean |
| OrmFloat | DOUBLE PRECISION | 64-bit floating point |
| OrmDateTime | TIMESTAMP | With timezone support |
| OrmBlob | BYTEA | Binary data |

**PostgreSQL-specific notes:**
- Native boolean type supported
- RETURNING clause supported for INSERT
- Sequences used for auto-increment (SERIAL)
- Full transaction support with savepoints

### MySQL/MariaDB

| ORM Type | MySQL Type | Notes |
|----------|------------|-------|
| OrmInteger | BIGINT | 64-bit signed integer |
| OrmString | VARCHAR(255) | Default length 255 |
| OrmText | TEXT | Up to 65,535 bytes |
| OrmBoolean | TINYINT(1) | 0 = FALSE, 1 = TRUE |
| OrmFloat | DOUBLE | 64-bit floating point |
| OrmDateTime | DATETIME | No timezone |
| OrmBlob | BLOB | Up to 65,535 bytes |

**MySQL-specific notes:**
- Boolean stored as TINYINT(1)
- AUTO_INCREMENT for primary keys
- InnoDB engine required for foreign keys
- LAST_INSERT_ID() for retrieving inserted ID

## Dialect-Specific Features

### RETURNING Clause

PostgreSQL supports RETURNING to get inserted/updated values:

```c
if (orm_dialect_supports_returning (dialect))
{
    /* INSERT INTO users (name) VALUES ('Alice') RETURNING id */
}
else
{
    /* INSERT then SELECT last_insert_rowid() / LAST_INSERT_ID() */
}
```

### Auto-increment Primary Keys

| Database | Syntax |
|----------|--------|
| SQLite | `INTEGER PRIMARY KEY` (implicit ROWID) |
| PostgreSQL | `SERIAL` or `BIGSERIAL` |
| MySQL | `BIGINT AUTO_INCREMENT` |

### DDL Differences

**CREATE TABLE:**

```sql
/* SQLite */
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    active INTEGER DEFAULT 1
);

/* PostgreSQL */
CREATE TABLE IF NOT EXISTS users (
    id BIGSERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    active BOOLEAN DEFAULT TRUE
);

/* MySQL */
CREATE TABLE IF NOT EXISTS users (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    active TINYINT(1) DEFAULT 1
) ENGINE=InnoDB;
```

## Foreign Key Actions

All dialects support foreign key constraint actions:

```c
typedef enum {
    ORM_FK_NO_ACTION,   /* Database default */
    ORM_FK_RESTRICT,    /* Prevent deletion */
    ORM_FK_CASCADE,     /* Delete/update related rows */
    ORM_FK_SET_NULL,    /* Set FK column to NULL */
    ORM_FK_SET_DEFAULT  /* Set FK column to default */
} OrmForeignKeyAction;
```

```c
OrmForeignKey *fk = orm_foreign_key_new ("fk_posts_author", "authors");
orm_foreign_key_add_column (fk, "author_id", "id");
orm_foreign_key_set_on_delete (fk, ORM_FK_CASCADE);
orm_foreign_key_set_on_update (fk, ORM_FK_SET_NULL);
```

**Note:** SQLite requires `PRAGMA foreign_keys = ON` to enforce foreign keys.

## Custom Dialects

To implement a custom dialect:

1. Implement `OrmDialect` interface
2. Implement `OrmTypeCompiler` for type mapping
3. Implement `OrmDdlCompiler` for DDL generation

```c
/* Example: Custom dialect header */
#define MY_TYPE_CUSTOM_DIALECT (my_custom_dialect_get_type ())
G_DECLARE_FINAL_TYPE (MyCustomDialect, my_custom_dialect, MY, CUSTOM_DIALECT, GObject)

/* Implement OrmDialect interface */
static void
my_custom_dialect_iface_init (OrmDialectInterface *iface)
{
    iface->get_name = my_custom_dialect_get_name;
    iface->get_driver = my_custom_dialect_get_driver;
    iface->supports_returning = my_custom_dialect_supports_returning;
    iface->get_type_compiler = my_custom_dialect_get_type_compiler;
    iface->get_ddl_compiler = my_custom_dialect_get_ddl_compiler;
}
```

## Testing with Different Databases

### SQLite (In-Memory)

Best for unit tests - fast and requires no setup:

```c
engine = orm_engine_new ("sqlite:///:memory:", &error);
```

### PostgreSQL (Docker)

```bash
docker run --name test-postgres -e POSTGRES_PASSWORD=test -p 5432:5432 -d postgres:15

# In tests
engine = orm_engine_new ("postgresql://postgres:test@localhost/postgres", &error);
```

### MySQL (Docker)

```bash
docker run --name test-mysql -e MYSQL_ROOT_PASSWORD=test -p 3306:3306 -d mysql:8

# In tests
engine = orm_engine_new ("mysql://root:test@localhost/mysql", &error);
```

## Performance Considerations

### SQLite
- Great for single-user, embedded applications
- Use WAL mode for better concurrency: `PRAGMA journal_mode=WAL`
- Consider connection pooling for web applications

### PostgreSQL
- Excellent for concurrent access
- Use prepared statements for repeated queries
- Connection pooling recommended (external: PgBouncer)

### MySQL
- Good balance of features and performance
- Use InnoDB for transactions and foreign keys
- Connection pooling recommended
