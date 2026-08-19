/* orm-driver-registry.c
 *
 * Copyright 2025 Zach Podbielniak
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

#include "orm-driver-registry.h"
#include "../core/orm-error.h"

#ifdef ORM_ENABLE_SQLITE
#include "sqlite/orm-sqlite-driver.h"
#endif

#ifdef ORM_ENABLE_POSTGRES
#include "postgres/orm-postgres-driver.h"
#endif

#ifdef ORM_ENABLE_MYSQL
#include "mysql/orm-mysql-driver.h"
#endif

/*
 * OrmDriverRegistry - URL scheme to backend lookup.
 *
 * A plain GHashTable keyed by scheme, not a GIOExtensionPoint.  Extension
 * points are built for GIOModule-based dynamic loading with
 * priority-ordered "pick the best implementation" semantics; what this
 * needs is exact keyed lookup of statically linked backends, where two
 * candidates for one scheme is a bug rather than a ranking problem.
 *
 * The registry is locked because registration can happen from any thread
 * once the library is in use, though in practice everything registers
 * during the first get_default().
 */

struct _OrmDriverRegistry
{
    GObject     parent_instance;

    GMutex      lock;
    GHashTable *by_scheme;   /* owned gchar* -> owned OrmDriver* (shared) */
    GPtrArray  *drivers;     /* owned OrmDriver*, one entry per driver */
};

G_DEFINE_TYPE (OrmDriverRegistry, orm_driver_registry, G_TYPE_OBJECT)

static void
orm_driver_registry_finalize (GObject *object)
{
    OrmDriverRegistry *self = ORM_DRIVER_REGISTRY (object);

    g_clear_pointer (&self->by_scheme, g_hash_table_unref);
    g_clear_pointer (&self->drivers, g_ptr_array_unref);
    g_mutex_clear (&self->lock);

    G_OBJECT_CLASS (orm_driver_registry_parent_class)->finalize (object);
}

static void
orm_driver_registry_class_init (OrmDriverRegistryClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = orm_driver_registry_finalize;
}

static void
orm_driver_registry_init (OrmDriverRegistry *self)
{
    g_mutex_init (&self->lock);

    /*
     * The scheme table does not own its values: a driver claiming two
     * schemes would otherwise be unreffed twice.  The drivers array is
     * the single owner.
     */
    self->by_scheme = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, NULL);
    self->drivers = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * orm_driver_registry_register:
 * @self: An #OrmDriverRegistry
 * @driver: (transfer none): The driver to add
 * @error: Return location for error
 *
 * Registers @driver under every scheme it claims.
 *
 * Registration is all-or-nothing: if any scheme is already taken the call
 * fails and none are added, so a failed registration cannot leave the
 * registry half-updated with a driver reachable by some of its names.
 *
 * Returns: %TRUE on success
 */
gboolean
orm_driver_registry_register (OrmDriverRegistry  *self,
                              OrmDriver          *driver,
                              GError            **error)
{
    const gchar * const *schemes;
    gint                 i;

    g_return_val_if_fail (ORM_IS_DRIVER_REGISTRY (self), FALSE);
    g_return_val_if_fail (ORM_IS_DRIVER (driver), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    schemes = orm_driver_get_schemes (driver);
    if (schemes == NULL || schemes[0] == NULL)
    {
        g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                     "Driver \"%s\" claims no URL schemes",
                     orm_driver_get_name (driver));
        return FALSE;
    }

    g_mutex_lock (&self->lock);

    for (i = 0; schemes[i] != NULL; i++)
    {
        if (g_hash_table_contains (self->by_scheme, schemes[i]))
        {
            OrmDriver *existing = g_hash_table_lookup (self->by_scheme, schemes[i]);

            g_mutex_unlock (&self->lock);
            g_set_error (error, ORM_ERROR, ORM_ERROR_INVALID_OPERATION,
                         "URL scheme \"%s\" is already claimed by driver \"%s\"",
                         schemes[i], orm_driver_get_name (existing));
            return FALSE;
        }
    }

    g_ptr_array_add (self->drivers, g_object_ref (driver));

    for (i = 0; schemes[i] != NULL; i++)
        g_hash_table_insert (self->by_scheme, g_strdup (schemes[i]), driver);

    g_mutex_unlock (&self->lock);

    return TRUE;
}

/**
 * orm_driver_registry_lookup:
 * @self: An #OrmDriverRegistry
 * @scheme: A URL scheme
 *
 * Finds the driver claiming @scheme.
 *
 * Returns: (transfer none) (nullable): The driver, or %NULL
 */
OrmDriver *
orm_driver_registry_lookup (OrmDriverRegistry *self,
                            const gchar       *scheme)
{
    OrmDriver *driver;

    g_return_val_if_fail (ORM_IS_DRIVER_REGISTRY (self), NULL);
    g_return_val_if_fail (scheme != NULL, NULL);

    g_mutex_lock (&self->lock);
    driver = g_hash_table_lookup (self->by_scheme, scheme);
    g_mutex_unlock (&self->lock);

    return driver;
}

/**
 * orm_driver_registry_lookup_dialect:
 * @self: An #OrmDriverRegistry
 * @dialect_type: The dialect type
 *
 * Finds the driver that generates SQL for @dialect_type.
 *
 * Returns: (transfer none) (nullable): The driver, or %NULL
 */
OrmDriver *
orm_driver_registry_lookup_dialect (OrmDriverRegistry *self,
                                    OrmDialectType     dialect_type)
{
    OrmDriver *found = NULL;
    guint      i;

    g_return_val_if_fail (ORM_IS_DRIVER_REGISTRY (self), NULL);

    g_mutex_lock (&self->lock);

    for (i = 0; i < self->drivers->len; i++)
    {
        OrmDriver *driver = g_ptr_array_index (self->drivers, i);

        if (orm_driver_get_dialect_type (driver) == dialect_type)
        {
            found = driver;
            break;
        }
    }

    g_mutex_unlock (&self->lock);

    return found;
}

/**
 * orm_driver_registry_list:
 * @self: An #OrmDriverRegistry
 *
 * Lists the registered drivers.
 *
 * Returns: (transfer container) (element-type OrmDriver): The drivers
 */
GPtrArray *
orm_driver_registry_list (OrmDriverRegistry *self)
{
    GPtrArray *out;
    guint      i;

    g_return_val_if_fail (ORM_IS_DRIVER_REGISTRY (self), NULL);

    out = g_ptr_array_new ();

    g_mutex_lock (&self->lock);
    for (i = 0; i < self->drivers->len; i++)
        g_ptr_array_add (out, g_ptr_array_index (self->drivers, i));
    g_mutex_unlock (&self->lock);

    return out;
}

/**
 * orm_driver_registry_list_schemes:
 * @self: An #OrmDriverRegistry
 *
 * Lists every registered URL scheme, sorted so the output is stable.
 *
 * Returns: (transfer full) (array zero-terminated=1): The schemes
 */
gchar **
orm_driver_registry_list_schemes (OrmDriverRegistry *self)
{
    GPtrArray      *out;
    GHashTableIter  iter;
    gpointer        key;

    g_return_val_if_fail (ORM_IS_DRIVER_REGISTRY (self), NULL);

    out = g_ptr_array_new ();

    g_mutex_lock (&self->lock);
    g_hash_table_iter_init (&iter, self->by_scheme);
    while (g_hash_table_iter_next (&iter, &key, NULL))
        g_ptr_array_add (out, g_strdup ((const gchar *) key));
    g_mutex_unlock (&self->lock);

    g_ptr_array_sort_values (out, (GCompareFunc) g_strcmp0);
    g_ptr_array_add (out, NULL);

    return (gchar **) g_ptr_array_free (out, FALSE);
}

/*
 * Builds the registry and fills it with whichever backends were compiled
 * in.  Registration happens here rather than in a constructor attribute
 * so it stays lazy and gnu89-clean.
 */
static OrmDriverRegistry *
orm_driver_registry_create_default (void)
{
    OrmDriverRegistry *self;
    g_autoptr(GError)  error = NULL;

    self = g_object_new (ORM_TYPE_DRIVER_REGISTRY, NULL);

#ifdef ORM_ENABLE_SQLITE
    {
        g_autoptr(OrmSqliteDriver) driver = orm_sqlite_driver_new ();

        if (!orm_driver_registry_register (self, ORM_DRIVER (driver), &error))
            g_warning ("orm: failed to register the SQLite driver: %s",
                       error->message);
        g_clear_error (&error);
    }
#endif

#ifdef ORM_ENABLE_POSTGRES
    {
        g_autoptr(OrmPostgresDriver) driver = orm_postgres_driver_new ();

        if (!orm_driver_registry_register (self, ORM_DRIVER (driver), &error))
            g_warning ("orm: failed to register the PostgreSQL driver: %s",
                       error->message);
        g_clear_error (&error);
    }
#endif

#ifdef ORM_ENABLE_MYSQL
    {
        g_autoptr(OrmMysqlDriver) driver = orm_mysql_driver_new ();

        if (!orm_driver_registry_register (self, ORM_DRIVER (driver), &error))
            g_warning ("orm: failed to register the MySQL driver: %s",
                       error->message);
        g_clear_error (&error);
    }
#endif

    return self;
}

/**
 * orm_driver_registry_get_default:
 *
 * Gets the process-wide driver registry, creating it and registering the
 * compiled-in backends on first use.
 *
 * Returns: (transfer none): The registry
 */
OrmDriverRegistry *
orm_driver_registry_get_default (void)
{
    static gsize    initialized = 0;
    static OrmDriverRegistry *instance = NULL;

    if (g_once_init_enter (&initialized))
    {
        instance = orm_driver_registry_create_default ();
        g_once_init_leave (&initialized, 1);
    }

    return instance;
}
