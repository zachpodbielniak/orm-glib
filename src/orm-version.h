/* orm-version.h
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

#ifndef ORM_VERSION_H
#define ORM_VERSION_H

#if !defined(ORM_INSIDE) && !defined(ORM_COMPILATION)
#error "Only <orm.h> can be included directly."
#endif

/**
 * ORM_MAJOR_VERSION:
 *
 * orm-glib major version component (e.g. 1 if version is 1.2.3)
 */
#define ORM_MAJOR_VERSION (0)

/**
 * ORM_MINOR_VERSION:
 *
 * orm-glib minor version component (e.g. 2 if version is 1.2.3)
 */
#define ORM_MINOR_VERSION (1)

/**
 * ORM_MICRO_VERSION:
 *
 * orm-glib micro version component (e.g. 3 if version is 1.2.3)
 */
#define ORM_MICRO_VERSION (0)

/**
 * ORM_VERSION:
 *
 * orm-glib version as a string constant
 */
#define ORM_VERSION "0.1.0"

/**
 * ORM_API_VERSION:
 *
 * orm-glib API version as a string constant
 */
#define ORM_API_VERSION "0.1"

/**
 * ORM_CHECK_VERSION:
 * @major: major version number
 * @minor: minor version number
 * @micro: micro version number
 *
 * Checks whether the orm-glib version is at least the specified version.
 *
 * Returns: %TRUE if the version is at least @major.@minor.@micro
 */
#define ORM_CHECK_VERSION(major, minor, micro) \
    (ORM_MAJOR_VERSION > (major) || \
     (ORM_MAJOR_VERSION == (major) && ORM_MINOR_VERSION > (minor)) || \
     (ORM_MAJOR_VERSION == (major) && ORM_MINOR_VERSION == (minor) && \
      ORM_MICRO_VERSION >= (micro)))

/**
 * ORM_ENCODE_VERSION:
 * @major: major version number
 * @minor: minor version number
 * @micro: micro version number
 *
 * Encodes the version into an integer for comparison.
 *
 * Returns: Encoded version number
 */
#define ORM_ENCODE_VERSION(major, minor, micro) \
    ((major) * 10000 + (minor) * 100 + (micro))

/**
 * ORM_VERSION_HEX:
 *
 * The current orm-glib version encoded as an integer.
 */
#define ORM_VERSION_HEX \
    ORM_ENCODE_VERSION(ORM_MAJOR_VERSION, ORM_MINOR_VERSION, ORM_MICRO_VERSION)

#endif /* ORM_VERSION_H */
