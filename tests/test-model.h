/* test-model.h
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

#ifndef TEST_MODEL_H
#define TEST_MODEL_H

#include <glib-object.h>

#define ORM_INSIDE
#include "orm.h"
#undef ORM_INSIDE

G_BEGIN_DECLS

/*
 * TestUser - Test GObject implementing OrmSerializable.
 *
 * Represents a user with id, name, email, active status, and created_at.
 */

#define TEST_TYPE_USER (test_user_get_type ())

G_DECLARE_FINAL_TYPE (TestUser, test_user, TEST, USER, GObject)

/*
 * test_user_new:
 *
 * Creates a new TestUser instance.
 *
 * Returns: (transfer full): A new #TestUser
 */
TestUser *      test_user_new           (void);

/*
 * test_user_new_with_values:
 * @name: The user's name
 * @email: The user's email
 *
 * Creates a new TestUser with initial values.
 *
 * Returns: (transfer full): A new #TestUser
 */
TestUser *      test_user_new_with_values   (const gchar *name,
                                             const gchar *email);

/*
 * Property accessors
 */
gint64          test_user_get_id        (TestUser *self);
void            test_user_set_id        (TestUser *self, gint64 id);

const gchar *   test_user_get_name      (TestUser *self);
void            test_user_set_name      (TestUser *self, const gchar *name);

const gchar *   test_user_get_email     (TestUser *self);
void            test_user_set_email     (TestUser *self, const gchar *email);

gboolean        test_user_get_active    (TestUser *self);
void            test_user_set_active    (TestUser *self, gboolean active);

GDateTime *     test_user_get_created_at (TestUser *self);
void            test_user_set_created_at (TestUser *self, GDateTime *created_at);

/*
 * TestPost - Test GObject implementing OrmSerializable.
 *
 * Represents a post with id, title, content, and user_id (foreign key).
 */

#define TEST_TYPE_POST (test_post_get_type ())

G_DECLARE_FINAL_TYPE (TestPost, test_post, TEST, POST, GObject)

/*
 * test_post_new:
 *
 * Creates a new TestPost instance.
 *
 * Returns: (transfer full): A new #TestPost
 */
TestPost *      test_post_new           (void);

/*
 * test_post_new_with_values:
 * @title: The post title
 * @content: The post content
 * @user_id: The author's user ID
 *
 * Creates a new TestPost with initial values.
 *
 * Returns: (transfer full): A new #TestPost
 */
TestPost *      test_post_new_with_values   (const gchar *title,
                                             const gchar *content,
                                             gint64       user_id);

/*
 * Property accessors
 */
gint64          test_post_get_id        (TestPost *self);
void            test_post_set_id        (TestPost *self, gint64 id);

const gchar *   test_post_get_title     (TestPost *self);
void            test_post_set_title     (TestPost *self, const gchar *title);

const gchar *   test_post_get_content   (TestPost *self);
void            test_post_set_content   (TestPost *self, const gchar *content);

gint64          test_post_get_user_id   (TestPost *self);
void            test_post_set_user_id   (TestPost *self, gint64 user_id);

G_END_DECLS

#endif /* TEST_MODEL_H */
