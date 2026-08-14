/* orm-value.c
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

#include "orm-value.h"

/*
 * Internal structure for OrmValue.
 * Uses a tagged union to store different value types efficiently.
 */
struct _OrmValue
{
    OrmValueType type;
    union {
        gint64      v_integer;
        gdouble     v_float;
        gchar      *v_string;
        GBytes     *v_blob;
        gboolean    v_boolean;
        GDateTime  *v_datetime;
    } data;
};

/*
 * GType registration for OrmValue boxed type.
 */
G_DEFINE_BOXED_TYPE (OrmValue, orm_value, orm_value_copy, orm_value_free)

/*
 * Helper function to allocate a new OrmValue structure.
 */
static OrmValue *
orm_value_alloc (OrmValueType type)
{
    OrmValue *value;

    value = g_slice_new0 (OrmValue);
    value->type = type;

    return value;
}

/**
 * orm_value_new_null:
 *
 * Creates a new NULL database value.
 *
 * Returns: (transfer full): A new #OrmValue representing NULL
 */
OrmValue *
orm_value_new_null (void)
{
    return orm_value_alloc (ORM_VALUE_NULL);
}

/**
 * orm_value_new_integer:
 * @value: The integer value
 *
 * Creates a new integer database value.
 *
 * Returns: (transfer full): A new #OrmValue containing the integer
 */
OrmValue *
orm_value_new_integer (gint64 value)
{
    OrmValue *v;

    v = orm_value_alloc (ORM_VALUE_INTEGER);
    v->data.v_integer = value;

    return v;
}

/**
 * orm_value_new_float:
 * @value: The floating point value
 *
 * Creates a new floating point database value.
 *
 * Returns: (transfer full): A new #OrmValue containing the float
 */
OrmValue *
orm_value_new_float (gdouble value)
{
    OrmValue *v;

    v = orm_value_alloc (ORM_VALUE_FLOAT);
    v->data.v_float = value;

    return v;
}

/**
 * orm_value_new_string:
 * @value: (nullable): The string value
 *
 * Creates a new string database value. If @value is %NULL,
 * creates a NULL value instead.
 *
 * Returns: (transfer full): A new #OrmValue containing the string
 */
OrmValue *
orm_value_new_string (const gchar *value)
{
    OrmValue *v;

    if (value == NULL)
    {
        return orm_value_new_null ();
    }

    v = orm_value_alloc (ORM_VALUE_STRING);
    v->data.v_string = g_strdup (value);

    return v;
}

/**
 * orm_value_new_blob:
 * @value: (nullable): The binary data
 *
 * Creates a new binary database value. If @value is %NULL,
 * creates a NULL value instead.
 *
 * Returns: (transfer full): A new #OrmValue containing the binary data
 */
OrmValue *
orm_value_new_blob (GBytes *value)
{
    OrmValue *v;

    if (value == NULL)
    {
        return orm_value_new_null ();
    }

    v = orm_value_alloc (ORM_VALUE_BLOB);
    v->data.v_blob = g_bytes_ref (value);

    return v;
}

/**
 * orm_value_new_boolean:
 * @value: The boolean value
 *
 * Creates a new boolean database value.
 *
 * Returns: (transfer full): A new #OrmValue containing the boolean
 */
OrmValue *
orm_value_new_boolean (gboolean value)
{
    OrmValue *v;

    v = orm_value_alloc (ORM_VALUE_BOOLEAN);
    v->data.v_boolean = value;

    return v;
}

/**
 * orm_value_new_datetime:
 * @value: (nullable): The datetime value
 *
 * Creates a new datetime database value. If @value is %NULL,
 * creates a NULL value instead.
 *
 * Returns: (transfer full): A new #OrmValue containing the datetime
 */
OrmValue *
orm_value_new_datetime (GDateTime *value)
{
    OrmValue *v;

    if (value == NULL)
    {
        return orm_value_new_null ();
    }

    v = orm_value_alloc (ORM_VALUE_DATETIME);
    v->data.v_datetime = g_date_time_ref (value);

    return v;
}

/**
 * orm_value_copy:
 * @value: (nullable): An #OrmValue to copy
 *
 * Creates a deep copy of an #OrmValue.
 *
 * Returns: (transfer full) (nullable): A copy of @value, or %NULL if @value is %NULL
 */
OrmValue *
orm_value_copy (const OrmValue *value)
{
    if (value == NULL)
    {
        return NULL;
    }

    switch (value->type)
    {
    case ORM_VALUE_NULL:
        return orm_value_new_null ();
    case ORM_VALUE_INTEGER:
        return orm_value_new_integer (value->data.v_integer);
    case ORM_VALUE_FLOAT:
        return orm_value_new_float (value->data.v_float);
    case ORM_VALUE_STRING:
        return orm_value_new_string (value->data.v_string);
    case ORM_VALUE_BLOB:
        return orm_value_new_blob (value->data.v_blob);
    case ORM_VALUE_BOOLEAN:
        return orm_value_new_boolean (value->data.v_boolean);
    case ORM_VALUE_DATETIME:
        return orm_value_new_datetime (value->data.v_datetime);
    default:
        g_assert_not_reached ();
        return NULL;
    }
}

/**
 * orm_value_free:
 * @value: (nullable): An #OrmValue to free
 *
 * Frees an #OrmValue and any associated memory.
 */
void
orm_value_free (OrmValue *value)
{
    if (value == NULL)
    {
        return;
    }

    switch (value->type)
    {
    case ORM_VALUE_STRING:
        g_free (value->data.v_string);
        break;
    case ORM_VALUE_BLOB:
        g_bytes_unref (value->data.v_blob);
        break;
    case ORM_VALUE_DATETIME:
        g_date_time_unref (value->data.v_datetime);
        break;
    default:
        break;
    }

    g_slice_free (OrmValue, value);
}

/**
 * orm_value_get_value_type:
 * @value: An #OrmValue
 *
 * Gets the type of value stored in the #OrmValue.
 *
 * Returns: The #OrmValueType of the value
 */
OrmValueType
orm_value_get_value_type (const OrmValue *value)
{
    g_return_val_if_fail (value != NULL, ORM_VALUE_NULL);

    return value->type;
}

/**
 * orm_value_is_null:
 * @value: An #OrmValue
 *
 * Checks if the value is NULL.
 *
 * Returns: %TRUE if the value is NULL, %FALSE otherwise
 */
gboolean
orm_value_is_null (const OrmValue *value)
{
    g_return_val_if_fail (value != NULL, TRUE);

    return value->type == ORM_VALUE_NULL;
}

/**
 * orm_value_get_integer:
 * @value: An #OrmValue containing an integer
 *
 * Gets the integer value. The value must be of type %ORM_VALUE_INTEGER.
 *
 * Returns: The integer value
 */
gint64
orm_value_get_integer (const OrmValue *value)
{
    g_return_val_if_fail (value != NULL, 0);
    g_return_val_if_fail (value->type == ORM_VALUE_INTEGER, 0);

    return value->data.v_integer;
}

/**
 * orm_value_get_float:
 * @value: An #OrmValue containing a float
 *
 * Gets the floating point value. The value must be of type %ORM_VALUE_FLOAT.
 *
 * Returns: The floating point value
 */
gdouble
orm_value_get_float (const OrmValue *value)
{
    g_return_val_if_fail (value != NULL, 0.0);
    g_return_val_if_fail (value->type == ORM_VALUE_FLOAT, 0.0);

    return value->data.v_float;
}

/**
 * orm_value_get_string:
 * @value: An #OrmValue containing a string
 *
 * Gets the string value. The value must be of type %ORM_VALUE_STRING.
 *
 * Returns: (transfer none): The string value
 */
const gchar *
orm_value_get_string (const OrmValue *value)
{
    g_return_val_if_fail (value != NULL, NULL);
    g_return_val_if_fail (value->type == ORM_VALUE_STRING, NULL);

    return value->data.v_string;
}

/**
 * orm_value_get_blob:
 * @value: An #OrmValue containing binary data
 *
 * Gets the binary data value. The value must be of type %ORM_VALUE_BLOB.
 *
 * Returns: (transfer none): The binary data
 */
GBytes *
orm_value_get_blob (const OrmValue *value)
{
    g_return_val_if_fail (value != NULL, NULL);
    g_return_val_if_fail (value->type == ORM_VALUE_BLOB, NULL);

    return value->data.v_blob;
}

/**
 * orm_value_get_boolean:
 * @value: An #OrmValue containing a boolean
 *
 * Gets the boolean value. The value must be of type %ORM_VALUE_BOOLEAN.
 *
 * Returns: The boolean value
 */
gboolean
orm_value_get_boolean (const OrmValue *value)
{
    g_return_val_if_fail (value != NULL, FALSE);

    /*
     * SQLite has no boolean storage class: a value written as a boolean
     * comes back as an integer 0 or 1, and MySQL's BOOL is likewise an
     * alias for TINYINT. Asserting on the type here made it impossible to
     * read back any boolean column on those backends, so an integer is
     * accepted and interpreted the way every SQL engine does.
     */
    if (value->type == ORM_VALUE_INTEGER)
    {
        return value->data.v_integer != 0;
    }

    /*
     * PostgreSQL renders booleans as "t"/"f" over the text protocol, and
     * some drivers hand back "true"/"false" or "1"/"0".
     */
    if (value->type == ORM_VALUE_STRING)
    {
        const gchar *text = value->data.v_string;

        if (text == NULL)
        {
            return FALSE;
        }

        return (g_ascii_strcasecmp (text, "t") == 0) ||
               (g_ascii_strcasecmp (text, "true") == 0) ||
               (g_ascii_strcasecmp (text, "y") == 0) ||
               (g_ascii_strcasecmp (text, "yes") == 0) ||
               (g_strcmp0 (text, "1") == 0);
    }

    g_return_val_if_fail (value->type == ORM_VALUE_BOOLEAN, FALSE);

    return value->data.v_boolean;
}

/**
 * orm_value_get_datetime:
 * @value: An #OrmValue containing a datetime
 *
 * Gets the datetime value. The value must be of type %ORM_VALUE_DATETIME.
 *
 * Returns: (transfer none): The datetime value
 */
GDateTime *
orm_value_get_datetime (const OrmValue *value)
{
    g_return_val_if_fail (value != NULL, NULL);
    g_return_val_if_fail (value->type == ORM_VALUE_DATETIME, NULL);

    return value->data.v_datetime;
}

/*
 * Helper to safely initialize a GValue, unsetting it first if needed.
 */
static void
safe_gvalue_init (GValue *gvalue,
                  GType   type)
{
    if (G_VALUE_TYPE (gvalue) != G_TYPE_INVALID)
    {
        g_value_unset (gvalue);
    }
    g_value_init (gvalue, type);
}

/**
 * orm_value_to_gvalue:
 * @value: An #OrmValue
 * @gvalue: (out): A #GValue to fill
 *
 * Converts an #OrmValue to a #GValue. The @gvalue may be
 * uninitialized, zeroed, or already initialized - this function
 * handles all cases safely by unsetting before re-initializing.
 *
 * Returns: %TRUE on success, %FALSE if conversion failed
 */
gboolean
orm_value_to_gvalue (const OrmValue *value,
                     GValue         *gvalue)
{
    g_return_val_if_fail (value != NULL, FALSE);
    g_return_val_if_fail (gvalue != NULL, FALSE);

    switch (value->type)
    {
    case ORM_VALUE_NULL:
        /* GValue has no direct NULL type, unset if initialized */
        if (G_VALUE_TYPE (gvalue) != G_TYPE_INVALID)
        {
            g_value_unset (gvalue);
        }
        return TRUE;

    case ORM_VALUE_INTEGER:
        safe_gvalue_init (gvalue, G_TYPE_INT64);
        g_value_set_int64 (gvalue, value->data.v_integer);
        return TRUE;

    case ORM_VALUE_FLOAT:
        safe_gvalue_init (gvalue, G_TYPE_DOUBLE);
        g_value_set_double (gvalue, value->data.v_float);
        return TRUE;

    case ORM_VALUE_STRING:
        safe_gvalue_init (gvalue, G_TYPE_STRING);
        g_value_set_string (gvalue, value->data.v_string);
        return TRUE;

    case ORM_VALUE_BLOB:
        safe_gvalue_init (gvalue, G_TYPE_BYTES);
        g_value_set_boxed (gvalue, value->data.v_blob);
        return TRUE;

    case ORM_VALUE_BOOLEAN:
        safe_gvalue_init (gvalue, G_TYPE_BOOLEAN);
        g_value_set_boolean (gvalue, value->data.v_boolean);
        return TRUE;

    case ORM_VALUE_DATETIME:
        safe_gvalue_init (gvalue, G_TYPE_DATE_TIME);
        g_value_set_boxed (gvalue, value->data.v_datetime);
        return TRUE;

    default:
        return FALSE;
    }
}

/**
 * orm_value_to_gvalue_with_type:
 * @value: An #OrmValue
 * @gvalue: (out): A #GValue to fill
 * @target_type: The desired GType for the output GValue
 *
 * Converts an #OrmValue to a #GValue with a specific target type.
 * This performs type coercion when possible (e.g., INTEGER to INT,
 * INTEGER to UINT, STRING to STRING).
 *
 * Returns: %TRUE on success, %FALSE if conversion failed
 */
gboolean
orm_value_to_gvalue_with_type (const OrmValue *value,
                               GValue         *gvalue,
                               GType           target_type)
{
    g_return_val_if_fail (value != NULL, FALSE);
    g_return_val_if_fail (gvalue != NULL, FALSE);

    /* Handle NULL value */
    if (value->type == ORM_VALUE_NULL)
    {
        if (G_VALUE_TYPE (gvalue) != G_TYPE_INVALID)
        {
            g_value_unset (gvalue);
        }
        return TRUE;
    }

    /* Unset existing value if any */
    if (G_VALUE_TYPE (gvalue) != G_TYPE_INVALID)
    {
        g_value_unset (gvalue);
    }
    g_value_init (gvalue, target_type);

    /* Convert based on target type */
    if (target_type == G_TYPE_INT && value->type == ORM_VALUE_INTEGER)
    {
        g_value_set_int (gvalue, (gint) value->data.v_integer);
        return TRUE;
    }
    else if (target_type == G_TYPE_UINT && value->type == ORM_VALUE_INTEGER)
    {
        g_value_set_uint (gvalue, (guint) value->data.v_integer);
        return TRUE;
    }
    else if (target_type == G_TYPE_INT64 && value->type == ORM_VALUE_INTEGER)
    {
        g_value_set_int64 (gvalue, value->data.v_integer);
        return TRUE;
    }
    else if (target_type == G_TYPE_UINT64 && value->type == ORM_VALUE_INTEGER)
    {
        g_value_set_uint64 (gvalue, (guint64) value->data.v_integer);
        return TRUE;
    }
    else if (target_type == G_TYPE_LONG && value->type == ORM_VALUE_INTEGER)
    {
        g_value_set_long (gvalue, (glong) value->data.v_integer);
        return TRUE;
    }
    else if (target_type == G_TYPE_ULONG && value->type == ORM_VALUE_INTEGER)
    {
        g_value_set_ulong (gvalue, (gulong) value->data.v_integer);
        return TRUE;
    }
    else if (target_type == G_TYPE_FLOAT && value->type == ORM_VALUE_FLOAT)
    {
        g_value_set_float (gvalue, (gfloat) value->data.v_float);
        return TRUE;
    }
    else if (target_type == G_TYPE_DOUBLE && value->type == ORM_VALUE_FLOAT)
    {
        g_value_set_double (gvalue, value->data.v_float);
        return TRUE;
    }
    else if (target_type == G_TYPE_STRING && value->type == ORM_VALUE_STRING)
    {
        g_value_set_string (gvalue, value->data.v_string);
        return TRUE;
    }
    else if (target_type == G_TYPE_BOOLEAN && value->type == ORM_VALUE_BOOLEAN)
    {
        g_value_set_boolean (gvalue, value->data.v_boolean);
        return TRUE;
    }
    else if (target_type == G_TYPE_BYTES && value->type == ORM_VALUE_BLOB)
    {
        g_value_set_boxed (gvalue, value->data.v_blob);
        return TRUE;
    }
    else if (target_type == G_TYPE_DATE_TIME && value->type == ORM_VALUE_DATETIME)
    {
        g_value_set_boxed (gvalue, value->data.v_datetime);
        return TRUE;
    }
    /* String to datetime coercion (ISO8601 parsing) */
    else if (target_type == G_TYPE_DATE_TIME && value->type == ORM_VALUE_STRING)
    {
        GDateTime *dt = NULL;
        const gchar *str = value->data.v_string;

        if (str != NULL && str[0] != '\0')
        {
            /* Try parsing as ISO8601 */
            dt = g_date_time_new_from_iso8601 (str, NULL);
        }

        if (dt != NULL)
        {
            g_value_take_boxed (gvalue, dt);
            return TRUE;
        }

        /* If parsing fails, return NULL datetime */
        g_value_set_boxed (gvalue, NULL);
        return TRUE;
    }
    /* Integer to boolean coercion */
    else if (target_type == G_TYPE_BOOLEAN && value->type == ORM_VALUE_INTEGER)
    {
        g_value_set_boolean (gvalue, value->data.v_integer != 0);
        return TRUE;
    }
    /* Float to integer coercion */
    else if ((target_type == G_TYPE_INT || target_type == G_TYPE_INT64) &&
             value->type == ORM_VALUE_FLOAT)
    {
        if (target_type == G_TYPE_INT)
            g_value_set_int (gvalue, (gint) value->data.v_float);
        else
            g_value_set_int64 (gvalue, (gint64) value->data.v_float);
        return TRUE;
    }

    /* Fallback: try standard conversion */
    g_value_unset (gvalue);
    return orm_value_to_gvalue (value, gvalue);
}

/**
 * orm_value_from_gvalue:
 * @gvalue: A #GValue to convert
 *
 * Creates an #OrmValue from a #GValue. Supports common types
 * including integers, floats, strings, booleans, and datetime.
 *
 * Returns: (transfer full) (nullable): A new #OrmValue, or %NULL if
 *          the type is not supported
 */
OrmValue *
orm_value_from_gvalue (const GValue *gvalue)
{
    GType type;

    g_return_val_if_fail (gvalue != NULL, NULL);

    /* Handle uninitialized GValue as NULL */
    if (!G_IS_VALUE (gvalue))
    {
        return orm_value_new_null ();
    }

    type = G_VALUE_TYPE (gvalue);

    /* Integer types */
    if (type == G_TYPE_INT)
    {
        return orm_value_new_integer ((gint64) g_value_get_int (gvalue));
    }
    else if (type == G_TYPE_UINT)
    {
        return orm_value_new_integer ((gint64) g_value_get_uint (gvalue));
    }
    else if (type == G_TYPE_INT64)
    {
        return orm_value_new_integer (g_value_get_int64 (gvalue));
    }
    else if (type == G_TYPE_UINT64)
    {
        return orm_value_new_integer ((gint64) g_value_get_uint64 (gvalue));
    }
    else if (type == G_TYPE_LONG)
    {
        return orm_value_new_integer ((gint64) g_value_get_long (gvalue));
    }
    else if (type == G_TYPE_ULONG)
    {
        return orm_value_new_integer ((gint64) g_value_get_ulong (gvalue));
    }
    /* Float types */
    else if (type == G_TYPE_FLOAT)
    {
        return orm_value_new_float ((gdouble) g_value_get_float (gvalue));
    }
    else if (type == G_TYPE_DOUBLE)
    {
        return orm_value_new_float (g_value_get_double (gvalue));
    }
    /* String */
    else if (type == G_TYPE_STRING)
    {
        return orm_value_new_string (g_value_get_string (gvalue));
    }
    /* Boolean */
    else if (type == G_TYPE_BOOLEAN)
    {
        return orm_value_new_boolean (g_value_get_boolean (gvalue));
    }
    /* Boxed types */
    else if (type == G_TYPE_BYTES)
    {
        return orm_value_new_blob ((GBytes *) g_value_get_boxed (gvalue));
    }
    else if (type == G_TYPE_DATE_TIME)
    {
        return orm_value_new_datetime ((GDateTime *) g_value_get_boxed (gvalue));
    }

    /* Unsupported type */
    return NULL;
}

/**
 * orm_value_to_string:
 * @value: An #OrmValue
 *
 * Creates a string representation of the value. This is useful
 * for debugging and logging purposes.
 *
 * Returns: (transfer full): A newly allocated string
 */
gchar *
orm_value_to_string (const OrmValue *value)
{
    g_return_val_if_fail (value != NULL, g_strdup ("(invalid)"));

    switch (value->type)
    {
    case ORM_VALUE_NULL:
        return g_strdup ("NULL");

    case ORM_VALUE_INTEGER:
        return g_strdup_printf ("%" G_GINT64_FORMAT, value->data.v_integer);

    case ORM_VALUE_FLOAT:
        return g_strdup_printf ("%g", value->data.v_float);

    case ORM_VALUE_STRING:
        return g_strdup_printf ("'%s'", value->data.v_string);

    case ORM_VALUE_BLOB:
        {
            gsize size;
            size = g_bytes_get_size (value->data.v_blob);
            return g_strdup_printf ("<blob: %zu bytes>", size);
        }

    case ORM_VALUE_BOOLEAN:
        return g_strdup (value->data.v_boolean ? "TRUE" : "FALSE");

    case ORM_VALUE_DATETIME:
        return g_date_time_format_iso8601 (value->data.v_datetime);

    default:
        return g_strdup ("(unknown)");
    }
}

/**
 * orm_value_compare:
 * @a: First #OrmValue
 * @b: Second #OrmValue
 *
 * Compares two #OrmValue instances. NULL values are considered less
 * than non-NULL values. Values of different types are compared by
 * their type enum value.
 *
 * Returns: negative if @a < @b, 0 if equal, positive if @a > @b
 */
gint
orm_value_compare (const OrmValue *a,
                   const OrmValue *b)
{
    if (a == b)
    {
        return 0;
    }

    if (a == NULL)
    {
        return -1;
    }

    if (b == NULL)
    {
        return 1;
    }

    /* Different types - compare by type */
    if (a->type != b->type)
    {
        return (gint) a->type - (gint) b->type;
    }

    /* Same type - compare values */
    switch (a->type)
    {
    case ORM_VALUE_NULL:
        return 0;

    case ORM_VALUE_INTEGER:
        if (a->data.v_integer < b->data.v_integer)
            return -1;
        if (a->data.v_integer > b->data.v_integer)
            return 1;
        return 0;

    case ORM_VALUE_FLOAT:
        if (a->data.v_float < b->data.v_float)
            return -1;
        if (a->data.v_float > b->data.v_float)
            return 1;
        return 0;

    case ORM_VALUE_STRING:
        return g_strcmp0 (a->data.v_string, b->data.v_string);

    case ORM_VALUE_BLOB:
        return g_bytes_compare (a->data.v_blob, b->data.v_blob);

    case ORM_VALUE_BOOLEAN:
        return (gint) a->data.v_boolean - (gint) b->data.v_boolean;

    case ORM_VALUE_DATETIME:
        return g_date_time_compare (a->data.v_datetime, b->data.v_datetime);

    default:
        return 0;
    }
}

/**
 * orm_value_equal:
 * @a: First #OrmValue
 * @b: Second #OrmValue
 *
 * Checks if two #OrmValue instances are equal.
 *
 * Returns: %TRUE if the values are equal, %FALSE otherwise
 */
gboolean
orm_value_equal (const OrmValue *a,
                 const OrmValue *b)
{
    return orm_value_compare (a, b) == 0;
}

/**
 * orm_value_hash:
 * @value: An #OrmValue
 *
 * Computes a hash value for an #OrmValue. This can be used for
 * hash tables.
 *
 * Returns: A hash value
 */
guint
orm_value_hash (const OrmValue *value)
{
    if (value == NULL || value->type == ORM_VALUE_NULL)
    {
        return 0;
    }

    switch (value->type)
    {
    case ORM_VALUE_INTEGER:
        return g_int64_hash (&value->data.v_integer);

    case ORM_VALUE_FLOAT:
        return g_double_hash (&value->data.v_float);

    case ORM_VALUE_STRING:
        return g_str_hash (value->data.v_string);

    case ORM_VALUE_BLOB:
        return g_bytes_hash (value->data.v_blob);

    case ORM_VALUE_BOOLEAN:
        return value->data.v_boolean ? 1 : 0;

    case ORM_VALUE_DATETIME:
        {
            gint64 unix_time;
            unix_time = g_date_time_to_unix (value->data.v_datetime);
            return g_int64_hash (&unix_time);
        }

    default:
        return 0;
    }
}
