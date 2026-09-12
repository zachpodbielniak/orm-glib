# Migrations

`OrmMigrator` evolves a live schema only when the application explicitly calls
`orm_migrator_up()` or `orm_migrator_down()`. Connecting and constructing a
migrator do not execute migrations. Keep the complete migration list in source
control, ordered by strictly increasing positive `gint64` versions (gaps are OK).
The migrator copies the boxed `OrmMigration` values and retains the connection.

## Adding a column to a live table

This example saves a customer, then adds a nullable email column. On later
deployments, call `orm_migrator_up(migrator, 0, &error)` with both definitions;
version 1 is validated and skipped, preserving its rows.

```c
g_autoptr(GError) error = NULL;
g_autoptr(OrmEngine) engine = orm_engine_new ("sqlite:///customers.db", &error);
g_autoptr(OrmConnection) connection = NULL;
g_autoptr(OrmMigration) initial = orm_migration_new (1, "customers",
    "CREATE TABLE customers (id INTEGER PRIMARY KEY, name VARCHAR(200))",
    "DROP TABLE customers");
g_autoptr(OrmMigration) email = orm_migration_new (2, "customer email",
    "ALTER TABLE customers ADD COLUMN email VARCHAR(255)",
    "ALTER TABLE customers DROP COLUMN email");
OrmMigration *migrations[] = { initial, email };
g_autoptr(OrmMigrator) migrator = NULL;

if (engine == NULL)
    goto failed;
connection = orm_engine_connect (engine, &error);
if (connection == NULL)
    goto failed;
migrator = orm_migrator_new (connection, migrations, 2, &error);
if (migrator == NULL || !orm_migrator_up (migrator, 1, &error))
    goto failed;
/* First-release application activity; run once in this demonstration. */
if (!orm_connection_execute (connection,
        "INSERT INTO customers (id, name) VALUES (1, 'Ada')", &error))
    goto failed;
/* Next release: Ada remains, with email initially NULL. */
if (!orm_migrator_up (migrator, 0, &error))
    goto failed;
/* Optional rollback of version 2 only (discards email values):
 * orm_migrator_down (migrator, 1, &error); */
return;
failed:
g_printerr ("Migration failed: %s\n", error->message);
```

The example's `DROP COLUMN` requires SQLite 3.35 or later. `up` target zero means
latest; `down` target zero removes every applied migration. Other targets must
exist in the supplied list. Each successful step remains committed if a later
step fails. A missing down operation is an error when that step is reached.
`orm_migrator_status()` returns two caller-owned `GArray`s of ascending `gint64`
versions, applied and pending. Status initializes bookkeeping but executes no
migration. Unknown applied versions, gaps in history, changed names, and changed
SHA-256 checksums are errors, including when the changed version exceeds the
requested target. `orm_migration_get_checksum()` returns that hash. Never
edit an applied migration; append another one.

## Callbacks, transactions, and locking

SQL migrations contain one statement per direction. For multiple statements or
portable DDL, use `orm_migration_new_callback()`. Its callback receives the
connection, dialect, and `GError **`; obtain the compiler with
`orm_dialect_get_ddl_compiler()`, compile an `OrmTable` with
`orm_ddl_compiler_compile_create_table()`, then execute and free the returned SQL.
The required `up_text` is the stable source/description hashed for callbacks;
update it whenever their implementation changes. Function pointers cannot be
checksummed portably. A NULL down callback marks an irreversible migration.

Use an idle connection exclusively for the entire call. Callbacks must return
FALSE on failure, must not commit/rollback, and must not alter migration tables
or locking state. `schema_migrations` records version, name, checksum, and the
server's `CURRENT_TIMESTAMP` as `applied_at` after a successful up operation.
PostgreSQL and SQLite commit each migration and its record together; failures
roll both back. PostgreSQL holds a session advisory lock across the call.
SQLite has no `LOCK TABLE` statement: `BEGIN IMMEDIATE` takes its database-wide
write reservation before checking history, with a temporary 30-second busy
timeout, restored afterwards. History is rechecked after every lock acquisition.

MySQL/MariaDB DDL implicitly commits and **cannot be rolled back**. A failed
migration is not recorded, but earlier statements may have changed the schema;
repair that partial state before retrying. A separate connection holds
`LOCK TABLES schema_migrations_lock WRITE` throughout the call, so DDL commits on
the working connection cannot release it. This requires one additional database
connection and CREATE/LOCK TABLES privileges. Keep the same database/schema and
PostgreSQL search path across runners. All schema writers must cooperate with
these locks; migrations do not lock out arbitrary application DDL.

The calling thread emits `migration-applied(OrmMigration, gboolean down)` after
each committed step and `migration-failed(OrmMigration, GError)` after a step
fails and rollback is attempted. Validation and lock errors are returned through
`GError`; they do not identify a step and emit no failure signal.

For live tests, build with `ENABLE_POSTGRES=1 ENABLE_MYSQL=1` and run
`LD_LIBRARY_PATH=build/release ORM_TEST_URL=... build/release/tests/test-migration`.
Use a disposable PostgreSQL/MySQL database: fixtures drop bookkeeping and test
tables. The default SQLite run uses a temporary file; concurrency uses two processes.
