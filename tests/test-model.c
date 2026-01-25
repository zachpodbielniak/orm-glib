/* test-model.c
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
 */

#include "test-model.h"

/* ============================================================================
 * TestUser Implementation
 * ============================================================================ */

struct _TestUser
{
    GObject    parent_instance;

    gint64     id;
    gchar     *name;
    gchar     *email;
    gboolean   active;
    GDateTime *created_at;
};

static void test_user_serializable_init (OrmSerializableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TestUser, test_user, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_SERIALIZABLE,
                                                test_user_serializable_init))

enum {
    USER_PROP_0,
    USER_PROP_ID,
    USER_PROP_NAME,
    USER_PROP_EMAIL,
    USER_PROP_ACTIVE,
    USER_PROP_CREATED_AT,
    USER_N_PROPS
};

static GParamSpec *user_props[USER_N_PROPS];

static void
test_user_finalize (GObject *object)
{
    TestUser *self = TEST_USER (object);

    g_free (self->name);
    g_free (self->email);
    g_clear_pointer (&self->created_at, g_date_time_unref);

    G_OBJECT_CLASS (test_user_parent_class)->finalize (object);
}

static void
test_user_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
    TestUser *self = TEST_USER (object);

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
    case USER_PROP_CREATED_AT:
        g_value_set_boxed (value, self->created_at);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
test_user_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
    TestUser *self = TEST_USER (object);

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
    case USER_PROP_CREATED_AT:
        g_clear_pointer (&self->created_at, g_date_time_unref);
        self->created_at = g_value_dup_boxed (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
test_user_class_init (TestUserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = test_user_finalize;
    object_class->get_property = test_user_get_property;
    object_class->set_property = test_user_set_property;

    user_props[USER_PROP_ID] =
        g_param_spec_int64 ("id", "ID", "User ID",
                            G_MININT64, G_MAXINT64, 0,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    user_props[USER_PROP_NAME] =
        g_param_spec_string ("name", "Name", "User name",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    user_props[USER_PROP_EMAIL] =
        g_param_spec_string ("email", "Email", "User email",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    user_props[USER_PROP_ACTIVE] =
        g_param_spec_boolean ("active", "Active", "User is active",
                              TRUE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    user_props[USER_PROP_CREATED_AT] =
        g_param_spec_boxed ("created-at", "Created At", "Creation timestamp",
                            G_TYPE_DATE_TIME,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, USER_N_PROPS, user_props);
}

static void
test_user_init (TestUser *self)
{
    self->id = 0;
    self->name = NULL;
    self->email = NULL;
    self->active = TRUE;
    self->created_at = NULL;
}

/*
 * OrmSerializable interface implementation for TestUser.
 */
static const gchar *
test_user_get_table_name (OrmSerializable *serializable)
{
    (void) serializable;
    return "test_user";
}

static const gchar *
test_user_get_primary_key (OrmSerializable *serializable)
{
    (void) serializable;
    return "id";
}

static void
test_user_serializable_init (OrmSerializableInterface *iface)
{
    iface->get_table_name = test_user_get_table_name;
    iface->get_primary_key = test_user_get_primary_key;
    /* Use default implementations for serialize/deserialize */
}

/*
 * Public API for TestUser
 */
TestUser *
test_user_new (void)
{
    return g_object_new (TEST_TYPE_USER, NULL);
}

TestUser *
test_user_new_with_values (const gchar *name,
                           const gchar *email)
{
    return g_object_new (TEST_TYPE_USER,
                         "name", name,
                         "email", email,
                         NULL);
}

gint64
test_user_get_id (TestUser *self)
{
    g_return_val_if_fail (TEST_IS_USER (self), 0);
    return self->id;
}

void
test_user_set_id (TestUser *self,
                  gint64    id)
{
    g_return_if_fail (TEST_IS_USER (self));
    self->id = id;
    g_object_notify_by_pspec (G_OBJECT (self), user_props[USER_PROP_ID]);
}

const gchar *
test_user_get_name (TestUser *self)
{
    g_return_val_if_fail (TEST_IS_USER (self), NULL);
    return self->name;
}

void
test_user_set_name (TestUser    *self,
                    const gchar *name)
{
    g_return_if_fail (TEST_IS_USER (self));
    g_free (self->name);
    self->name = g_strdup (name);
    g_object_notify_by_pspec (G_OBJECT (self), user_props[USER_PROP_NAME]);
}

const gchar *
test_user_get_email (TestUser *self)
{
    g_return_val_if_fail (TEST_IS_USER (self), NULL);
    return self->email;
}

void
test_user_set_email (TestUser    *self,
                     const gchar *email)
{
    g_return_if_fail (TEST_IS_USER (self));
    g_free (self->email);
    self->email = g_strdup (email);
    g_object_notify_by_pspec (G_OBJECT (self), user_props[USER_PROP_EMAIL]);
}

gboolean
test_user_get_active (TestUser *self)
{
    g_return_val_if_fail (TEST_IS_USER (self), FALSE);
    return self->active;
}

void
test_user_set_active (TestUser *self,
                      gboolean  active)
{
    g_return_if_fail (TEST_IS_USER (self));
    self->active = active;
    g_object_notify_by_pspec (G_OBJECT (self), user_props[USER_PROP_ACTIVE]);
}

GDateTime *
test_user_get_created_at (TestUser *self)
{
    g_return_val_if_fail (TEST_IS_USER (self), NULL);
    return self->created_at;
}

void
test_user_set_created_at (TestUser  *self,
                          GDateTime *created_at)
{
    g_return_if_fail (TEST_IS_USER (self));
    g_clear_pointer (&self->created_at, g_date_time_unref);
    if (created_at != NULL)
    {
        self->created_at = g_date_time_ref (created_at);
    }
    g_object_notify_by_pspec (G_OBJECT (self), user_props[USER_PROP_CREATED_AT]);
}

/* ============================================================================
 * TestPost Implementation
 * ============================================================================ */

struct _TestPost
{
    GObject   parent_instance;

    gint64    id;
    gchar    *title;
    gchar    *content;
    gint64    user_id;
};

static void test_post_serializable_init (OrmSerializableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TestPost, test_post, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ORM_TYPE_SERIALIZABLE,
                                                test_post_serializable_init))

enum {
    POST_PROP_0,
    POST_PROP_ID,
    POST_PROP_TITLE,
    POST_PROP_CONTENT,
    POST_PROP_USER_ID,
    POST_N_PROPS
};

static GParamSpec *post_props[POST_N_PROPS];

static void
test_post_finalize (GObject *object)
{
    TestPost *self = TEST_POST (object);

    g_free (self->title);
    g_free (self->content);

    G_OBJECT_CLASS (test_post_parent_class)->finalize (object);
}

static void
test_post_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
    TestPost *self = TEST_POST (object);

    switch (prop_id)
    {
    case POST_PROP_ID:
        g_value_set_int64 (value, self->id);
        break;
    case POST_PROP_TITLE:
        g_value_set_string (value, self->title);
        break;
    case POST_PROP_CONTENT:
        g_value_set_string (value, self->content);
        break;
    case POST_PROP_USER_ID:
        g_value_set_int64 (value, self->user_id);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
test_post_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
    TestPost *self = TEST_POST (object);

    switch (prop_id)
    {
    case POST_PROP_ID:
        self->id = g_value_get_int64 (value);
        break;
    case POST_PROP_TITLE:
        g_free (self->title);
        self->title = g_value_dup_string (value);
        break;
    case POST_PROP_CONTENT:
        g_free (self->content);
        self->content = g_value_dup_string (value);
        break;
    case POST_PROP_USER_ID:
        self->user_id = g_value_get_int64 (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
test_post_class_init (TestPostClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = test_post_finalize;
    object_class->get_property = test_post_get_property;
    object_class->set_property = test_post_set_property;

    post_props[POST_PROP_ID] =
        g_param_spec_int64 ("id", "ID", "Post ID",
                            G_MININT64, G_MAXINT64, 0,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    post_props[POST_PROP_TITLE] =
        g_param_spec_string ("title", "Title", "Post title",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    post_props[POST_PROP_CONTENT] =
        g_param_spec_string ("content", "Content", "Post content",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    post_props[POST_PROP_USER_ID] =
        g_param_spec_int64 ("user-id", "User ID", "Author's user ID",
                            G_MININT64, G_MAXINT64, 0,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, POST_N_PROPS, post_props);
}

static void
test_post_init (TestPost *self)
{
    self->id = 0;
    self->title = NULL;
    self->content = NULL;
    self->user_id = 0;
}

/*
 * OrmSerializable interface implementation for TestPost.
 */
static const gchar *
test_post_get_table_name (OrmSerializable *serializable)
{
    (void) serializable;
    return "test_post";
}

static const gchar *
test_post_get_primary_key (OrmSerializable *serializable)
{
    (void) serializable;
    return "id";
}

static void
test_post_serializable_init (OrmSerializableInterface *iface)
{
    iface->get_table_name = test_post_get_table_name;
    iface->get_primary_key = test_post_get_primary_key;
    /* Use default implementations for serialize/deserialize */
}

/*
 * Public API for TestPost
 */
TestPost *
test_post_new (void)
{
    return g_object_new (TEST_TYPE_POST, NULL);
}

TestPost *
test_post_new_with_values (const gchar *title,
                           const gchar *content,
                           gint64       user_id)
{
    return g_object_new (TEST_TYPE_POST,
                         "title", title,
                         "content", content,
                         "user-id", user_id,
                         NULL);
}

gint64
test_post_get_id (TestPost *self)
{
    g_return_val_if_fail (TEST_IS_POST (self), 0);
    return self->id;
}

void
test_post_set_id (TestPost *self,
                  gint64    id)
{
    g_return_if_fail (TEST_IS_POST (self));
    self->id = id;
    g_object_notify_by_pspec (G_OBJECT (self), post_props[POST_PROP_ID]);
}

const gchar *
test_post_get_title (TestPost *self)
{
    g_return_val_if_fail (TEST_IS_POST (self), NULL);
    return self->title;
}

void
test_post_set_title (TestPost    *self,
                     const gchar *title)
{
    g_return_if_fail (TEST_IS_POST (self));
    g_free (self->title);
    self->title = g_strdup (title);
    g_object_notify_by_pspec (G_OBJECT (self), post_props[POST_PROP_TITLE]);
}

const gchar *
test_post_get_content (TestPost *self)
{
    g_return_val_if_fail (TEST_IS_POST (self), NULL);
    return self->content;
}

void
test_post_set_content (TestPost    *self,
                       const gchar *content)
{
    g_return_if_fail (TEST_IS_POST (self));
    g_free (self->content);
    self->content = g_strdup (content);
    g_object_notify_by_pspec (G_OBJECT (self), post_props[POST_PROP_CONTENT]);
}

gint64
test_post_get_user_id (TestPost *self)
{
    g_return_val_if_fail (TEST_IS_POST (self), 0);
    return self->user_id;
}

void
test_post_set_user_id (TestPost *self,
                       gint64    user_id)
{
    g_return_if_fail (TEST_IS_POST (self));
    self->user_id = user_id;
    g_object_notify_by_pspec (G_OBJECT (self), post_props[POST_PROP_USER_ID]);
}
