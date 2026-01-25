/* example-models.h
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

#ifndef EXAMPLE_MODELS_H
#define EXAMPLE_MODELS_H

#include <orm.h>

G_BEGIN_DECLS

/*
 * ExampleUser - A simple user model.
 *
 * Fields:
 *   id     - Primary key (auto-increment)
 *   name   - User's display name
 *   email  - User's email address
 *   active - Whether the user is active
 */
#define EXAMPLE_TYPE_USER (example_user_get_type ())
G_DECLARE_FINAL_TYPE (ExampleUser, example_user, EXAMPLE, USER, GObject)

struct _ExampleUser
{
    GObject parent_instance;

    gint64      id;
    gchar      *name;
    gchar      *email;
    gboolean    active;
};

ExampleUser * example_user_new (const gchar *name,
                                const gchar *email);

/*
 * ExampleProduct - A product model for inventory examples.
 *
 * Fields:
 *   id       - Primary key (auto-increment)
 *   name     - Product name
 *   category - Product category
 *   price    - Product price
 *   stock    - Number in stock
 */
#define EXAMPLE_TYPE_PRODUCT (example_product_get_type ())
G_DECLARE_FINAL_TYPE (ExampleProduct, example_product, EXAMPLE, PRODUCT, GObject)

struct _ExampleProduct
{
    GObject parent_instance;

    gint64      id;
    gchar      *name;
    gchar      *category;
    gdouble     price;
    gint        stock;
};

ExampleProduct * example_product_new (const gchar *name,
                                      const gchar *category,
                                      gdouble      price,
                                      gint         stock);

/*
 * ExampleAuthor - An author model for relationship examples.
 *
 * Fields:
 *   id   - Primary key (auto-increment)
 *   name - Author's name
 */
#define EXAMPLE_TYPE_AUTHOR (example_author_get_type ())
G_DECLARE_FINAL_TYPE (ExampleAuthor, example_author, EXAMPLE, AUTHOR, GObject)

struct _ExampleAuthor
{
    GObject parent_instance;

    gint64      id;
    gchar      *name;
};

ExampleAuthor * example_author_new (const gchar *name);

/*
 * ExamplePost - A blog post model with foreign key to Author.
 *
 * Fields:
 *   id        - Primary key (auto-increment)
 *   title     - Post title
 *   author_id - Foreign key to ExampleAuthor
 */
#define EXAMPLE_TYPE_POST (example_post_get_type ())
G_DECLARE_FINAL_TYPE (ExamplePost, example_post, EXAMPLE, POST, GObject)

struct _ExamplePost
{
    GObject parent_instance;

    gint64      id;
    gchar      *title;
    gint64      author_id;
};

ExamplePost * example_post_new (const gchar *title,
                                gint64       author_id);

G_END_DECLS

#endif /* EXAMPLE_MODELS_H */
