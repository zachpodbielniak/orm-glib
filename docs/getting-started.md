# Getting Started with orm-glib

This guide walks you through installing orm-glib and creating your first database-backed GObject application.

## Prerequisites

- GCC with gnu89 support
- GLib 2.0 development files
- SQLite3 development files (for SQLite support)
- pkg-config

### Fedora/RHEL

```bash
sudo dnf install gcc make glib2-devel sqlite-devel
```

### Debian/Ubuntu

```bash
sudo apt install gcc make libglib2.0-dev libsqlite3-dev
```

## Building orm-glib

```bash
git clone https://gitlab.com/your-repo/orm-glib.git
cd orm-glib

# Basic build (SQLite only)
make

# With PostgreSQL support
make ENABLE_POSTGRES=1

# With MySQL/MariaDB support
make ENABLE_MYSQL=1

# Run tests
make test

# Install (default: /usr/local)
make install PREFIX=/usr/local
```

## Your First ORM Application

### Step 1: Define a Model

Create a GObject that implements `OrmSerializable`:

```c
#include <orm.h>

/* Type declaration */
#define MY_TYPE_USER (my_user_get_type ())
G_DECLARE_FINAL_TYPE (MyUser, my_user, MY, USER, GObject)

struct _MyUser
{
    GObject parent_instance;
    gint64   id;
    gchar   *name;
    gchar   *email;
    gboolean active;
};

enum { PROP_0, PROP_ID, PROP_NAME, PROP_EMAIL, PROP_ACTIVE, N_PROPS };
static GParamSpec *props[N_PROPS] = { NULL, };

/* Forward declare interface init */
static void my_user_serializable_init (OrmSerializableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MyUser, my_user, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_SERIALIZABLE,
                                                my_user_serializable_init))
```

### Step 2: Implement GObject Methods

```c
static void
my_user_finalize (GObject *object)
{
    MyUser *self = MY_USER (object);
    g_free (self->name);
    g_free (self->email);
    G_OBJECT_CLASS (my_user_parent_class)->finalize (object);
}

static void
my_user_get_property (GObject *object, guint prop_id,
                      GValue *value, GParamSpec *pspec)
{
    MyUser *self = MY_USER (object);
    switch (prop_id)
    {
    case PROP_ID:     g_value_set_int64 (value, self->id); break;
    case PROP_NAME:   g_value_set_string (value, self->name); break;
    case PROP_EMAIL:  g_value_set_string (value, self->email); break;
    case PROP_ACTIVE: g_value_set_boolean (value, self->active); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
my_user_set_property (GObject *object, guint prop_id,
                      const GValue *value, GParamSpec *pspec)
{
    MyUser *self = MY_USER (object);
    switch (prop_id)
    {
    case PROP_ID:     self->id = g_value_get_int64 (value); break;
    case PROP_NAME:   g_free (self->name); self->name = g_value_dup_string (value); break;
    case PROP_EMAIL:  g_free (self->email); self->email = g_value_dup_string (value); break;
    case PROP_ACTIVE: self->active = g_value_get_boolean (value); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
my_user_class_init (MyUserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = my_user_finalize;
    object_class->get_property = my_user_get_property;
    object_class->set_property = my_user_set_property;

    props[PROP_ID] = g_param_spec_int64 ("id", NULL, NULL,
        G_MININT64, G_MAXINT64, 0, G_PARAM_READWRITE);
    props[PROP_NAME] = g_param_spec_string ("name", NULL, NULL,
        NULL, G_PARAM_READWRITE);
    props[PROP_EMAIL] = g_param_spec_string ("email", NULL, NULL,
        NULL, G_PARAM_READWRITE);
    props[PROP_ACTIVE] = g_param_spec_boolean ("active", NULL, NULL,
        TRUE, G_PARAM_READWRITE);

    g_object_class_install_properties (object_class, N_PROPS, props);
}

static void my_user_init (MyUser *self) { self->active = TRUE; }
```

### Step 3: Implement OrmSerializable

```c
static const gchar *
my_user_get_table_name (OrmSerializable *s)
{
    (void) s;
    return "users";
}

static const gchar *
my_user_get_primary_key (OrmSerializable *s)
{
    (void) s;
    return "id";
}

static void
my_user_serializable_init (OrmSerializableInterface *iface)
{
    iface->get_table_name = my_user_get_table_name;
    iface->get_primary_key = my_user_get_primary_key;
}
```

### Step 4: Create Engine and Session

```c
int
main (void)
{
    g_autoptr(GError) error = NULL;

    /* Create engine - parses URL and selects dialect */
    g_autoptr(OrmEngine) engine = orm_engine_new ("sqlite:///myapp.db", &error);
    if (engine == NULL)
    {
        g_printerr ("Engine error: %s\n", error->message);
        return 1;
    }

    /* Get a connection */
    g_autoptr(OrmConnection) conn = orm_engine_connect (engine, &error);
    if (conn == NULL)
    {
        g_printerr ("Connection error: %s\n", error->message);
        return 1;
    }

    /* Create mapper from our model */
    g_autoptr(OrmMapper) mapper = orm_mapper_new_from_serializable (MY_TYPE_USER);

    /* Generate and execute CREATE TABLE */
    {
        g_autoptr(OrmTable) table = orm_mapper_to_table (mapper);
        OrmDialect *dialect = orm_engine_get_dialect (engine);
        g_autoptr(OrmDdlCompiler) ddl = ORM_DDL_COMPILER (orm_dialect_get_ddl_compiler (dialect));
        g_autofree gchar *sql = orm_ddl_compiler_compile_create_table (ddl, table, TRUE);
        orm_connection_execute (conn, sql, &error);
    }

    /* Create session */
    g_autoptr(OrmSession) session = orm_session_new_with_connection (conn);
    orm_session_register_mapper (session, mapper);

    /* Now you can use the session for CRUD operations */
    return 0;
}
```

### Step 5: CRUD Operations

#### Create

```c
g_autoptr(MyUser) user = g_object_new (MY_TYPE_USER,
    "name", "Alice",
    "email", "alice@example.com",
    NULL);

orm_session_add (session, G_OBJECT (user));
orm_session_commit (session, &error);

/* After commit, user->id is populated with the inserted row ID */
g_print ("Created user with id=%ld\n", (long) user->id);
```

#### Read

```c
/* Query all users */
g_autoptr(OrmQuery) query = orm_session_query (session, MY_TYPE_USER);
g_autoptr(GList) users = orm_query_all (query, &error);

GList *l;
for (l = users; l != NULL; l = l->next)
{
    MyUser *u = MY_USER (l->data);
    g_print ("User: %s <%s>\n", u->name, u->email);
}
g_list_free_full (users, g_object_unref);

/* Query with filter */
g_autoptr(OrmQuery) active_query = orm_session_query (session, MY_TYPE_USER);
g_autoptr(OrmValue) active_val = orm_value_new_boolean (TRUE);
orm_query_filter_by (active_query, "active", active_val);
g_autoptr(GList) active_users = orm_query_all (active_query, &error);
```

#### Update

```c
/* Modify properties and commit */
g_object_set (user, "email", "alice.new@example.com", NULL);
orm_session_add (session, G_OBJECT (user));
orm_session_commit (session, &error);
```

#### Delete

```c
orm_session_delete (session, G_OBJECT (user));
orm_session_commit (session, &error);
```

## Compiling Your Application

```bash
gcc -std=gnu89 $(pkg-config --cflags orm-glib-1.0) \
    myapp.c -o myapp \
    $(pkg-config --libs orm-glib-1.0)
```

Or with the library built locally:

```bash
gcc -std=gnu89 -I/path/to/orm-glib/src \
    myapp.c -o myapp \
    -L/path/to/orm-glib/build -lorm-glib-1.0 \
    $(pkg-config --cflags --libs glib-2.0 gobject-2.0 sqlite3)
```

## Connection URLs

orm-glib uses URL-style connection strings:

| Database   | Format                              | Example                          |
|------------|-------------------------------------|----------------------------------|
| SQLite     | `sqlite:///path/to/file.db`         | `sqlite:///var/data/app.db`      |
| SQLite     | `sqlite:///:memory:`                | In-memory database               |
| PostgreSQL | `postgresql://user:pass@host/db`    | `postgresql://app:secret@localhost/mydb` |
| MySQL      | `mysql://user:pass@host/db`         | `mysql://root@localhost/mydb`    |

## Next Steps

- [Architecture](architecture.md) - Understand the library design
- [OrmSerializable](serializable.md) - Advanced serialization options
- [Dialects](dialects.md) - Database-specific information
