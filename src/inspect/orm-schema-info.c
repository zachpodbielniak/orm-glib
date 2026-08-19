/* orm-schema-info.c
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

#include "orm-schema-info.h"

/*
 * The four records an inspector reports.
 *
 * Each is immutable after construction and reference counted, so a copy
 * is a ref and no accessor has to duplicate anything. That is what makes
 * them cheap to pass around a UI that is holding a whole schema.
 */

/* ---------------------------------------------------------------- */
/* OrmRelationKind                                                  */
/* ---------------------------------------------------------------- */

GType
orm_relation_kind_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { ORM_RELATION_TABLE, "ORM_RELATION_TABLE", "table" },
            { ORM_RELATION_VIEW, "ORM_RELATION_VIEW", "view" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("OrmRelationKind", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/* ---------------------------------------------------------------- */
/* OrmTableInfo                                                     */
/* ---------------------------------------------------------------- */

struct _OrmTableInfo
{
    gatomicrefcount  ref_count;

    gchar           *name;
    gchar           *schema;
    OrmRelationKind  kind;
};

static void
orm_table_info_free (OrmTableInfo *self)
{
    g_free (self->name);
    g_free (self->schema);
    g_free (self);
}

G_DEFINE_BOXED_TYPE (OrmTableInfo, orm_table_info,
                     orm_table_info_ref, orm_table_info_unref)

/**
 * orm_table_info_new:
 * @name: The relation name
 * @schema: (nullable): The schema it lives in
 * @kind: Table or view
 *
 * Returns: (transfer full): A new #OrmTableInfo
 */
OrmTableInfo *
orm_table_info_new (const gchar     *name,
                    const gchar     *schema,
                    OrmRelationKind  kind)
{
    OrmTableInfo *self;

    g_return_val_if_fail (name != NULL, NULL);

    self = g_new0 (OrmTableInfo, 1);
    g_atomic_ref_count_init (&self->ref_count);
    self->name = g_strdup (name);
    self->schema = g_strdup (schema);
    self->kind = kind;

    return self;
}

/**
 * orm_table_info_ref:
 * @self: An #OrmTableInfo
 *
 * Returns: (transfer full): @self
 */
OrmTableInfo *
orm_table_info_ref (OrmTableInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);

    g_atomic_ref_count_inc (&self->ref_count);
    return self;
}

/**
 * orm_table_info_unref:
 * @self: An #OrmTableInfo
 *
 * Drops a reference, freeing @self when the last one goes.
 */
void
orm_table_info_unref (OrmTableInfo *self)
{
    g_return_if_fail (self != NULL);

    if (g_atomic_ref_count_dec (&self->ref_count))
        orm_table_info_free (self);
}

/**
 * orm_table_info_get_name:
 * @self: An #OrmTableInfo
 *
 * Returns: (transfer none): The relation name
 */
const gchar *
orm_table_info_get_name (OrmTableInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->name;
}

/**
 * orm_table_info_get_schema:
 * @self: An #OrmTableInfo
 *
 * Returns: (transfer none) (nullable): The schema, or %NULL
 */
const gchar *
orm_table_info_get_schema (OrmTableInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->schema;
}

/**
 * orm_table_info_get_kind:
 * @self: An #OrmTableInfo
 *
 * Returns: Whether this is a table or a view
 */
OrmRelationKind
orm_table_info_get_kind (OrmTableInfo *self)
{
    g_return_val_if_fail (self != NULL, ORM_RELATION_TABLE);
    return self->kind;
}

/* ---------------------------------------------------------------- */
/* OrmColumnInfo                                                    */
/* ---------------------------------------------------------------- */

struct _OrmColumnInfo
{
    gatomicrefcount  ref_count;

    gchar           *name;
    gchar           *type_name;
    OrmValueType     value_type;
    gboolean         nullable;
    gchar           *default_value;
    gboolean         primary_key;
    gboolean         autoincrement;
    gint             ordinal;
};

static void
orm_column_info_free (OrmColumnInfo *self)
{
    g_free (self->name);
    g_free (self->type_name);
    g_free (self->default_value);
    g_free (self);
}

G_DEFINE_BOXED_TYPE (OrmColumnInfo, orm_column_info,
                     orm_column_info_ref, orm_column_info_unref)

/**
 * orm_column_info_new:
 * @name: The column name
 * @type_name: (nullable): The backend's name for the declared type
 * @value_type: The #OrmValueType values decode to
 * @nullable: Whether the column accepts NULL
 * @default_value: (nullable): The default, as SQL text
 * @primary_key: Whether the column is part of the primary key
 * @autoincrement: Whether the server generates the value
 * @ordinal: Zero-based position in the table
 *
 * Returns: (transfer full): A new #OrmColumnInfo
 */
OrmColumnInfo *
orm_column_info_new (const gchar  *name,
                     const gchar  *type_name,
                     OrmValueType  value_type,
                     gboolean      nullable,
                     const gchar  *default_value,
                     gboolean      primary_key,
                     gboolean      autoincrement,
                     gint          ordinal)
{
    OrmColumnInfo *self;

    g_return_val_if_fail (name != NULL, NULL);

    self = g_new0 (OrmColumnInfo, 1);
    g_atomic_ref_count_init (&self->ref_count);
    self->name = g_strdup (name);
    self->type_name = g_strdup (type_name);
    self->value_type = value_type;
    self->nullable = nullable;
    self->default_value = g_strdup (default_value);
    self->primary_key = primary_key;
    self->autoincrement = autoincrement;
    self->ordinal = ordinal;

    return self;
}

/**
 * orm_column_info_ref:
 * @self: An #OrmColumnInfo
 *
 * Returns: (transfer full): @self
 */
OrmColumnInfo *
orm_column_info_ref (OrmColumnInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);

    g_atomic_ref_count_inc (&self->ref_count);
    return self;
}

/**
 * orm_column_info_unref:
 * @self: An #OrmColumnInfo
 *
 * Drops a reference, freeing @self when the last one goes.
 */
void
orm_column_info_unref (OrmColumnInfo *self)
{
    g_return_if_fail (self != NULL);

    if (g_atomic_ref_count_dec (&self->ref_count))
        orm_column_info_free (self);
}

/**
 * orm_column_info_get_name:
 * @self: An #OrmColumnInfo
 *
 * Returns: (transfer none): The column name
 */
const gchar *
orm_column_info_get_name (OrmColumnInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->name;
}

/**
 * orm_column_info_get_type_name:
 * @self: An #OrmColumnInfo
 *
 * Gets the backend's own name for the declared type, such as
 * "VARCHAR(80)".
 *
 * Returns: (transfer none) (nullable): The type name, or %NULL
 */
const gchar *
orm_column_info_get_type_name (OrmColumnInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->type_name;
}

/**
 * orm_column_info_get_value_type:
 * @self: An #OrmColumnInfo
 *
 * Returns: The #OrmValueType values in this column decode to
 */
OrmValueType
orm_column_info_get_value_type (OrmColumnInfo *self)
{
    g_return_val_if_fail (self != NULL, ORM_VALUE_NULL);
    return self->value_type;
}

/**
 * orm_column_info_get_nullable:
 * @self: An #OrmColumnInfo
 *
 * Returns: %TRUE if the column accepts NULL
 */
gboolean
orm_column_info_get_nullable (OrmColumnInfo *self)
{
    g_return_val_if_fail (self != NULL, TRUE);
    return self->nullable;
}

/**
 * orm_column_info_get_default_value:
 * @self: An #OrmColumnInfo
 *
 * Gets the column default as the SQL text the schema gave, which may be
 * an expression such as CURRENT_TIMESTAMP rather than a literal.
 *
 * Returns: (transfer none) (nullable): The default, or %NULL if there is none
 */
const gchar *
orm_column_info_get_default_value (OrmColumnInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->default_value;
}

/**
 * orm_column_info_get_primary_key:
 * @self: An #OrmColumnInfo
 *
 * Returns: %TRUE if the column is part of the primary key
 */
gboolean
orm_column_info_get_primary_key (OrmColumnInfo *self)
{
    g_return_val_if_fail (self != NULL, FALSE);
    return self->primary_key;
}

/**
 * orm_column_info_get_autoincrement:
 * @self: An #OrmColumnInfo
 *
 * Returns: %TRUE if the server generates this column's value
 */
gboolean
orm_column_info_get_autoincrement (OrmColumnInfo *self)
{
    g_return_val_if_fail (self != NULL, FALSE);
    return self->autoincrement;
}

/**
 * orm_column_info_get_ordinal:
 * @self: An #OrmColumnInfo
 *
 * Returns: The zero-based position of the column in its table
 */
gint
orm_column_info_get_ordinal (OrmColumnInfo *self)
{
    g_return_val_if_fail (self != NULL, -1);
    return self->ordinal;
}

/* ---------------------------------------------------------------- */
/* OrmIndexInfo                                                     */
/* ---------------------------------------------------------------- */

struct _OrmIndexInfo
{
    gatomicrefcount  ref_count;

    gchar           *name;
    gboolean         unique;
    gchar          **columns;
};

static void
orm_index_info_free (OrmIndexInfo *self)
{
    g_free (self->name);
    g_strfreev (self->columns);
    g_free (self);
}

G_DEFINE_BOXED_TYPE (OrmIndexInfo, orm_index_info,
                     orm_index_info_ref, orm_index_info_unref)

/**
 * orm_index_info_new:
 * @name: The index name
 * @unique: Whether the index enforces uniqueness
 * @columns: (array zero-terminated=1): The indexed columns, in order
 *
 * Column order is significant: an index on (a, b) is not an index on
 * (b, a), and only the first can serve a lookup by @a alone.
 *
 * Returns: (transfer full): A new #OrmIndexInfo
 */
OrmIndexInfo *
orm_index_info_new (const gchar        *name,
                    gboolean            unique,
                    const gchar *const *columns)
{
    OrmIndexInfo *self;

    g_return_val_if_fail (name != NULL, NULL);

    self = g_new0 (OrmIndexInfo, 1);
    g_atomic_ref_count_init (&self->ref_count);
    self->name = g_strdup (name);
    self->unique = unique;
    self->columns = (columns != NULL)
        ? g_strdupv ((gchar **) columns)
        : g_new0 (gchar *, 1);

    return self;
}

/**
 * orm_index_info_ref:
 * @self: An #OrmIndexInfo
 *
 * Returns: (transfer full): @self
 */
OrmIndexInfo *
orm_index_info_ref (OrmIndexInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);

    g_atomic_ref_count_inc (&self->ref_count);
    return self;
}

/**
 * orm_index_info_unref:
 * @self: An #OrmIndexInfo
 *
 * Drops a reference, freeing @self when the last one goes.
 */
void
orm_index_info_unref (OrmIndexInfo *self)
{
    g_return_if_fail (self != NULL);

    if (g_atomic_ref_count_dec (&self->ref_count))
        orm_index_info_free (self);
}

/**
 * orm_index_info_get_name:
 * @self: An #OrmIndexInfo
 *
 * Returns: (transfer none): The index name
 */
const gchar *
orm_index_info_get_name (OrmIndexInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->name;
}

/**
 * orm_index_info_get_unique:
 * @self: An #OrmIndexInfo
 *
 * Returns: %TRUE if the index enforces uniqueness
 */
gboolean
orm_index_info_get_unique (OrmIndexInfo *self)
{
    g_return_val_if_fail (self != NULL, FALSE);
    return self->unique;
}

/**
 * orm_index_info_get_columns:
 * @self: An #OrmIndexInfo
 *
 * Returns: (transfer none) (array zero-terminated=1): The indexed columns
 */
const gchar * const *
orm_index_info_get_columns (OrmIndexInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return (const gchar * const *) self->columns;
}

/* ---------------------------------------------------------------- */
/* OrmForeignKeyInfo                                                */
/* ---------------------------------------------------------------- */

struct _OrmForeignKeyInfo
{
    gatomicrefcount      ref_count;

    gchar               *name;
    gchar              **columns;
    gchar               *ref_table;
    gchar               *ref_schema;
    gchar              **ref_columns;
    OrmForeignKeyAction  on_delete;
    OrmForeignKeyAction  on_update;
};

static void
orm_foreign_key_info_free (OrmForeignKeyInfo *self)
{
    g_free (self->name);
    g_strfreev (self->columns);
    g_free (self->ref_table);
    g_free (self->ref_schema);
    g_strfreev (self->ref_columns);
    g_free (self);
}

G_DEFINE_BOXED_TYPE (OrmForeignKeyInfo, orm_foreign_key_info,
                     orm_foreign_key_info_ref, orm_foreign_key_info_unref)

/**
 * orm_foreign_key_info_new:
 * @name: (nullable): The constraint name, where the backend gives one
 * @columns: (array zero-terminated=1): The referring columns
 * @ref_table: The referenced table
 * @ref_schema: (nullable): The referenced table's schema
 * @ref_columns: (array zero-terminated=1): The referenced columns
 * @on_delete: What happens to referring rows when a referenced row goes
 * @on_update: What happens when a referenced key changes
 *
 * @columns and @ref_columns are positionally paired, so the nth referring
 * column points at the nth referenced one. A composite key is why both
 * are arrays rather than single names.
 *
 * Returns: (transfer full): A new #OrmForeignKeyInfo
 */
OrmForeignKeyInfo *
orm_foreign_key_info_new (const gchar         *name,
                          const gchar *const  *columns,
                          const gchar         *ref_table,
                          const gchar         *ref_schema,
                          const gchar *const  *ref_columns,
                          OrmForeignKeyAction  on_delete,
                          OrmForeignKeyAction  on_update)
{
    OrmForeignKeyInfo *self;

    g_return_val_if_fail (ref_table != NULL, NULL);

    self = g_new0 (OrmForeignKeyInfo, 1);
    g_atomic_ref_count_init (&self->ref_count);
    self->name = g_strdup (name);
    self->columns = (columns != NULL)
        ? g_strdupv ((gchar **) columns)
        : g_new0 (gchar *, 1);
    self->ref_table = g_strdup (ref_table);
    self->ref_schema = g_strdup (ref_schema);
    self->ref_columns = (ref_columns != NULL)
        ? g_strdupv ((gchar **) ref_columns)
        : g_new0 (gchar *, 1);
    self->on_delete = on_delete;
    self->on_update = on_update;

    return self;
}

/**
 * orm_foreign_key_info_ref:
 * @self: An #OrmForeignKeyInfo
 *
 * Returns: (transfer full): @self
 */
OrmForeignKeyInfo *
orm_foreign_key_info_ref (OrmForeignKeyInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);

    g_atomic_ref_count_inc (&self->ref_count);
    return self;
}

/**
 * orm_foreign_key_info_unref:
 * @self: An #OrmForeignKeyInfo
 *
 * Drops a reference, freeing @self when the last one goes.
 */
void
orm_foreign_key_info_unref (OrmForeignKeyInfo *self)
{
    g_return_if_fail (self != NULL);

    if (g_atomic_ref_count_dec (&self->ref_count))
        orm_foreign_key_info_free (self);
}

/**
 * orm_foreign_key_info_get_name:
 * @self: An #OrmForeignKeyInfo
 *
 * Gets the constraint name.  SQLite does not name foreign keys, so this
 * is %NULL there.
 *
 * Returns: (transfer none) (nullable): The name, or %NULL
 */
const gchar *
orm_foreign_key_info_get_name (OrmForeignKeyInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->name;
}

/**
 * orm_foreign_key_info_get_columns:
 * @self: An #OrmForeignKeyInfo
 *
 * Returns: (transfer none) (array zero-terminated=1): The referring columns
 */
const gchar * const *
orm_foreign_key_info_get_columns (OrmForeignKeyInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return (const gchar * const *) self->columns;
}

/**
 * orm_foreign_key_info_get_ref_table:
 * @self: An #OrmForeignKeyInfo
 *
 * Returns: (transfer none): The referenced table
 */
const gchar *
orm_foreign_key_info_get_ref_table (OrmForeignKeyInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->ref_table;
}

/**
 * orm_foreign_key_info_get_ref_schema:
 * @self: An #OrmForeignKeyInfo
 *
 * Returns: (transfer none) (nullable): The referenced schema, or %NULL
 */
const gchar *
orm_foreign_key_info_get_ref_schema (OrmForeignKeyInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->ref_schema;
}

/**
 * orm_foreign_key_info_get_ref_columns:
 * @self: An #OrmForeignKeyInfo
 *
 * Returns: (transfer none) (array zero-terminated=1): The referenced columns
 */
const gchar * const *
orm_foreign_key_info_get_ref_columns (OrmForeignKeyInfo *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return (const gchar * const *) self->ref_columns;
}

/**
 * orm_foreign_key_info_get_on_delete:
 * @self: An #OrmForeignKeyInfo
 *
 * Returns: The ON DELETE action
 */
OrmForeignKeyAction
orm_foreign_key_info_get_on_delete (OrmForeignKeyInfo *self)
{
    g_return_val_if_fail (self != NULL, ORM_FK_NO_ACTION);
    return self->on_delete;
}

/**
 * orm_foreign_key_info_get_on_update:
 * @self: An #OrmForeignKeyInfo
 *
 * Returns: The ON UPDATE action
 */
OrmForeignKeyAction
orm_foreign_key_info_get_on_update (OrmForeignKeyInfo *self)
{
    g_return_val_if_fail (self != NULL, ORM_FK_NO_ACTION);
    return self->on_update;
}
