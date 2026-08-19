# Drivers

A **driver** is a database backend: everything orm-glib needs in order to
talk to one kind of database. Drivers are looked up by URL scheme, so
`postgresql://…` reaches the PostgreSQL driver without anything in the
engine, connection or result layers knowing PostgreSQL exists.

## Dialects and drivers are different things

This trips people up, because both are "the PostgreSQL bit".

| | Dialect | Driver |
|---|---|---|
| Answers | *How is this SQL spelled?* | *How do I talk to the server?* |
| Deals in | strings | sockets, statements, result sets |
| Interfaces | `OrmDialect`, `OrmTypeCompiler`, `OrmDdlCompiler` | `OrmDriver`, `OrmDriverConnection`, `OrmDriverResult` |
| Stateless? | yes | the driver is; connections are not |

A dialect quotes an identifier and knows whether the backend supports
`RETURNING`. A driver opens the connection, binds parameters, and hands
back rows. They are separate because SQL generation is testable without a
database and connection handling is not.

## The three classes

```
OrmDriver                 stateless factory, one instance per backend
 └── open()            →  OrmDriverConnection    one live connection
                           └── query()        →  OrmDriverResult   one cursor
```

**`OrmDriver`** — `get_name`, `get_schemes`, `get_dialect_type`,
`create_dialect`, `open`. Registered once and shared, so it must hold no
per-database state.

**`OrmDriverConnection`** — `close`, `execute`, `query`,
`get_last_insert_id`, `get_changes`, `set_isolation_level`, `interrupt`.

**`OrmDriverResult`** — `get_column_count`, `get_column_name`,
`get_column_value_type`, `get_column_type_name`, `fetch_row`, `close`.

All three vtables carry `gpointer _reserved[8]`, so methods can be added
without breaking an out-of-tree driver's ABI.

### `fetch_row` returns NULL twice over

```c
row = orm_driver_result_fetch_row (result, &error);
if (row == NULL)
{
    if (error != NULL)
        /* the fetch failed */;
    else
        /* the results are exhausted */;
}
```

Callers **must** check `error` rather than treating every `NULL` as
end-of-results. Conflating the two makes a broken query look like an
empty table, which is the kind of bug that gets noticed only when the
data is already wrong.

### `interrupt` is the one method with a threading contract

It may be called from a thread other than the one running the query,
because that is the only moment at which cancelling is any use. Backends
whose cancel primitive is not safe that way must leave it unimplemented;
the base class default fails with `ORM_ERROR_NOT_SUPPORTED` and callers
fall back to abandoning the result.

| Backend | Primitive | Notes |
|---------|-----------|-------|
| SQLite | `sqlite3_interrupt()` | documented safe from another thread |
| PostgreSQL | `PQcancel()` | uses a `PGcancel` captured at connect |
| MySQL | — | no safe same-connection cancel; needs a second connection issuing `KILL QUERY` |

## Streaming

`ORM_QUERY_FLAGS_STREAMING` asks for rows incrementally. It is a request,
not a guarantee:

- **SQLite** always streams — a result *is* a statement being stepped.
- **PostgreSQL** and **MySQL** materialize the whole result client-side.

A streaming result holds its connection busy until drained or closed, so
the flag is opt-in rather than the default. If you want page semantics
instead, use SQL `LIMIT`/`OFFSET` on a normal query.

## Adding a backend

Out of tree, with no changes to orm-glib:

```c
/* 1. Subclass the three abstract classes. */
G_DECLARE_FINAL_TYPE (MyDriver, my_driver, MY, DRIVER, OrmDriver)

static const gchar * const *
my_driver_get_schemes (OrmDriver *driver)
{
    static const gchar * const schemes[] = { "mydb", NULL };
    return schemes;
}

static void
my_driver_class_init (MyDriverClass *klass)
{
    OrmDriverClass *driver_class = ORM_DRIVER_CLASS (klass);

    driver_class->get_name = my_driver_get_name;
    driver_class->get_schemes = my_driver_get_schemes;
    driver_class->get_dialect_type = my_driver_get_dialect_type;
    driver_class->create_dialect = my_driver_create_dialect;
    driver_class->open = my_driver_open;
}

/* 2. Register it. */
g_autoptr(MyDriver) driver = g_object_new (MY_TYPE_DRIVER, NULL);

if (!orm_driver_registry_register (orm_driver_registry_get_default (),
                                   ORM_DRIVER (driver), &error))
    g_warning ("%s", error->message);

/* 3. mydb:// URLs now work. */
engine = orm_engine_new ("mydb://host/database", &error);
```

`tests/test-driver.c` builds exactly this — a complete fake backend
driven through the registry — so if the extension point ever closes, that
test stops compiling.

### The limitation to know about

SQL *generation* still switches on the closed `OrmDialectType` enum in
`src/sql/` and `src/types/`. An out-of-tree driver therefore gets
pluggable I/O and introspection, but must borrow one of the three in-tree
dialects for SQL flavour via `create_dialect`. Routing those switches
through `OrmDialect` virtual methods would remove the restriction and is
a candidate for a later release.

## The registry

```c
OrmDriverRegistry *registry = orm_driver_registry_get_default ();

orm_driver_registry_lookup (registry, "postgresql");   /* by scheme */
orm_driver_registry_lookup_dialect (registry, ORM_DIALECT_MYSQL);
orm_driver_registry_list (registry);          /* one entry per driver */
orm_driver_registry_list_schemes (registry);  /* every scheme, sorted */
```

The default registry is built on first use and populated with whichever
backends were compiled in. Registration is **all-or-nothing**: a driver
claiming two schemes where one is taken registers neither, because a
driver reachable by some of its names and not others is a worse state
than a clean refusal.

It is a plain `GHashTable` keyed by scheme, not a `GIOExtensionPoint`.
Extension points are built around `GIOModule` dynamic loading with
priority-ordered "pick the best implementation" semantics; what is needed
here is exact keyed lookup of statically linked backends, where two
candidates for one scheme is a bug rather than a ranking problem.

## Built-in drivers

| Driver | Schemes | Build flag | Library |
|--------|---------|------------|---------|
| sqlite | `sqlite`, `sqlite3` | `ENABLE_SQLITE=1` (default) | sqlite3 |
| postgres | `postgresql`, `postgres` | `ENABLE_POSTGRES=1` | libpq |
| mysql | `mysql`, `mariadb` | `ENABLE_MYSQL=1` | mysqlclient / mariadb-connector-c |

A backend that was not compiled in is simply absent from the registry, so
its URLs fail at parse time with a message naming the build flag —
rather than succeeding into a half-built connection.
