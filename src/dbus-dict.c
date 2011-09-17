/*
 * WPA Supplicant / dbus-based control interface
 * Copyright (c) 2006, Dan Williams <dcbw@redhat.com> and Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <wicked/util.h>
#include <dbus/dbus.h>

#include "netinfo_priv.h"
#include "dbus-dict.h"


/**
 * Start a dict in a dbus message.  Should be paired with a call to
 * ni_dbus_dict_close_write().
 *
 * @param iter A valid dbus message iterator
 * @param iter_dict (out) A dict iterator to pass to further dict functions
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_open_write(DBusMessageIter *iter,
				     DBusMessageIter *iter_dict)
{
	dbus_bool_t result;

	if (!iter || !iter_dict)
		return FALSE;

	result = dbus_message_iter_open_container(
		iter,
		DBUS_TYPE_ARRAY,
		DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
		DBUS_TYPE_STRING_AS_STRING
		DBUS_TYPE_VARIANT_AS_STRING
		DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
		iter_dict);
	return result;
}


/**
 * End a dict element in a dbus message.  Should be paired with
 * a call to ni_dbus_dict_open_write().
 *
 * @param iter valid dbus message iterator, same as passed to
 *    ni_dbus_dict_open_write()
 * @param iter_dict a dbus dict iterator returned from
 *    ni_dbus_dict_open_write()
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_close_write(DBusMessageIter *iter,
				      DBusMessageIter *iter_dict)
{
	if (!iter || !iter_dict)
		return FALSE;

	return dbus_message_iter_close_container(iter, iter_dict);
}


static const char * __ni_dbus_basic_type_as_string[256] = {
[DBUS_TYPE_BYTE]	= DBUS_TYPE_BYTE_AS_STRING,
[DBUS_TYPE_BOOLEAN]	= DBUS_TYPE_BOOLEAN_AS_STRING,
[DBUS_TYPE_INT16]	= DBUS_TYPE_INT16_AS_STRING,
[DBUS_TYPE_UINT16]	= DBUS_TYPE_UINT16_AS_STRING,
[DBUS_TYPE_INT32]	= DBUS_TYPE_INT32_AS_STRING,
[DBUS_TYPE_UINT32]	= DBUS_TYPE_UINT32_AS_STRING,
[DBUS_TYPE_INT64]	= DBUS_TYPE_INT64_AS_STRING,
[DBUS_TYPE_UINT64]	= DBUS_TYPE_UINT64_AS_STRING,
[DBUS_TYPE_DOUBLE]	= DBUS_TYPE_DOUBLE_AS_STRING,
[DBUS_TYPE_STRING]	= DBUS_TYPE_STRING_AS_STRING,
[DBUS_TYPE_OBJECT_PATH]	= DBUS_TYPE_OBJECT_PATH_AS_STRING,
};

static const char *
__ni_dbus_get_type_as_string_from_type(const int type)
{
	if (type < 0 || type >= 256)
		return NULL;
	return __ni_dbus_basic_type_as_string[(unsigned int) type];
}

const char *
ni_dbus_variant_signature(const ni_dbus_variant_t *var)
{
	static char buffer[64];
	const char *sig;

	sig = __ni_dbus_get_type_as_string_from_type(var->type);
	if (sig)
		return sig;

	switch (var->type) {
	case DBUS_TYPE_ARRAY:
		strcpy(buffer, DBUS_TYPE_ARRAY_AS_STRING);
		switch (var->array.element_type) {
		case DBUS_TYPE_BYTE:
			return DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_BYTE_AS_STRING;
		}
		break;
	}

	return NULL;
}

static dbus_bool_t __ni_dbus_add_dict_entry_start(
	DBusMessageIter *iter_dict, DBusMessageIter *iter_dict_entry,
	const char *key)
{
	if (!dbus_message_iter_open_container(iter_dict,
					      DBUS_TYPE_DICT_ENTRY, NULL,
					      iter_dict_entry))
		return FALSE;

	if (!dbus_message_iter_append_basic(iter_dict_entry, DBUS_TYPE_STRING,
					    &key))
		return FALSE;

	return TRUE;
}


static dbus_bool_t __ni_dbus_add_dict_entry_end(
	DBusMessageIter *iter_dict, DBusMessageIter *iter_dict_entry,
	DBusMessageIter *iter_dict_val)
{
	if (!dbus_message_iter_close_container(iter_dict_entry, iter_dict_val))
		return FALSE;
	if (!dbus_message_iter_close_container(iter_dict, iter_dict_entry))
		return FALSE;

	return TRUE;
}


static dbus_bool_t __ni_dbus_add_dict_entry_basic(DBusMessageIter *iter_dict,
						  const char *key,
						  const int value_type,
						  const void *value)
{
	DBusMessageIter iter_dict_entry, iter_dict_val;
	const char *type_as_string = NULL;

	type_as_string = __ni_dbus_get_type_as_string_from_type(value_type);
	if (!type_as_string)
		return FALSE;

	if (!__ni_dbus_add_dict_entry_start(iter_dict, &iter_dict_entry, key))
		return FALSE;

	if (!dbus_message_iter_open_container(&iter_dict_entry,
					      DBUS_TYPE_VARIANT,
					      type_as_string, &iter_dict_val))
		return FALSE;

	if (!dbus_message_iter_append_basic(&iter_dict_val, value_type, value))
		return FALSE;

	if (!__ni_dbus_add_dict_entry_end(iter_dict, &iter_dict_entry,
					  &iter_dict_val))
		return FALSE;

	return TRUE;
}

static dbus_bool_t __ni_dbus_add_dict_entry_byte_array(
	DBusMessageIter *iter_dict, const char *key,
	const char *value, const dbus_uint32_t value_len)
{
	DBusMessageIter iter_dict_entry, iter_dict_val, iter_array;
	dbus_uint32_t i;

	if (!__ni_dbus_add_dict_entry_start(iter_dict, &iter_dict_entry, key))
		return FALSE;

	if (!dbus_message_iter_open_container(&iter_dict_entry,
					      DBUS_TYPE_VARIANT,
					      DBUS_TYPE_ARRAY_AS_STRING
					      DBUS_TYPE_BYTE_AS_STRING,
					      &iter_dict_val))
		return FALSE;

	if (!dbus_message_iter_open_container(&iter_dict_val, DBUS_TYPE_ARRAY,
					      DBUS_TYPE_BYTE_AS_STRING,
					      &iter_array))
		return FALSE;

	for (i = 0; i < value_len; i++) {
		if (!dbus_message_iter_append_basic(&iter_array,
						    DBUS_TYPE_BYTE,
						    &(value[i])))
			return FALSE;
	}

	if (!dbus_message_iter_close_container(&iter_dict_val, &iter_array))
		return FALSE;

	if (!__ni_dbus_add_dict_entry_end(iter_dict, &iter_dict_entry,
					  &iter_dict_val))
		return FALSE;

	return TRUE;
}

dbus_bool_t ni_dbus_dict_append_variant(DBusMessageIter *iter_dict,
				      const char *key, const ni_dbus_variant_t *variant)
{
	const void *value;

	value = ni_dbus_variant_datum_const_ptr(variant);
	if (value == NULL)
		return FALSE;

	return __ni_dbus_add_dict_entry_basic(iter_dict, key, variant->type, value);
}

/**
 * Add a string entry to the dict.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param value The string value
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_append_string(DBusMessageIter *iter_dict,
					const char *key, const char *value)
{
	if (!key || !value)
		return FALSE;
	return __ni_dbus_add_dict_entry_basic(iter_dict, key, DBUS_TYPE_STRING,
					      &value);
}


/**
 * Add a byte entry to the dict.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param value The byte value
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_append_byte(DBusMessageIter *iter_dict,
				      const char *key, const char value)
{
	if (!key)
		return FALSE;
	return __ni_dbus_add_dict_entry_basic(iter_dict, key, DBUS_TYPE_BYTE,
					      &value);
}


/**
 * Add a boolean entry to the dict.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param value The boolean value
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_append_bool(DBusMessageIter *iter_dict,
				      const char *key, const dbus_bool_t value)
{
	if (!key)
		return FALSE;
	return __ni_dbus_add_dict_entry_basic(iter_dict, key,
					      DBUS_TYPE_BOOLEAN, &value);
}


/**
 * Add a 16-bit signed integer entry to the dict.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param value The 16-bit signed integer value
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_append_int16(DBusMessageIter *iter_dict,
				       const char *key,
				       const dbus_int16_t value)
{
	if (!key)
		return FALSE;
	return __ni_dbus_add_dict_entry_basic(iter_dict, key, DBUS_TYPE_INT16,
					      &value);
}


/**
 * Add a 16-bit unsigned integer entry to the dict.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param value The 16-bit unsigned integer value
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_append_uint16(DBusMessageIter *iter_dict,
					const char *key,
					const dbus_uint16_t value)
{
	if (!key)
		return FALSE;
	return __ni_dbus_add_dict_entry_basic(iter_dict, key, DBUS_TYPE_UINT16,
					      &value);
}


/**
 * Add a 32-bit signed integer to the dict.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param value The 32-bit signed integer value
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_append_int32(DBusMessageIter *iter_dict,
				       const char *key,
				       const dbus_int32_t value)
{
	if (!key)
		return FALSE;
	return __ni_dbus_add_dict_entry_basic(iter_dict, key, DBUS_TYPE_INT32,
					      &value);
}


/**
 * Add a 32-bit unsigned integer entry to the dict.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param value The 32-bit unsigned integer value
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_append_uint32(DBusMessageIter *iter_dict,
					const char *key,
					const dbus_uint32_t value)
{
	if (!key)
		return FALSE;
	return __ni_dbus_add_dict_entry_basic(iter_dict, key, DBUS_TYPE_UINT32,
					      &value);
}


/**
 * Add a 64-bit integer entry to the dict.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param value The 64-bit integer value
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_append_int64(DBusMessageIter *iter_dict,
				       const char *key,
				       const dbus_int64_t value)
{
	if (!key)
		return FALSE;
	return __ni_dbus_add_dict_entry_basic(iter_dict, key, DBUS_TYPE_INT64,
					      &value);
}


/**
 * Add a 64-bit unsigned integer entry to the dict.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param value The 64-bit unsigned integer value
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_append_uint64(DBusMessageIter *iter_dict,
					const char *key,
					const dbus_uint64_t value)
{
	if (!key)
		return FALSE;
	return __ni_dbus_add_dict_entry_basic(iter_dict, key, DBUS_TYPE_UINT64,
					      &value);
}


/**
 * Add a double-precision floating point entry to the dict.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param value The double-precision floating point value
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_append_double(DBusMessageIter *iter_dict,
					const char * key,
					const double value)
{
	if (!key)
		return FALSE;
	return __ni_dbus_add_dict_entry_basic(iter_dict, key, DBUS_TYPE_DOUBLE,
					      &value);
}


/**
 * Add a DBus object path entry to the dict.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param value The DBus object path value
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_append_object_path(DBusMessageIter *iter_dict,
					     const char *key,
					     const char *value)
{
	if (!key || !value)
		return FALSE;
	return __ni_dbus_add_dict_entry_basic(iter_dict, key,
					      DBUS_TYPE_OBJECT_PATH, &value);
}


/**
 * Add a byte array entry to the dict.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param value The byte array
 * @param value_len The length of the byte array, in bytes
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_append_byte_array(DBusMessageIter *iter_dict,
					    const char *key,
					    const char *value,
					    const dbus_uint32_t value_len)
{
	if (!key)
		return FALSE;
	if (!value && (value_len != 0))
		return FALSE;
	return __ni_dbus_add_dict_entry_byte_array(iter_dict, key, value,
						   value_len);
}


/**
 * Begin a string array entry in the dict
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *                  ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param iter_dict_entry A private DBusMessageIter provided by the caller to
 *                        be passed to ni_dbus_dict_end_string_array()
 * @param iter_dict_val A private DBusMessageIter provided by the caller to
 *                      be passed to ni_dbus_dict_end_string_array()
 * @param iter_array On return, the DBusMessageIter to be passed to
 *                   ni_dbus_dict_string_array_add_element()
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_begin_string_array(DBusMessageIter *iter_dict,
					     const char *key,
					     DBusMessageIter *iter_dict_entry,
					     DBusMessageIter *iter_dict_val,
					     DBusMessageIter *iter_array)
{
	if (!iter_dict || !iter_dict_entry || !iter_dict_val || !iter_array)
		return FALSE;

	if (!__ni_dbus_add_dict_entry_start(iter_dict, iter_dict_entry, key))
		return FALSE;

	if (!dbus_message_iter_open_container(iter_dict_entry,
					      DBUS_TYPE_VARIANT,
					      DBUS_TYPE_ARRAY_AS_STRING
					      DBUS_TYPE_STRING_AS_STRING,
					      iter_dict_val))
		return FALSE;

	if (!dbus_message_iter_open_container(iter_dict_val, DBUS_TYPE_ARRAY,
					      DBUS_TYPE_BYTE_AS_STRING,
					      iter_array))
		return FALSE;

	return TRUE;
}


/**
 * Add a single string element to a string array dict entry
 *
 * @param iter_array A valid DBusMessageIter returned from
 *                   ni_dbus_dict_begin_string_array()'s
 *                   iter_array parameter
 * @param elem The string element to be added to the dict entry's string array
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_string_array_add_element(DBusMessageIter *iter_array,
						   const char *elem)
{
	if (!iter_array || !elem)
		return FALSE;

	return dbus_message_iter_append_basic(iter_array, DBUS_TYPE_STRING,
					      &elem);
}


/**
 * End a string array dict entry
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *                  ni_dbus_dict_open_write()
 * @param iter_dict_entry A private DBusMessageIter returned from
 *                        ni_dbus_dict_end_string_array()
 * @param iter_dict_val A private DBusMessageIter returned from
 *                      ni_dbus_dict_end_string_array()
 * @param iter_array A DBusMessageIter returned from
 *                   ni_dbus_dict_end_string_array()
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_end_string_array(DBusMessageIter *iter_dict,
					   DBusMessageIter *iter_dict_entry,
					   DBusMessageIter *iter_dict_val,
					   DBusMessageIter *iter_array)
{
	if (!iter_dict || !iter_dict_entry || !iter_dict_val || !iter_array)
		return FALSE;

	if (!dbus_message_iter_close_container(iter_dict_val, iter_array))
		return FALSE;

	if (!__ni_dbus_add_dict_entry_end(iter_dict, iter_dict_entry,
					  iter_dict_val))
		return FALSE;

	return TRUE;
}


/**
 * Convenience function to add an entire string array to the dict.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *                  ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param items The array of strings
 * @param num_items The number of strings in the array
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_append_string_array(DBusMessageIter *iter_dict,
					      const char *key,
					      const char **items,
					      const dbus_uint32_t num_items)
{
	DBusMessageIter iter_dict_entry, iter_dict_val, iter_array;
	dbus_uint32_t i;

	if (!key)
		return FALSE;
	if (!items && (num_items != 0))
		return FALSE;

	if (!ni_dbus_dict_begin_string_array(iter_dict, key,
					      &iter_dict_entry, &iter_dict_val,
					      &iter_array))
		return FALSE;

	for (i = 0; i < num_items; i++) {
		if (!ni_dbus_dict_string_array_add_element(&iter_array,
							    items[i]))
			return FALSE;
	}

	if (!ni_dbus_dict_end_string_array(iter_dict, &iter_dict_entry,
					    &iter_dict_val, &iter_array))
		return FALSE;

	return TRUE;
}


/**
 * Begin a string dict entry in the dict
 *
 * @param iter_parent_dict A valid DBusMessageIter returned from
 *                  ni_dbus_dict_open_write()
 * @param key The key of the dict item
 * @param iter_parent_entry A private DBusMessageIter provided by the caller to
 *                        be passed to ni_dbus_dict_end_string_dict()
 * @param iter_parent_val A private DBusMessageIter provided by the caller to
 *                      be passed to ni_dbus_dict_end_string_dict()
 * @param iter_child_dict On return, the DBusMessageIter to be passed to
 *                   ni_dbus_dict_string_dict_add_element()
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_begin_string_dict(DBusMessageIter *iter_parent_dict,
					     const char *key,
					     DBusMessageIter *iter_parent_entry,
					     DBusMessageIter *iter_parent_val,
					     DBusMessageIter *iter_child_dict)
{
	if (!iter_parent_dict || !iter_parent_entry || !iter_parent_val || !iter_child_dict)
		return FALSE;

	if (!__ni_dbus_add_dict_entry_start(iter_parent_dict, iter_parent_entry, key))
		return FALSE;

#if 0
	ni_debug_dbus("dbus_message_iter_open_container(%d, %s)",
					      DBUS_TYPE_VARIANT,
					      DBUS_TYPE_ARRAY_AS_STRING
					      DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
					      DBUS_TYPE_STRING_AS_STRING
					      DBUS_TYPE_VARIANT_AS_STRING
					      DBUS_DICT_ENTRY_END_CHAR_AS_STRING);
#endif

	if (!dbus_message_iter_open_container(iter_parent_entry,
					      DBUS_TYPE_VARIANT,
					      DBUS_TYPE_ARRAY_AS_STRING
					      DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
					      DBUS_TYPE_STRING_AS_STRING
					      DBUS_TYPE_VARIANT_AS_STRING
					      DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
					      iter_parent_val))
		return FALSE;

	if (!ni_dbus_dict_open_write(iter_parent_val, iter_child_dict))
		return FALSE;

	return TRUE;
}


/**
 * End a string dict dict entry
 *
 * @param iter_parent_dict A valid DBusMessageIter returned from
 *                  ni_dbus_dict_open_write()
 * @param iter_parent_entry A private DBusMessageIter returned from
 *                        ni_dbus_dict_end_string_dict()
 * @param iter_parent_val A private DBusMessageIter returned from
 *                      ni_dbus_dict_end_string_dict()
 * @param iter_child_dict A DBusMessageIter returned from
 *                   ni_dbus_dict_end_string_dict()
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_end_string_dict(DBusMessageIter *iter_parent_dict,
					   DBusMessageIter *iter_parent_entry,
					   DBusMessageIter *iter_parent_val,
					   DBusMessageIter *iter_child_dict)
{
	if (!iter_parent_dict || !iter_parent_entry || !iter_parent_val || !iter_child_dict)
		return FALSE;

	if (!dbus_message_iter_close_container(iter_parent_val, iter_child_dict))
		return FALSE;

	if (!__ni_dbus_add_dict_entry_end(iter_parent_dict, iter_parent_entry,
					  iter_parent_val))
		return FALSE;

	return TRUE;
}

dbus_bool_t
ni_dbus_message_iter_append_byte_array(DBusMessageIter *iter,
				const unsigned char *value, unsigned int len)
{
	DBusMessageIter iter_array;
	unsigned int i;

	if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
					      DBUS_TYPE_BYTE_AS_STRING,
					      &iter_array))
		return FALSE;

	for (i = 0; i < len; i++) {
		if (!dbus_message_iter_append_basic(&iter_array,
						    DBUS_TYPE_BYTE,
						    &(value[i])))
			return FALSE;
	}

	if (!dbus_message_iter_close_container(iter, &iter_array))
		return FALSE;

	return TRUE;
}

dbus_bool_t
ni_dbus_message_iter_append_variant(DBusMessageIter *iter, const ni_dbus_variant_t *variant)
{
	const char *type_as_string = NULL;
	const void *value;
	DBusMessageIter iter_val;
	dbus_bool_t rv;

	type_as_string = ni_dbus_variant_signature(variant);
	if (!type_as_string)
		return FALSE;

	if (!dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, type_as_string, &iter_val))
		return FALSE;

	value = ni_dbus_variant_datum_const_ptr(variant);
	if (value != NULL) {
		rv = dbus_message_iter_append_basic(&iter_val, variant->type, value);
	} else {
		rv = FALSE;

		switch (variant->type) {
		case DBUS_TYPE_ARRAY:
			rv = ni_dbus_message_iter_append_byte_array(&iter_val,
					variant->byte_array_value, variant->array.len);
			break;
		}

		if (!rv)
			ni_warn("%s: variant type %s not supported", __FUNCTION__, type_as_string);
	}

	if (rv)
		rv = dbus_message_iter_close_container(iter, &iter_val);

	return rv;
}

dbus_bool_t
ni_dbus_message_iter_get_variant(DBusMessageIter *iter,
					ni_dbus_variant_t *variant)
{
	DBusMessageIter iter_val;
	void *value;
	int type;

	ni_dbus_variant_destroy(variant);

	type = dbus_message_iter_get_arg_type(iter);
	if (type != DBUS_TYPE_VARIANT)
		return FALSE;

	dbus_message_iter_recurse(iter, &iter_val);
	variant->type = dbus_message_iter_get_arg_type(&iter_val);

	value = ni_dbus_variant_datum_ptr(variant);
	if (value != NULL) {
		/* Basic types */
		dbus_message_iter_get_basic(&iter_val, value);

		if (variant->type == DBUS_TYPE_STRING
		 || variant->type == DBUS_TYPE_OBJECT_PATH)
			variant->string_value = xstrdup(variant->string_value);
	} else {
		/* FIXME: need to handle arrays here */
		return FALSE;
	}

	dbus_message_iter_next(iter);
	return TRUE;
}

/*****************************************************/
/* Stuff for reading dicts                           */
/*****************************************************/

/**
 * Start reading from a dbus dict.
 *
 * @param iter A valid DBusMessageIter pointing to the start of the dict
 * @param iter_dict (out) A DBusMessageIter to be passed to
 *    ni_dbus_dict_read_next_entry()
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_open_read(DBusMessageIter *iter,
				    DBusMessageIter *iter_dict)
{
	if (!iter || !iter_dict)
		return FALSE;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY ||
	    dbus_message_iter_get_element_type(iter) != DBUS_TYPE_DICT_ENTRY)
		return FALSE;

	dbus_message_iter_recurse(iter, iter_dict);
	return TRUE;
}


#define BYTE_ARRAY_CHUNK_SIZE 34
#define BYTE_ARRAY_ITEM_SIZE (sizeof(char))

static dbus_bool_t __ni_dbus_dict_entry_get_byte_array(
	DBusMessageIter *iter, int array_type,
	struct ni_dbus_dict_entry *entry)
{
	dbus_uint32_t count = 0;
	dbus_bool_t success = FALSE;
	char *buffer;

	entry->bytearray_value = NULL;
	entry->array_type = DBUS_TYPE_BYTE;

	buffer = xcalloc(1, BYTE_ARRAY_ITEM_SIZE * BYTE_ARRAY_CHUNK_SIZE);
	if (!buffer) {
		perror("__ni_dbus_dict_entry_get_byte_array[dbus]: out of "
		       "memory");
		goto done;
	}

	entry->bytearray_value = buffer;
	entry->array_len = 0;
	while (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_BYTE) {
		char byte;

		if ((count % BYTE_ARRAY_CHUNK_SIZE) == 0 && count != 0) {
			buffer = realloc(buffer, BYTE_ARRAY_ITEM_SIZE *
					 (count + BYTE_ARRAY_CHUNK_SIZE));
			if (buffer == NULL) {
				perror("__ni_dbus_dict_entry_get_byte_array["
				       "dbus] out of memory trying to "
				       "retrieve the string array");
				goto done;
			}
		}
		entry->bytearray_value = buffer;

		dbus_message_iter_get_basic(iter, &byte);
		entry->bytearray_value[count] = byte;
		entry->array_len = ++count;
		dbus_message_iter_next(iter);
	}

	/* Zero-length arrays are valid. */
	if (entry->array_len == 0) {
		free(entry->bytearray_value);
		entry->bytearray_value = NULL;
	}

	success = TRUE;

done:
	return success;
}


#define STR_ARRAY_CHUNK_SIZE 8
#define STR_ARRAY_ITEM_SIZE (sizeof(char *))

static dbus_bool_t __ni_dbus_dict_entry_get_string_array(
	DBusMessageIter *iter, int array_type,
	struct ni_dbus_dict_entry *entry)
{
	dbus_uint32_t count = 0;
	dbus_bool_t success = FALSE;
	char **buffer;

	entry->strarray_value = NULL;
	entry->array_type = DBUS_TYPE_STRING;

	buffer = xcalloc(1, STR_ARRAY_ITEM_SIZE * STR_ARRAY_CHUNK_SIZE);
	if (buffer == NULL) {
		perror("__ni_dbus_dict_entry_get_string_array[dbus] out of "
		       "memory trying to retrieve a string array");
		goto done;
	}

	entry->strarray_value = buffer;
	entry->array_len = 0;
	while (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRING) {
		const char *value;
		char *str;

		if ((count % STR_ARRAY_CHUNK_SIZE) == 0 && count != 0) {
			buffer = realloc(buffer, STR_ARRAY_ITEM_SIZE *
					 (count + STR_ARRAY_CHUNK_SIZE));
			if (buffer == NULL) {
				perror("__ni_dbus_dict_entry_get_string_array["
				       "dbus] out of memory trying to "
				       "retrieve the string array");
				goto done;
			}
		}
		entry->strarray_value = buffer;

		dbus_message_iter_get_basic(iter, &value);
		str = xstrdup(value);
		if (str == NULL) {
			perror("__ni_dbus_dict_entry_get_string_array[dbus] "
			       "out of memory trying to duplicate the string "
			       "array");
			goto done;
		}
		entry->strarray_value[count] = str;
		entry->array_len = ++count;
		dbus_message_iter_next(iter);
	}

	/* Zero-length arrays are valid. */
	if (entry->array_len == 0) {
		free(entry->strarray_value);
		entry->strarray_value = NULL;
	}

	success = TRUE;

done:
	return success;
}


static dbus_bool_t __ni_dbus_dict_entry_get_array(
	DBusMessageIter *iter_dict_val, struct ni_dbus_dict_entry *entry)
{
	int array_type = dbus_message_iter_get_element_type(iter_dict_val);
	dbus_bool_t success = FALSE;
	DBusMessageIter iter_array;

	if (!entry)
		return FALSE;

	dbus_message_iter_recurse(iter_dict_val, &iter_array);

	switch (array_type) {
	case DBUS_TYPE_BYTE:
		success = __ni_dbus_dict_entry_get_byte_array(&iter_array,
							      array_type,
							      entry);
		break;
	case DBUS_TYPE_STRING:
		success = __ni_dbus_dict_entry_get_string_array(&iter_array,
								array_type,
								entry);
		break;
	default:
		break;
	}

	return success;
}


static dbus_bool_t __ni_dbus_dict_fill_value_from_variant(
	struct ni_dbus_dict_entry *entry, DBusMessageIter *iter_dict_val)
{
	dbus_bool_t success = TRUE;

	switch (entry->type) {
	case DBUS_TYPE_STRING: {
		const char *v;
		dbus_message_iter_get_basic(iter_dict_val, &v);
		entry->str_value = xstrdup(v);
		break;
	}
	case DBUS_TYPE_BOOLEAN: {
		dbus_bool_t v;
		dbus_message_iter_get_basic(iter_dict_val, &v);
		entry->bool_value = v;
		break;
	}
	case DBUS_TYPE_BYTE: {
		char v;
		dbus_message_iter_get_basic(iter_dict_val, &v);
		entry->byte_value = v;
		break;
	}
	case DBUS_TYPE_INT16: {
		dbus_int16_t v;
		dbus_message_iter_get_basic(iter_dict_val, &v);
		entry->int16_value = v;
		break;
	}
	case DBUS_TYPE_UINT16: {
		dbus_uint16_t v;
		dbus_message_iter_get_basic(iter_dict_val, &v);
		entry->uint16_value = v;
		break;
	}
	case DBUS_TYPE_INT32: {
		dbus_int32_t v;
		dbus_message_iter_get_basic(iter_dict_val, &v);
		entry->int32_value = v;
		break;
	}
	case DBUS_TYPE_UINT32: {
		dbus_uint32_t v;
		dbus_message_iter_get_basic(iter_dict_val, &v);
		entry->uint32_value = v;
		break;
	}
	case DBUS_TYPE_INT64: {
		dbus_int64_t v;
		dbus_message_iter_get_basic(iter_dict_val, &v);
		entry->int64_value = v;
		break;
	}
	case DBUS_TYPE_UINT64: {
		dbus_uint64_t v;
		dbus_message_iter_get_basic(iter_dict_val, &v);
		entry->uint64_value = v;
		break;
	}
	case DBUS_TYPE_DOUBLE: {
		double v;
		dbus_message_iter_get_basic(iter_dict_val, &v);
		entry->double_value = v;
		break;
	}
	case DBUS_TYPE_OBJECT_PATH: {
		char *v;
		dbus_message_iter_get_basic(iter_dict_val, &v);
		entry->str_value = xstrdup(v);
		break;
	}
	case DBUS_TYPE_ARRAY: {
		success = __ni_dbus_dict_entry_get_array(iter_dict_val, entry);
		break;
	}
	default:
		success = FALSE;
		break;
	}

	return success;
}


/**
 * Read the current key/value entry from the dict.  Entries are dynamically
 * allocated when needed and must be freed after use with the
 * ni_dbus_dict_entry_clear() function.
 *
 * The returned entry object will be filled with the type and value of the next
 * entry in the dict, or the type will be DBUS_TYPE_INVALID if an error
 * occurred.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_read()
 * @param entry A valid dict entry object into which the dict key and value
 *    will be placed
 * @return TRUE on success, FALSE on failure
 *
 */
dbus_bool_t ni_dbus_dict_get_entry(DBusMessageIter *iter_dict,
				    struct ni_dbus_dict_entry * entry)
{
	DBusMessageIter iter_dict_entry, iter_dict_val;
	int type;
	const char *key;

	if (!iter_dict || !entry)
		goto error;

	if (dbus_message_iter_get_arg_type(iter_dict) != DBUS_TYPE_DICT_ENTRY)
		goto error;

	dbus_message_iter_recurse(iter_dict, &iter_dict_entry);
	dbus_message_iter_get_basic(&iter_dict_entry, &key);
	entry->key = key;

	if (!dbus_message_iter_next(&iter_dict_entry))
		goto error;
	type = dbus_message_iter_get_arg_type(&iter_dict_entry);
	if (type != DBUS_TYPE_VARIANT)
		goto error;

	dbus_message_iter_recurse(&iter_dict_entry, &iter_dict_val);
	entry->type = dbus_message_iter_get_arg_type(&iter_dict_val);
	if (!__ni_dbus_dict_fill_value_from_variant(entry, &iter_dict_val))
		goto error;

	dbus_message_iter_next(iter_dict);
	return TRUE;

error:
	if (entry) {
		ni_dbus_dict_entry_clear(entry);
		entry->type = DBUS_TYPE_INVALID;
		entry->array_type = DBUS_TYPE_INVALID;
	}

	return FALSE;
}


/**
 * Return whether or not there are additional dictionary entries.
 *
 * @param iter_dict A valid DBusMessageIter returned from
 *    ni_dbus_dict_open_read()
 * @return TRUE if more dict entries exists, FALSE if no more dict entries
 * exist
 */
dbus_bool_t ni_dbus_dict_has_dict_entry(DBusMessageIter *iter_dict)
{
	if (!iter_dict) {
		perror("ni_dbus_dict_has_dict_entry[dbus]: out of memory");
		return FALSE;
	}
	return dbus_message_iter_get_arg_type(iter_dict) ==
		DBUS_TYPE_DICT_ENTRY;
}


/**
 * Free any memory used by the entry object.
 *
 * @param entry The entry object
 */
void ni_dbus_dict_entry_clear(struct ni_dbus_dict_entry *entry)
{
	unsigned int i;

	if (!entry)
		return;
	switch (entry->type) {
	case DBUS_TYPE_OBJECT_PATH:
	case DBUS_TYPE_STRING:
		free(entry->str_value);
		break;
	case DBUS_TYPE_ARRAY:
		switch (entry->array_type) {
		case DBUS_TYPE_BYTE:
			free(entry->bytearray_value);
			break;
		case DBUS_TYPE_STRING:
			for (i = 0; i < entry->array_len; i++)
				free(entry->strarray_value[i]);
			free(entry->strarray_value);
			break;
		}
		break;
	}

	memset(entry, 0, sizeof(struct ni_dbus_dict_entry));
}
