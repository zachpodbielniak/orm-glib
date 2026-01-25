/* orm-literal.c
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

#include "orm-literal.h"

/*
 * OrmLiteral - Literal value expression.
 *
 * Represents a constant value in SQL. When compiling:
 * - Without params: The value is rendered inline in SQL
 * - With params: A placeholder (?) is used and value added to params list
 */

struct _OrmLiteral
{
    OrmExpression parent_instance;

    OrmValue *value;
};

G_DEFINE_TYPE (OrmLiteral, orm_literal, ORM_TYPE_EXPRESSION)

/*
 * Escape a string for SQL.
 * Doubles single quotes: ' -> ''
 */
static gchar *
escape_string (const gchar *str)
{
    GString *result;
    const gchar *p;

    result = g_string_new ("'");

    for (p = str; *p != '\0'; p++)
    {
        if (*p == '\'')
        {
            g_string_append (result, "''");
        }
        else
        {
            g_string_append_c (result, *p);
        }
    }

    g_string_append_c (result, '\'');
    return g_string_free (result, FALSE);
}

/*
 * Compile the literal to SQL.
 */
static gchar *
orm_literal_compile_impl (OrmExpression  *expr,
                          OrmDialectType  dialect,
                          GList         **params)
{
    OrmLiteral *self = ORM_LITERAL (expr);
    OrmValueType vtype;

    (void) dialect;

    /* If params list provided, use parameterized query */
    if (params != NULL)
    {
        *params = g_list_append (*params, orm_value_copy (self->value));
        return g_strdup ("?");
    }

    /* Otherwise, render inline */
    vtype = orm_value_get_value_type (self->value);

    switch (vtype)
    {
    case ORM_VALUE_NULL:
        return g_strdup ("NULL");

    case ORM_VALUE_INTEGER:
        return g_strdup_printf ("%" G_GINT64_FORMAT,
                                orm_value_get_integer (self->value));

    case ORM_VALUE_FLOAT:
        return g_strdup_printf ("%g", orm_value_get_float (self->value));

    case ORM_VALUE_STRING:
        return escape_string (orm_value_get_string (self->value));

    case ORM_VALUE_BOOLEAN:
        /* Most databases use 1/0 for boolean, but some use TRUE/FALSE */
        return g_strdup (orm_value_get_boolean (self->value) ? "1" : "0");

    case ORM_VALUE_DATETIME:
        {
            GDateTime *dt = orm_value_get_datetime (self->value);
            g_autofree gchar *iso = g_date_time_format_iso8601 (dt);
            return escape_string (iso);
        }

    case ORM_VALUE_BLOB:
        {
            /* Render as hex literal X'...' */
            GBytes *bytes = orm_value_get_blob (self->value);
            gsize size;
            const guint8 *data;
            GString *hex;
            gsize i;

            data = g_bytes_get_data (bytes, &size);
            hex = g_string_new ("X'");

            for (i = 0; i < size; i++)
            {
                g_string_append_printf (hex, "%02X", data[i]);
            }

            g_string_append_c (hex, '\'');
            return g_string_free (hex, FALSE);
        }

    default:
        return g_strdup ("NULL");
    }
}

/*
 * Clone the literal.
 */
static OrmExpression *
orm_literal_clone_impl (OrmExpression *expr)
{
    OrmLiteral *self = ORM_LITERAL (expr);
    return ORM_EXPRESSION (orm_literal_new (orm_value_copy (self->value)));
}

static void
orm_literal_finalize (GObject *object)
{
    OrmLiteral *self = ORM_LITERAL (object);

    g_clear_pointer (&self->value, orm_value_free);

    G_OBJECT_CLASS (orm_literal_parent_class)->finalize (object);
}

static void
orm_literal_class_init (OrmLiteralClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    OrmExpressionClass *expr_class = ORM_EXPRESSION_CLASS (klass);

    object_class->finalize = orm_literal_finalize;

    expr_class->compile = orm_literal_compile_impl;
    expr_class->clone = orm_literal_clone_impl;
}

static void
orm_literal_init (OrmLiteral *self)
{
    self->value = NULL;
}

/**
 * orm_literal_new:
 * @value: (transfer full): The value
 *
 * Creates a new literal expression from an OrmValue.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral *
orm_literal_new (OrmValue *value)
{
    OrmLiteral *self;

    g_return_val_if_fail (value != NULL, NULL);

    self = g_object_new (ORM_TYPE_LITERAL, NULL);
    self->value = value;

    return self;
}

/**
 * orm_literal_new_null:
 *
 * Creates a new NULL literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral *
orm_literal_new_null (void)
{
    return orm_literal_new (orm_value_new_null ());
}

/**
 * orm_literal_new_integer:
 * @value: Integer value
 *
 * Creates a new integer literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral *
orm_literal_new_integer (gint64 value)
{
    return orm_literal_new (orm_value_new_integer (value));
}

/**
 * orm_literal_new_float:
 * @value: Float value
 *
 * Creates a new floating point literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral *
orm_literal_new_float (gdouble value)
{
    return orm_literal_new (orm_value_new_float (value));
}

/**
 * orm_literal_new_string:
 * @value: String value
 *
 * Creates a new string literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral *
orm_literal_new_string (const gchar *value)
{
    g_return_val_if_fail (value != NULL, NULL);
    return orm_literal_new (orm_value_new_string (value));
}

/**
 * orm_literal_new_boolean:
 * @value: Boolean value
 *
 * Creates a new boolean literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral *
orm_literal_new_boolean (gboolean value)
{
    return orm_literal_new (orm_value_new_boolean (value));
}

/**
 * orm_literal_new_datetime:
 * @value: DateTime value
 *
 * Creates a new datetime literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral *
orm_literal_new_datetime (GDateTime *value)
{
    g_return_val_if_fail (value != NULL, NULL);
    return orm_literal_new (orm_value_new_datetime (value));
}

/**
 * orm_literal_new_blob:
 * @value: Blob value
 *
 * Creates a new blob literal.
 *
 * Returns: (transfer full): A new #OrmLiteral
 */
OrmLiteral *
orm_literal_new_blob (GBytes *value)
{
    g_return_val_if_fail (value != NULL, NULL);
    return orm_literal_new (orm_value_new_blob (value));
}

/**
 * orm_literal_get_value:
 * @self: A #OrmLiteral
 *
 * Gets the underlying OrmValue.
 *
 * Returns: (transfer none): The value
 */
OrmValue *
orm_literal_get_value (OrmLiteral *self)
{
    g_return_val_if_fail (ORM_IS_LITERAL (self), NULL);
    return self->value;
}

/**
 * orm_literal_get_value_type:
 * @self: A #OrmLiteral
 *
 * Gets the type of the value.
 *
 * Returns: The value type
 */
OrmValueType
orm_literal_get_value_type (OrmLiteral *self)
{
    g_return_val_if_fail (ORM_IS_LITERAL (self), ORM_VALUE_NULL);
    return orm_value_get_value_type (self->value);
}
