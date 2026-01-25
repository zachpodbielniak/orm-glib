# OrmSerializable Interface

The `OrmSerializable` interface is the core mechanism for making GObjects persistable to a database. It defines how objects are serialized to database rows and deserialized back to GObjects.

## Basic Implementation

At minimum, implement `get_table_name` and `get_primary_key`:

```c
#include <orm.h>

#define MY_TYPE_ENTITY (my_entity_get_type ())
G_DECLARE_FINAL_TYPE (MyEntity, my_entity, MY, ENTITY, GObject)

static void my_entity_serializable_init (OrmSerializableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MyEntity, my_entity, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_SERIALIZABLE,
                                                my_entity_serializable_init))

static const gchar *
my_entity_get_table_name (OrmSerializable *self)
{
    (void) self;
    return "entities";
}

static const gchar *
my_entity_get_primary_key (OrmSerializable *self)
{
    (void) self;
    return "id";
}

static void
my_entity_serializable_init (OrmSerializableInterface *iface)
{
    iface->get_table_name = my_entity_get_table_name;
    iface->get_primary_key = my_entity_get_primary_key;
}
```

## Interface Methods

### Required Methods

#### get_table_name

Returns the database table name for this type.

```c
const gchar * (*get_table_name) (OrmSerializable *self);
```

**Example:**
```c
static const gchar *
my_user_get_table_name (OrmSerializable *self)
{
    (void) self;
    return "users";
}
```

#### get_primary_key

Returns the property name that serves as the primary key.

```c
const gchar * (*get_primary_key) (OrmSerializable *self);
```

**Example:**
```c
static const gchar *
my_user_get_primary_key (OrmSerializable *self)
{
    (void) self;
    return "id";
}
```

### Optional Methods

#### serialize_property

Custom serialization from GValue to OrmValue. Override to customize how properties are stored in the database.

```c
OrmValue * (*serialize_property) (OrmSerializable *self,
                                  const gchar     *property_name,
                                  const GValue    *value,
                                  GParamSpec      *pspec);
```

**Default behavior:** Automatic conversion based on GValue type.

**Example - Custom enum serialization:**
```c
static OrmValue *
my_entity_serialize_property (OrmSerializable *self,
                              const gchar     *property_name,
                              const GValue    *value,
                              GParamSpec      *pspec)
{
    (void) self;
    (void) pspec;

    if (g_strcmp0 (property_name, "status") == 0)
    {
        /* Store enum as string instead of integer */
        MyStatus status = g_value_get_enum (value);
        const gchar *str = my_status_to_string (status);
        return orm_value_new_string (str);
    }

    /* Fall back to default serialization */
    return NULL;
}
```

#### deserialize_property

Custom deserialization from OrmValue to GValue. Override to customize how database values are converted to properties.

```c
gboolean (*deserialize_property) (OrmSerializable *self,
                                  const gchar     *property_name,
                                  GValue          *value,
                                  GParamSpec      *pspec,
                                  OrmValue        *db_value);
```

**Return value:** TRUE if handled, FALSE to use default deserialization.

**Example - Custom enum deserialization:**
```c
static gboolean
my_entity_deserialize_property (OrmSerializable *self,
                                const gchar     *property_name,
                                GValue          *value,
                                GParamSpec      *pspec,
                                OrmValue        *db_value)
{
    (void) self;
    (void) pspec;

    if (g_strcmp0 (property_name, "status") == 0)
    {
        const gchar *str = orm_value_get_string (db_value);
        MyStatus status = my_status_from_string (str);
        g_value_set_enum (value, status);
        return TRUE;
    }

    return FALSE;  /* Use default */
}
```

#### find_property

Custom property lookup. Override to map database column names to different property names.

```c
GParamSpec * (*find_property) (OrmSerializable *self,
                               const gchar     *name);
```

**Default behavior:** Uses `g_object_class_find_property`.

**Example - Column name mapping:**
```c
static GParamSpec *
my_entity_find_property (OrmSerializable *self,
                         const gchar     *name)
{
    GObjectClass *klass = G_OBJECT_GET_CLASS (self);

    /* Map database column "user_name" to property "name" */
    if (g_strcmp0 (name, "user_name") == 0)
        return g_object_class_find_property (klass, "name");

    return g_object_class_find_property (klass, name);
}
```

#### list_properties

Custom property enumeration. Override to control which properties are persisted.

```c
GParamSpec ** (*list_properties) (OrmSerializable *self,
                                  guint           *n_pspecs);
```

**Default behavior:** Uses `g_object_class_list_properties`.

**Example - Exclude transient properties:**
```c
static GParamSpec **
my_entity_list_properties (OrmSerializable *self,
                           guint           *n_pspecs)
{
    GObjectClass *klass = G_OBJECT_GET_CLASS (self);
    guint n_props;
    GParamSpec **all_props = g_object_class_list_properties (klass, &n_props);

    /* Filter out transient properties */
    GPtrArray *filtered = g_ptr_array_new ();
    guint i;
    for (i = 0; i < n_props; i++)
    {
        /* Skip properties starting with underscore (transient) */
        if (all_props[i]->name[0] != '_')
            g_ptr_array_add (filtered, all_props[i]);
    }

    *n_pspecs = filtered->len;
    g_free (all_props);
    return (GParamSpec **) g_ptr_array_free (filtered, FALSE);
}
```

## Property-to-Column Mapping

By default, GObject property names map directly to database column names with hyphens converted to underscores:

| Property Name | Column Name |
|---------------|-------------|
| `id` | `id` |
| `name` | `name` |
| `created-at` | `created_at` |
| `user-email` | `user_email` |

### Customizing Column Names

Override `find_property` to map column names to different property names.

## Type Mapping

### Automatic Type Detection

The ORM automatically maps GLib types to SQL types:

| GType | SQL Type |
|-------|----------|
| G_TYPE_INT | INTEGER |
| G_TYPE_INT64 | BIGINT/INTEGER |
| G_TYPE_UINT | INTEGER |
| G_TYPE_UINT64 | BIGINT/INTEGER |
| G_TYPE_BOOLEAN | BOOLEAN/INTEGER |
| G_TYPE_STRING | VARCHAR/TEXT |
| G_TYPE_FLOAT | REAL/FLOAT |
| G_TYPE_DOUBLE | REAL/DOUBLE |
| G_TYPE_DATE_TIME | TIMESTAMP/TEXT |
| G_TYPE_BYTES | BLOB/BYTEA |

### Enum Types

Enums are serialized as integers by default. Override `serialize_property` and `deserialize_property` to store as strings.

### Custom Types

For custom types, implement both serialization methods:

```c
static OrmValue *
my_entity_serialize_property (OrmSerializable *self,
                              const gchar     *property_name,
                              const GValue    *value,
                              GParamSpec      *pspec)
{
    if (g_strcmp0 (property_name, "custom-data") == 0)
    {
        MyCustomData *data = g_value_get_boxed (value);
        /* Serialize to JSON string */
        gchar *json = my_custom_data_to_json (data);
        OrmValue *result = orm_value_new_string (json);
        g_free (json);
        return result;
    }
    return NULL;
}

static gboolean
my_entity_deserialize_property (OrmSerializable *self,
                                const gchar     *property_name,
                                GValue          *value,
                                GParamSpec      *pspec,
                                OrmValue        *db_value)
{
    if (g_strcmp0 (property_name, "custom-data") == 0)
    {
        const gchar *json = orm_value_get_string (db_value);
        MyCustomData *data = my_custom_data_from_json (json);
        g_value_take_boxed (value, data);
        return TRUE;
    }
    return FALSE;
}
```

## Working with the Mapper

### Automatic Mapper Creation

```c
OrmMapper *mapper = orm_mapper_new_from_serializable (MY_TYPE_ENTITY);
```

This creates a mapper by:
1. Instantiating a temporary object
2. Calling `get_table_name` and `get_primary_key`
3. Iterating `list_properties` to discover columns
4. Determining SQL types from property types

### Manual Mapper Configuration

For more control, configure the mapper after creation:

```c
OrmMapper *mapper = orm_mapper_new_from_serializable (MY_TYPE_ENTITY);

/* Mark a column as non-nullable */
OrmProperty *prop = orm_mapper_get_property (mapper, "email");
orm_property_set_nullable (prop, FALSE);

/* Add a unique constraint */
orm_property_set_unique (prop, TRUE);
```

### Generating Tables

```c
OrmTable *table = orm_mapper_to_table (mapper);
OrmDialect *dialect = orm_engine_get_dialect (engine);
OrmDdlCompiler *ddl = orm_dialect_get_ddl_compiler (dialect);

gchar *sql = orm_ddl_compiler_compile_create_table (ddl, table, TRUE);
orm_connection_execute (conn, sql, &error);
```

## Complete Example

```c
#include <orm.h>

/* Product model with custom serialization */

#define APP_TYPE_PRODUCT (app_product_get_type ())
G_DECLARE_FINAL_TYPE (AppProduct, app_product, APP, PRODUCT, GObject)

typedef enum {
    APP_PRODUCT_STATUS_DRAFT,
    APP_PRODUCT_STATUS_ACTIVE,
    APP_PRODUCT_STATUS_ARCHIVED
} AppProductStatus;

struct _AppProduct
{
    GObject parent_instance;

    gint64           id;
    gchar           *name;
    gchar           *description;
    gdouble          price;
    AppProductStatus status;
    GDateTime       *created_at;

    /* Transient (not persisted) */
    gboolean         _is_dirty;
};

enum {
    PROP_0, PROP_ID, PROP_NAME, PROP_DESCRIPTION,
    PROP_PRICE, PROP_STATUS, PROP_CREATED_AT, N_PROPS
};
static GParamSpec *props[N_PROPS];

static void app_product_serializable_init (OrmSerializableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (AppProduct, app_product, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_SERIALIZABLE,
                                                app_product_serializable_init))

/* ... standard GObject methods (finalize, get/set_property, class_init) ... */

/* OrmSerializable implementation */

static const gchar *
app_product_get_table_name (OrmSerializable *self)
{
    (void) self;
    return "products";
}

static const gchar *
app_product_get_primary_key (OrmSerializable *self)
{
    (void) self;
    return "id";
}

static const gchar *
status_to_string (AppProductStatus status)
{
    switch (status)
    {
    case APP_PRODUCT_STATUS_DRAFT:    return "draft";
    case APP_PRODUCT_STATUS_ACTIVE:   return "active";
    case APP_PRODUCT_STATUS_ARCHIVED: return "archived";
    default: return "draft";
    }
}

static AppProductStatus
status_from_string (const gchar *str)
{
    if (g_strcmp0 (str, "active") == 0)   return APP_PRODUCT_STATUS_ACTIVE;
    if (g_strcmp0 (str, "archived") == 0) return APP_PRODUCT_STATUS_ARCHIVED;
    return APP_PRODUCT_STATUS_DRAFT;
}

static OrmValue *
app_product_serialize_property (OrmSerializable *self,
                                const gchar     *property_name,
                                const GValue    *value,
                                GParamSpec      *pspec)
{
    (void) self;
    (void) pspec;

    /* Serialize enum as string */
    if (g_strcmp0 (property_name, "status") == 0)
    {
        AppProductStatus status = g_value_get_enum (value);
        return orm_value_new_string (status_to_string (status));
    }

    return NULL;  /* Use default serialization */
}

static gboolean
app_product_deserialize_property (OrmSerializable *self,
                                  const gchar     *property_name,
                                  GValue          *value,
                                  GParamSpec      *pspec,
                                  OrmValue        *db_value)
{
    (void) self;
    (void) pspec;

    /* Deserialize string to enum */
    if (g_strcmp0 (property_name, "status") == 0)
    {
        const gchar *str = orm_value_get_string (db_value);
        g_value_set_enum (value, status_from_string (str));
        return TRUE;
    }

    return FALSE;  /* Use default deserialization */
}

static GParamSpec **
app_product_list_properties (OrmSerializable *self,
                             guint           *n_pspecs)
{
    /* Exclude transient _is_dirty property */
    static const gchar *persist_props[] = {
        "id", "name", "description", "price", "status", "created-at"
    };
    guint n = G_N_ELEMENTS (persist_props);
    GParamSpec **result = g_new0 (GParamSpec *, n + 1);
    GObjectClass *klass = G_OBJECT_GET_CLASS (self);
    guint i;

    for (i = 0; i < n; i++)
        result[i] = g_object_class_find_property (klass, persist_props[i]);

    *n_pspecs = n;
    return result;
}

static void
app_product_serializable_init (OrmSerializableInterface *iface)
{
    iface->get_table_name = app_product_get_table_name;
    iface->get_primary_key = app_product_get_primary_key;
    iface->serialize_property = app_product_serialize_property;
    iface->deserialize_property = app_product_deserialize_property;
    iface->list_properties = app_product_list_properties;
}
```

## Best Practices

1. **Always implement `get_table_name` and `get_primary_key`** - These are required.

2. **Use `gint64` for primary keys** - Provides consistent behavior across databases.

3. **Name properties to match columns** - Simplifies mapping; use hyphens in properties (they map to underscores).

4. **Override serialization for complex types** - Enums, boxed types, and custom data need explicit handling.

5. **Exclude transient properties** - Override `list_properties` to skip properties that shouldn't be persisted.

6. **Use nullable defaults wisely** - Properties are nullable by default; set NOT NULL constraints via mapper or DDL.
