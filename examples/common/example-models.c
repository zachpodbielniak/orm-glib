/* example-models.c
 *
 * Copyright 2025 Zach Pobiel
 *
 * This file is part of orm-glib.
 *
 * orm-glib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * orm-glib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with orm-glib.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Shared example models for orm-glib examples.
 */

#include "example-models.h"

/* ========================================================================= */
/* ExampleUser                                                               */
/* ========================================================================= */

enum
{
    USER_PROP_0,
    USER_PROP_ID,
    USER_PROP_NAME,
    USER_PROP_EMAIL,
    USER_PROP_ACTIVE,
    USER_N_PROPS
};

static GParamSpec *user_props[USER_N_PROPS] = { NULL, };

static void example_user_serializable_init (OrmSerializableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ExampleUser, example_user, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_SERIALIZABLE,
                                                example_user_serializable_init))

static void
example_user_finalize (GObject *object)
{
    ExampleUser *self = EXAMPLE_USER (object);

    g_free (self->name);
    g_free (self->email);

    G_OBJECT_CLASS (example_user_parent_class)->finalize (object);
}

static void
example_user_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    ExampleUser *self = EXAMPLE_USER (object);

    switch (prop_id)
    {
    case USER_PROP_ID:
        g_value_set_int64 (value, self->id);
        break;
    case USER_PROP_NAME:
        g_value_set_string (value, self->name);
        break;
    case USER_PROP_EMAIL:
        g_value_set_string (value, self->email);
        break;
    case USER_PROP_ACTIVE:
        g_value_set_boolean (value, self->active);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
example_user_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    ExampleUser *self = EXAMPLE_USER (object);

    switch (prop_id)
    {
    case USER_PROP_ID:
        self->id = g_value_get_int64 (value);
        break;
    case USER_PROP_NAME:
        g_free (self->name);
        self->name = g_value_dup_string (value);
        break;
    case USER_PROP_EMAIL:
        g_free (self->email);
        self->email = g_value_dup_string (value);
        break;
    case USER_PROP_ACTIVE:
        self->active = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
example_user_class_init (ExampleUserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = example_user_finalize;
    object_class->get_property = example_user_get_property;
    object_class->set_property = example_user_set_property;

    user_props[USER_PROP_ID] = g_param_spec_int64 (
        "id", "ID", "User ID",
        G_MININT64, G_MAXINT64, 0,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    user_props[USER_PROP_NAME] = g_param_spec_string (
        "name", "Name", "User name",
        NULL,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    user_props[USER_PROP_EMAIL] = g_param_spec_string (
        "email", "Email", "User email address",
        NULL,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    user_props[USER_PROP_ACTIVE] = g_param_spec_boolean (
        "active", "Active", "Whether user is active",
        TRUE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, USER_N_PROPS, user_props);
}

static void
example_user_init (ExampleUser *self)
{
    self->id = 0;
    self->name = NULL;
    self->email = NULL;
    self->active = TRUE;
}

static const gchar *
example_user_get_table_name (OrmSerializable *serializable)
{
    (void) serializable;
    return "users";
}

static const gchar *
example_user_get_primary_key (OrmSerializable *serializable)
{
    (void) serializable;
    return "id";
}

static void
example_user_serializable_init (OrmSerializableInterface *iface)
{
    iface->get_table_name = example_user_get_table_name;
    iface->get_primary_key = example_user_get_primary_key;
}

ExampleUser *
example_user_new (const gchar *name,
                  const gchar *email)
{
    return g_object_new (EXAMPLE_TYPE_USER,
                         "name", name,
                         "email", email,
                         "active", TRUE,
                         NULL);
}

/* ========================================================================= */
/* ExampleProduct                                                            */
/* ========================================================================= */

enum
{
    PRODUCT_PROP_0,
    PRODUCT_PROP_ID,
    PRODUCT_PROP_NAME,
    PRODUCT_PROP_CATEGORY,
    PRODUCT_PROP_PRICE,
    PRODUCT_PROP_STOCK,
    PRODUCT_N_PROPS
};

static GParamSpec *product_props[PRODUCT_N_PROPS] = { NULL, };

static void example_product_serializable_init (OrmSerializableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ExampleProduct, example_product, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_SERIALIZABLE,
                                                example_product_serializable_init))

static void
example_product_finalize (GObject *object)
{
    ExampleProduct *self = EXAMPLE_PRODUCT (object);

    g_free (self->name);
    g_free (self->category);

    G_OBJECT_CLASS (example_product_parent_class)->finalize (object);
}

static void
example_product_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
    ExampleProduct *self = EXAMPLE_PRODUCT (object);

    switch (prop_id)
    {
    case PRODUCT_PROP_ID:
        g_value_set_int64 (value, self->id);
        break;
    case PRODUCT_PROP_NAME:
        g_value_set_string (value, self->name);
        break;
    case PRODUCT_PROP_CATEGORY:
        g_value_set_string (value, self->category);
        break;
    case PRODUCT_PROP_PRICE:
        g_value_set_double (value, self->price);
        break;
    case PRODUCT_PROP_STOCK:
        g_value_set_int (value, self->stock);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
example_product_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
    ExampleProduct *self = EXAMPLE_PRODUCT (object);

    switch (prop_id)
    {
    case PRODUCT_PROP_ID:
        self->id = g_value_get_int64 (value);
        break;
    case PRODUCT_PROP_NAME:
        g_free (self->name);
        self->name = g_value_dup_string (value);
        break;
    case PRODUCT_PROP_CATEGORY:
        g_free (self->category);
        self->category = g_value_dup_string (value);
        break;
    case PRODUCT_PROP_PRICE:
        self->price = g_value_get_double (value);
        break;
    case PRODUCT_PROP_STOCK:
        self->stock = g_value_get_int (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
example_product_class_init (ExampleProductClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = example_product_finalize;
    object_class->get_property = example_product_get_property;
    object_class->set_property = example_product_set_property;

    product_props[PRODUCT_PROP_ID] = g_param_spec_int64 (
        "id", "ID", "Product ID",
        G_MININT64, G_MAXINT64, 0,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    product_props[PRODUCT_PROP_NAME] = g_param_spec_string (
        "name", "Name", "Product name",
        NULL,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    product_props[PRODUCT_PROP_CATEGORY] = g_param_spec_string (
        "category", "Category", "Product category",
        NULL,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    product_props[PRODUCT_PROP_PRICE] = g_param_spec_double (
        "price", "Price", "Product price",
        0.0, G_MAXDOUBLE, 0.0,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    product_props[PRODUCT_PROP_STOCK] = g_param_spec_int (
        "stock", "Stock", "Number in stock",
        0, G_MAXINT, 0,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, PRODUCT_N_PROPS, product_props);
}

static void
example_product_init (ExampleProduct *self)
{
    self->id = 0;
    self->name = NULL;
    self->category = NULL;
    self->price = 0.0;
    self->stock = 0;
}

static const gchar *
example_product_get_table_name (OrmSerializable *serializable)
{
    (void) serializable;
    return "products";
}

static const gchar *
example_product_get_primary_key (OrmSerializable *serializable)
{
    (void) serializable;
    return "id";
}

static void
example_product_serializable_init (OrmSerializableInterface *iface)
{
    iface->get_table_name = example_product_get_table_name;
    iface->get_primary_key = example_product_get_primary_key;
}

ExampleProduct *
example_product_new (const gchar *name,
                     const gchar *category,
                     gdouble      price,
                     gint         stock)
{
    return g_object_new (EXAMPLE_TYPE_PRODUCT,
                         "name", name,
                         "category", category,
                         "price", price,
                         "stock", stock,
                         NULL);
}

/* ========================================================================= */
/* ExampleAuthor                                                             */
/* ========================================================================= */

enum
{
    AUTHOR_PROP_0,
    AUTHOR_PROP_ID,
    AUTHOR_PROP_NAME,
    AUTHOR_N_PROPS
};

static GParamSpec *author_props[AUTHOR_N_PROPS] = { NULL, };

static void example_author_serializable_init (OrmSerializableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ExampleAuthor, example_author, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_SERIALIZABLE,
                                                example_author_serializable_init))

static void
example_author_finalize (GObject *object)
{
    ExampleAuthor *self = EXAMPLE_AUTHOR (object);

    g_free (self->name);

    G_OBJECT_CLASS (example_author_parent_class)->finalize (object);
}

static void
example_author_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
    ExampleAuthor *self = EXAMPLE_AUTHOR (object);

    switch (prop_id)
    {
    case AUTHOR_PROP_ID:
        g_value_set_int64 (value, self->id);
        break;
    case AUTHOR_PROP_NAME:
        g_value_set_string (value, self->name);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
example_author_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
    ExampleAuthor *self = EXAMPLE_AUTHOR (object);

    switch (prop_id)
    {
    case AUTHOR_PROP_ID:
        self->id = g_value_get_int64 (value);
        break;
    case AUTHOR_PROP_NAME:
        g_free (self->name);
        self->name = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
example_author_class_init (ExampleAuthorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = example_author_finalize;
    object_class->get_property = example_author_get_property;
    object_class->set_property = example_author_set_property;

    author_props[AUTHOR_PROP_ID] = g_param_spec_int64 (
        "id", "ID", "Author ID",
        G_MININT64, G_MAXINT64, 0,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    author_props[AUTHOR_PROP_NAME] = g_param_spec_string (
        "name", "Name", "Author name",
        NULL,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, AUTHOR_N_PROPS, author_props);
}

static void
example_author_init (ExampleAuthor *self)
{
    self->id = 0;
    self->name = NULL;
}

static const gchar *
example_author_get_table_name (OrmSerializable *serializable)
{
    (void) serializable;
    return "authors";
}

static const gchar *
example_author_get_primary_key (OrmSerializable *serializable)
{
    (void) serializable;
    return "id";
}

static void
example_author_serializable_init (OrmSerializableInterface *iface)
{
    iface->get_table_name = example_author_get_table_name;
    iface->get_primary_key = example_author_get_primary_key;
}

ExampleAuthor *
example_author_new (const gchar *name)
{
    return g_object_new (EXAMPLE_TYPE_AUTHOR,
                         "name", name,
                         NULL);
}

/* ========================================================================= */
/* ExamplePost                                                               */
/* ========================================================================= */

enum
{
    POST_PROP_0,
    POST_PROP_ID,
    POST_PROP_TITLE,
    POST_PROP_AUTHOR_ID,
    POST_N_PROPS
};

static GParamSpec *post_props[POST_N_PROPS] = { NULL, };

static void example_post_serializable_init (OrmSerializableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ExamplePost, example_post, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_SERIALIZABLE,
                                                example_post_serializable_init))

static void
example_post_finalize (GObject *object)
{
    ExamplePost *self = EXAMPLE_POST (object);

    g_free (self->title);

    G_OBJECT_CLASS (example_post_parent_class)->finalize (object);
}

static void
example_post_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    ExamplePost *self = EXAMPLE_POST (object);

    switch (prop_id)
    {
    case POST_PROP_ID:
        g_value_set_int64 (value, self->id);
        break;
    case POST_PROP_TITLE:
        g_value_set_string (value, self->title);
        break;
    case POST_PROP_AUTHOR_ID:
        g_value_set_int64 (value, self->author_id);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
example_post_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    ExamplePost *self = EXAMPLE_POST (object);

    switch (prop_id)
    {
    case POST_PROP_ID:
        self->id = g_value_get_int64 (value);
        break;
    case POST_PROP_TITLE:
        g_free (self->title);
        self->title = g_value_dup_string (value);
        break;
    case POST_PROP_AUTHOR_ID:
        self->author_id = g_value_get_int64 (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
example_post_class_init (ExamplePostClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = example_post_finalize;
    object_class->get_property = example_post_get_property;
    object_class->set_property = example_post_set_property;

    post_props[POST_PROP_ID] = g_param_spec_int64 (
        "id", "ID", "Post ID",
        G_MININT64, G_MAXINT64, 0,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    post_props[POST_PROP_TITLE] = g_param_spec_string (
        "title", "Title", "Post title",
        NULL,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    post_props[POST_PROP_AUTHOR_ID] = g_param_spec_int64 (
        "author-id", "Author ID", "Foreign key to author",
        G_MININT64, G_MAXINT64, 0,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, POST_N_PROPS, post_props);
}

static void
example_post_init (ExamplePost *self)
{
    self->id = 0;
    self->title = NULL;
    self->author_id = 0;
}

static const gchar *
example_post_get_table_name (OrmSerializable *serializable)
{
    (void) serializable;
    return "posts";
}

static const gchar *
example_post_get_primary_key (OrmSerializable *serializable)
{
    (void) serializable;
    return "id";
}

static void
example_post_serializable_init (OrmSerializableInterface *iface)
{
    iface->get_table_name = example_post_get_table_name;
    iface->get_primary_key = example_post_get_primary_key;
}

ExamplePost *
example_post_new (const gchar *title,
                  gint64       author_id)
{
    return g_object_new (EXAMPLE_TYPE_POST,
                         "title", title,
                         "author-id", author_id,
                         NULL);
}
