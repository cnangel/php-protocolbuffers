/*
 * Protocol Buffers for PHP
 * Copyright 2013 Shuhei Tanuma.  All rights reserved.
 *
 * https://github.com/chobie/php-protocolbuffers
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "protocolbuffers.h"
#include "message.h"
#if ZEND_MODULE_API_NO >= 20151012
#include "Zend/zend_smart_str.h"
#else
#include "ext/standard/php_smart_str.h"
#endif
#include "unknown_field_set.h"
#include "extension_registry.h"

//#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION == 4) && (PHP_RELEASE_VERSION >= 26)) || ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION == 5) && (PHP_RELEASE_VERSION >= 10))
//#include "ext/json/php_json.h"
//#endif

enum ProtocolBuffers_MagicMethod {
	MAGICMETHOD_GET    = 1,
	MAGICMETHOD_SET    = 2,
	MAGICMETHOD_APPEND = 3,
	MAGICMETHOD_CLEAR  = 4,
	MAGICMETHOD_HAS    = 5,
	MAGICMETHOD_MUTABLE= 6,
};

static zend_object_handlers php_protocolbuffers_message_object_handlers;
static int json_serializable_checked = 0;

#define PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME2(instance, container, proto) \
	{\
		zend_class_entry *__ce;\
		int __err;\
		\
		__ce  = Z_OBJCE_P(instance);\
		__err = php_protocolbuffers_get_scheme_container(ZSTR_VAL(__ce->name), ZSTR_LEN(__ce->name), container TSRMLS_CC);\
		if (__err) {\
			if (EG(exception)) {\
				return;\
			} else {\
				/* TODO: improve displaying error message */\
				php_error_docref(NULL TSRMLS_CC, E_ERROR, "php_protocolbuffers_get_scheme_container failed. %s does not have getDescriptor method", __ce->name);\
				return;\
			}\
		}\
	}


#define PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME2(instance, &container, proto)

static void php_protocolbuffers_message_free_storage(php_protocolbuffers_message *object TSRMLS_DC)
{
	if (object->container != NULL) {
		ZVAL_NULL(object->container);
#if ZEND_MODULE_API_NO >= 20151012
		zval_ptr_dtor(object->container);
#else
		zval_ptr_dtor(&object->container);
#endif
		object->container = NULL;
	}
	zend_object_std_dtor(&object->zo TSRMLS_CC);
	efree(object);
}

#if ZEND_MODULE_API_NO >= 20151012
zend_object *php_protocolbuffers_message_new(zend_class_entry *ce TSRMLS_DC)
{
    php_protocolbuffers_message *message_object;

    message_object = ecalloc(1, sizeof(php_protocolbuffers_message) + zend_object_properties_size(ce));
    message_object->zo.ce = ce;

    zend_object_std_init(&message_object->zo, ce);
    object_properties_init(&message_object->zo, ce);
    message_object->zo.handlers = zend_get_std_object_handlers();
    return &message_object->zo;
}
#else
zend_object_value php_protocolbuffers_message_new(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value retval;

	PHP_PROTOCOLBUFFERS_STD_CREATE_OBJECT(php_protocolbuffers_message);

	object->max    = 0;
	object->offset = 0;
	MAKE_STD_ZVAL(object->container);
	ZVAL_NULL(object->container);

	retval.handlers = &php_protocolbuffers_message_object_handlers;

	return retval;
}
#endif

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_set_from, 0, 0, 1)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_serialize_to_string, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_parse_from_string, 0, 0, 1)
	ZEND_ARG_INFO(0, string)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message___call, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_merge_from, 0, 0, 1)
	ZEND_ARG_INFO(0, message)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_discard_unknown_fields, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_clear, 0, 0, 0)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_clear_alls, 0, 0, 0)
	ZEND_ARG_INFO(0, clear_unknown_fields)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_has, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_get, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_mutable, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_set, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_append, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_get_extension, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_has_extension, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_set_extension, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_message_clear_extension, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

static void php_protocolbuffers_get_hash(php_protocolbuffers_scheme_container *container, php_protocolbuffers_scheme *scheme, zval *object, char **name, int *name_len, HashTable **ht TSRMLS_DC)
{
	char *n = {0};
	int n_len = 0;
	HashTable *htt = NULL;

	if (container->use_single_property > 0) {
		n     = container->single_property_name;
		n_len = container->single_property_name_len;

#if ZEND_MODULE_API_NO >= 20151012
		if ((htt = zend_hash_str_find_ptr(Z_OBJPROP_P(object), n, n_len)) == NULL) {
#else
		if (zend_hash_find(Z_OBJPROP_P(object), n, n_len, (void **)&htt) == FAILURE) {
#endif
			return;
		}

		n = scheme->name;
		n_len = scheme->name_len;
	} else {
		htt = Z_OBJPROP_P(object);

		n = scheme->mangled_name;
		n_len = scheme->mangled_name_len;
	}

	name = &n;
	name_len = &n_len;
	*ht = htt;

}

static void php_protocolbuffers_message_merge_from(php_protocolbuffers_scheme_container *container, HashTable *htt, HashTable *hts TSRMLS_DC)
{
	php_protocolbuffers_scheme *scheme;

	int i = 0;
	for (i = 0; i < container->size; i++) {
		zval **tmp = NULL;
		char *name = {0};
		int name_len = 0;

		scheme = &(container->scheme[i]);

		if (container->use_single_property > 0) {
			name = scheme->name;
			name_len = scheme->name_len;
		} else {
			name = scheme->mangled_name;
			name_len = scheme->mangled_name_len;
		}

#if ZEND_MODULE_API_NO >= 20151012
		if ((*tmp = zend_hash_str_find(hts, name, name_len)) != NULL) {
#else
		if (zend_hash_find(hts, name, name_len, (void **)&tmp) == SUCCESS) {
#endif
			zval *val;

			switch (Z_TYPE_PP(tmp)) {
			case IS_NULL:
			break;
			case IS_STRING:
#if ZEND_MODULE_API_NO >= 20151012
				ZVAL_STRINGL(val, Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp));

				Z_ADDREF_P(val);
				zend_string *nn = zend_string_init(name, name_len, 0);
				zend_hash_update(htt, nn, val);
				zend_string_release(nn);
				zval_ptr_dtor(val);
#else
				MAKE_STD_ZVAL(val);
				ZVAL_STRINGL(val, Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp), 1);

				Z_ADDREF_P(val);
				zend_hash_update(htt, name, name_len, (void **)&val, sizeof(zval *), NULL);
				zval_ptr_dtor(&val);
#endif
				break;
			case IS_LONG:
#if ZEND_MODULE_API_NO >= 20151012
				ZVAL_LONG(val, Z_LVAL_PP(tmp));

				Z_ADDREF_P(val);
				zend_string *n2 = zend_string_init(name, name_len, 0);
				zend_hash_update(htt, n2, val);
				zend_string_release(n2);
				zval_ptr_dtor(val);
#else
				MAKE_STD_ZVAL(val);
				ZVAL_LONG(val, Z_LVAL_PP(tmp));

				Z_ADDREF_P(val);
				zend_hash_update(htt, name, name_len, (void **)&val, sizeof(zval *), NULL);
				zval_ptr_dtor(&val);
#endif
			break;
			case IS_DOUBLE:
#if ZEND_MODULE_API_NO >= 20151012
				ZVAL_DOUBLE(val, Z_DVAL_PP(tmp));

				Z_ADDREF_P(val);
				zend_string *n3 = zend_string_init(name, name_len, 0);
				zend_hash_update(htt, n3, val);
				zend_string_release(n3);
				zval_ptr_dtor(val);
#else
				MAKE_STD_ZVAL(val);
				ZVAL_DOUBLE(val, Z_DVAL_PP(tmp));

				Z_ADDREF_P(val);
				zend_hash_update(htt, name, name_len, (void **)&val, sizeof(zval *), NULL);
				zval_ptr_dtor(&val);
#endif
			break;
			case IS_OBJECT:
			{
				zval **tmp2 = NULL;

#if ZEND_MODULE_API_NO >= 20151012
				if ((*tmp2 = zend_hash_str_find(htt, name, name_len)) != NULL) {
#else
				if (zend_hash_find(htt, name, name_len, (void **)&tmp2) == SUCCESS) {
#endif
					if (Z_TYPE_PP(tmp2) == IS_OBJECT) {
						char *n;
						int n_len;
						php_protocolbuffers_scheme_container *c;
						HashTable *_htt = NULL, *_hts = NULL;

						PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME2(*tmp, &c, p)
						php_protocolbuffers_get_hash(c, c->scheme, *tmp, &n, &n_len, &_htt TSRMLS_CC);
						php_protocolbuffers_get_hash(c, c->scheme, *tmp2, &n, &n_len, &_hts TSRMLS_CC);

						php_protocolbuffers_message_merge_from(c, _htt, _hts TSRMLS_CC);
					} else {
#if ZEND_MODULE_API_NO >= 20151012
						ZVAL_ZVAL(val, *tmp, 1, 0);

						Z_ADDREF_P(val);
						zend_string *n = zend_string_init(name, name_len, 0);
						zend_hash_update(htt, n, val);
						zend_string_release(n);
						zval_ptr_dtor(val);
#else
						MAKE_STD_ZVAL(val);
						ZVAL_ZVAL(val, *tmp, 1, 0);

						Z_ADDREF_P(val);
						zend_hash_update(htt, name, name_len, (void **)&val, sizeof(zval *), NULL);
						zval_ptr_dtor(&val);
#endif
					}
				} else {
#if ZEND_MODULE_API_NO >= 20151012
					ZVAL_ZVAL(val, *tmp, 1, 0);

					Z_ADDREF_P(val);
					zend_string *n = zend_string_init(name, name_len, 0);
					zend_hash_update(htt, n, val);
					zend_string_release(n);
					zval_ptr_dtor(val);
#else
					MAKE_STD_ZVAL(val);
					ZVAL_ZVAL(val, *tmp, 1, 0);

					Z_ADDREF_P(val);
					zend_hash_update(htt, name, name_len, (void **)&val, sizeof(zval *), NULL);
					zval_ptr_dtor(&val);
#endif
				}
			}
			break;
			case IS_ARRAY: {
				zval **tmp2 = NULL;

#if ZEND_MODULE_API_NO >= 20151012
				if ((*tmp2 = zend_hash_str_find(htt, name, name_len)) != NULL) {
					if (Z_TYPE_PP(tmp2) == IS_ARRAY) {
						php_array_merge(Z_ARRVAL_PP(tmp2), Z_ARRVAL_PP(tmp));
					} else {
						ZVAL_ZVAL(val, *tmp, 1, 0);

						Z_ADDREF_P(val);
						zend_string *n = zend_string_init(name, name_len, 0);
						zend_hash_update(htt, n, val);
						zend_string_release(n);
						zval_ptr_dtor(val);
					}
				} else {
					ZVAL_ZVAL(val, *tmp, 1, 0);

					Z_ADDREF_P(val);
					zend_string *n = zend_string_init(name, name_len, 0);
					zend_hash_update(htt, n, val);
					zend_string_release(n);
					zval_ptr_dtor(val);
#else
				if (zend_hash_find(htt, name, name_len, (void **)&tmp2) == SUCCESS) {
					if (Z_TYPE_PP(tmp2) == IS_ARRAY) {
						php_array_merge(Z_ARRVAL_PP(tmp2), Z_ARRVAL_PP(tmp), 1 TSRMLS_CC);
					} else {
						MAKE_STD_ZVAL(val);
						ZVAL_ZVAL(val, *tmp, 1, 0);

						Z_ADDREF_P(val);
						zend_hash_update(htt, name, name_len, (void **)&val, sizeof(zval *), NULL);
						zval_ptr_dtor(&val);
					}
				} else {
					MAKE_STD_ZVAL(val);
					ZVAL_ZVAL(val, *tmp, 1, 0);

					Z_ADDREF_P(val);
					zend_hash_update(htt, name, name_len, (void **)&val, sizeof(zval *), NULL);
					zval_ptr_dtor(&val);
#endif
				}
			}
			break;
			default:
				zend_error(E_NOTICE, "mergeFrom: zval type %d is not supported.", Z_TYPE_PP(tmp));
			}
		}

	}
}

static inline void php_protocolbuffers_typeconvert(php_protocolbuffers_scheme *scheme, zval *vl TSRMLS_DC)
{
	// TODO: check message
#if ZEND_MODULE_API_NO >= 20151012
#define TYPE_CONVERT(vl) \
		switch (scheme->type) {\
			case TYPE_STRING:\
				if (Z_TYPE_P(vl) != IS_STRING) {\
					if (Z_TYPE_P(vl) == IS_OBJECT && zend_hash_str_exists(&(Z_OBJCE_P(vl)->function_table), ZEND_STRS("__tostring"))) {\
					} else {\
						convert_to_string(vl);\
					}\
				}\
			break;\
			case TYPE_SINT32:\
			case TYPE_INT32:\
			case TYPE_UINT32:\
				if (Z_TYPE_P(vl) != IS_LONG) {\
					if (Z_TYPE_P(vl) != IS_STRING) {\
						convert_to_long(vl);\
					}\
				}\
			break;\
			case TYPE_SINT64:\
			case TYPE_INT64:\
			case TYPE_UINT64:\
				if (Z_TYPE_P(vl) != IS_LONG) {\
					if (Z_TYPE_P(vl) != IS_STRING) {\
						convert_to_long(vl);\
					}\
				}\
			break;\
			case TYPE_DOUBLE:\
			case TYPE_FLOAT:\
				if (Z_TYPE_P(vl) != IS_DOUBLE) {\
					if (Z_TYPE_P(vl) != IS_STRING) {\
						convert_to_double(vl);\
					}\
				}\
			break;\
			case TYPE_BOOL:\
				if (Z_TYPE_P(vl) != IS_TRUE || Z_TYPE_P(vl) != IS_FALSE) {\
					convert_to_boolean(vl);\
				}\
			break;\
		}
#else
#define TYPE_CONVERT(vl) \
		switch (scheme->type) {\
			case TYPE_STRING:\
				if (Z_TYPE_P(vl) != IS_STRING) {\
					if (Z_TYPE_P(vl) == IS_OBJECT && zend_hash_exists(&(Z_OBJCE_P(vl)->function_table), ZEND_STRS("__tostring"))) {\
					} else {\
						convert_to_string(vl);\
					}\
				}\
			break;\
			case TYPE_SINT32:\
			case TYPE_INT32:\
			case TYPE_UINT32:\
				if (Z_TYPE_P(vl) != IS_LONG) {\
					if (Z_TYPE_P(vl) != IS_STRING) {\
						convert_to_long(vl);\
					}\
				}\
			break;\
			case TYPE_SINT64:\
			case TYPE_INT64:\
			case TYPE_UINT64:\
				if (Z_TYPE_P(vl) != IS_LONG) {\
					if (Z_TYPE_P(vl) != IS_STRING) {\
						convert_to_long(vl);\
					}\
				}\
			break;\
			case TYPE_DOUBLE:\
			case TYPE_FLOAT:\
				if (Z_TYPE_P(vl) != IS_DOUBLE) {\
					if (Z_TYPE_P(vl) != IS_STRING) {\
						convert_to_double(vl);\
					}\
				}\
			break;\
			case TYPE_BOOL:\
				if (Z_TYPE_P(vl) != IS_BOOL) {\
					convert_to_boolean(vl);\
				}\
			break;\
		}
#endif

	if (scheme->repeated < 1) {
		TYPE_CONVERT(vl)
	} else {
		zval **entry;
		HashPosition pos;

		if (Z_TYPE_P(vl) == IS_ARRAY) {
			zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(vl), &pos);
			while ((*entry = zend_hash_get_current_data_ex(Z_ARRVAL_P(vl), &pos)) != NULL) {
				TYPE_CONVERT(*entry)

				zend_hash_move_forward_ex(Z_ARRVAL_P(vl), &pos);
			}
		}
	}

#undef TYPE_CONVERT
}


static php_protocolbuffers_scheme *php_protocolbuffers_message_get_scheme_by_name(php_protocolbuffers_scheme_container *container, char *name, int name_len, char *name2, int name2_len)
{
	int i = 0;
	php_protocolbuffers_scheme *scheme = NULL;

	for (i = 0; i < container->size; i++) {
		scheme = &container->scheme[i];

		if (strcmp(scheme->name, name) == 0) {
			break;
		}
		if (name2 != NULL) {
			if (scheme->magic_type == 1 && strcasecmp(scheme->original_name, name2) == 0) {
				break;
			}
		}
		scheme = NULL;
	}

	return scheme;
}

static void php_protocolbuffers_message_get_hash_table_by_container(php_protocolbuffers_scheme_container *container, php_protocolbuffers_scheme *scheme, zval *instance, HashTable **hash, char **name, int *name_len TSRMLS_DC)
{
	char *n = NULL;
	int n_len = 0;
	HashTable *htt = NULL;

	if (container->use_single_property > 0) {
		n     = container->single_property_name;
		n_len = container->single_property_name_len;

#if ZEND_MODULE_API_NO >= 20151012
		if ((htt = zend_hash_str_find_ptr(Z_OBJPROP_P(instance), n, n_len)) == NULL) {
#else
		if (zend_hash_find(Z_OBJPROP_P(instance), n, n_len, (void **)&htt) == FAILURE) {
#endif
			return;
		}
	} else {
		htt = Z_OBJPROP_P(instance);

		n = scheme->mangled_name;
		n_len = scheme->mangled_name_len;
	}

	*hash = htt;
	*name = n;
	*name_len = n_len;
}

static void php_protocolbuffers_message_get(INTERNAL_FUNCTION_PARAMETERS, zval *instance, php_protocolbuffers_scheme_container *container, char *name, int name_len, char *name2, int name2_len, zval *params)
{
	char *n = {0};
	int n_len = 0;
	HashTable *htt = NULL;
	php_protocolbuffers_scheme *scheme;
	zval **e = NULL;

	scheme = php_protocolbuffers_message_get_scheme_by_name(container, name, name_len, name2, name2_len);
	if (scheme == NULL) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "%s does not find", name);
		return;
	}

	if (scheme->is_extension) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "get method can't use for extension value", name);
		return;
	}

	php_protocolbuffers_message_get_hash_table_by_container(container, scheme, instance, &htt, &n, &n_len TSRMLS_CC);

#if ZEND_MODULE_API_NO >= 20151012
	if ((*e = zend_hash_str_find(htt, n, n_len)) != NULL) {
		if (scheme->repeated && params != NULL && Z_TYPE_P(params) != IS_NULL) {
			zval **value;

			if (Z_TYPE_P(params) != IS_LONG) {
				convert_to_long(params);
			}

			if ((*value = zend_hash_index_find(Z_ARRVAL_PP(e), Z_LVAL_P(params))) != NULL) {
				RETURN_ZVAL(*value, 1, 0);
			}
		} else {
			if (scheme->ce != NULL && Z_TYPE_PP(e) == IS_NULL) {
				zval *tmp;
				object_init_ex(tmp, scheme->ce);
				php_protocolbuffers_properties_init(tmp, scheme->ce TSRMLS_CC);

				RETURN_ZVAL(tmp, 0, 1);
			} else {
				RETURN_ZVAL(*e, 1, 0);
			}
		}
	}
#else
	if (zend_hash_find(htt, n, n_len, (void **)&e) == SUCCESS) {
		if (scheme->repeated && params != NULL && Z_TYPE_P(params) != IS_NULL) {
			zval **value;

			if (Z_TYPE_P(params) != IS_LONG) {
				convert_to_long(params);
			}

			if (zend_hash_index_find(Z_ARRVAL_PP(e), Z_LVAL_P(params), (void **)&value) == SUCCESS) {
				RETURN_ZVAL(*value, 1, 0);
			}
		} else {
			if (scheme->ce != NULL && Z_TYPE_PP(e) == IS_NULL) {
				zval *tmp;
				MAKE_STD_ZVAL(tmp);
				object_init_ex(tmp, scheme->ce);
				php_protocolbuffers_properties_init(tmp, scheme->ce TSRMLS_CC);

				RETURN_ZVAL(tmp, 0, 1);
			} else {
				RETURN_ZVAL(*e, 1, 0);
			}
		}
	}
#endif
}

static void php_protocolbuffers_message_set(INTERNAL_FUNCTION_PARAMETERS, zval *instance, php_protocolbuffers_scheme_container *container, char *name, int name_len, char *name2, int name2_len, zval *value)
{
	php_protocolbuffers_scheme *scheme;
	php_protocolbuffers_message *m;
	zval *tmp;
	zval **e = NULL;
	HashTable *htt;
	char *n = {0};
	int n_len = 0;
	int should_free = 0;

	scheme = php_protocolbuffers_message_get_scheme_by_name(container, name, name_len, name2, name2_len);
	if (scheme == NULL) {
#if ZEND_MODULE_API_NO >= 20151012
		zval_ptr_dtor(value);
#else
		zval_ptr_dtor(&value);
#endif
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "%s does not find", name);
		return;
	}

	if (scheme->is_extension) {
#if ZEND_MODULE_API_NO >= 20151012
		zval_ptr_dtor(value);
#else
		zval_ptr_dtor(&value);
#endif
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "set method can't use for extension value", name);
		return;
	}

	if (Z_TYPE_P(value) == IS_NULL) {
#if ZEND_MODULE_API_NO >= 20151012
		zval_ptr_dtor(value);
#else
		zval_ptr_dtor(&value);
#endif
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "set method can't pass null value", name);
		return;
	}

	if (scheme->ce != NULL) {
		if (scheme->repeated) {
			HashPosition pos;
			zval **element, *outer;

			if (zend_hash_num_elements(Z_ARRVAL_P(value)) > 0) {
#if ZEND_MODULE_API_NO >= 20151012
				array_init(outer);

				for(zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(value), &pos);
								(*element = zend_hash_get_current_data_ex(Z_ARRVAL_P(value), &pos)) != NULL;
								zend_hash_move_forward_ex(Z_ARRVAL_P(value), &pos)
				) {
#else
				MAKE_STD_ZVAL(outer);
				array_init(outer);

				for(zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(value), &pos);
								zend_hash_get_current_data_ex(Z_ARRVAL_P(value), (void **)&element, &pos) == SUCCESS;
								zend_hash_move_forward_ex(Z_ARRVAL_P(value), &pos)
				) {
#endif
					zval *child, *param;

					if (Z_TYPE_PP(element) == IS_NULL) {
						continue;
					}

					if (Z_TYPE_PP(element) == IS_OBJECT) {
						if (scheme->ce != Z_OBJCE_PP(element)) {
#if ZEND_MODULE_API_NO >= 20151012
							zval_ptr_dtor(outer);
#else
							zval_ptr_dtor(&outer);
#endif
							zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "expected %s class. given %s class", scheme->ce->name, Z_OBJCE_PP(element)->name);
							return;
						} else {
#if ZEND_MODULE_API_NO >= 20151012
							m = (php_protocolbuffers_message *) ((char*)Z_OBJ_P(*element) - XtOffsetOf(php_protocolbuffers_message, zo)); 
#else
							m = PHP_PROTOCOLBUFFERS_GET_OBJECT(php_protocolbuffers_message, *element);
#endif
							ZVAL_ZVAL(m->container, instance, 0, 0);

							Z_ADDREF_PP(element);
							add_next_index_zval(outer, *element);
						}
					} else {
#if ZEND_MODULE_API_NO >= 20151012
						ZVAL_ZVAL(param, *element, 1, 0);

						object_init_ex(child, scheme->ce);
						php_protocolbuffers_properties_init(child, scheme->ce TSRMLS_CC);

						zend_call_method_with_1_params(child, scheme->ce, NULL, ZEND_CONSTRUCTOR_FUNC_NAME, NULL, param);
						zval_ptr_dtor(param);

						m = (php_protocolbuffers_message *) ((char*)Z_OBJ_P(child) - XtOffsetOf(php_protocolbuffers_message, zo)); 
#else
						MAKE_STD_ZVAL(child);
						MAKE_STD_ZVAL(param);
						ZVAL_ZVAL(param, *element, 1, 0);

						object_init_ex(child, scheme->ce);
						php_protocolbuffers_properties_init(child, scheme->ce TSRMLS_CC);

						zend_call_method_with_1_params(&child, scheme->ce, NULL, ZEND_CONSTRUCTOR_FUNC_NAME, NULL, param);
						zval_ptr_dtor(&param);

						m = PHP_PROTOCOLBUFFERS_GET_OBJECT(php_protocolbuffers_message, child);
#endif
						ZVAL_ZVAL(m->container, instance, 0, 0);
						add_next_index_zval(outer, child);
					}

				}
				value = outer;
				should_free = 1;
			}

		} else {
			if (Z_TYPE_P(value) == IS_ARRAY) {
				/* NOTE(chobie): Array to Object conversion */
				zval *param;

#if ZEND_MODULE_API_NO >= 20151012
				ZVAL_ZVAL(param, value, 1, 0);

				object_init_ex(tmp, scheme->ce);
				php_protocolbuffers_properties_init(tmp, scheme->ce TSRMLS_CC);
				zend_call_method_with_1_params(tmp, scheme->ce, NULL, ZEND_CONSTRUCTOR_FUNC_NAME, NULL, param);
				zval_ptr_dtor(param);
#else
				MAKE_STD_ZVAL(tmp);
				MAKE_STD_ZVAL(param);

				ZVAL_ZVAL(param, value, 1, 0);

				object_init_ex(tmp, scheme->ce);
				php_protocolbuffers_properties_init(tmp, scheme->ce TSRMLS_CC);
				zend_call_method_with_1_params(&tmp, scheme->ce, NULL, ZEND_CONSTRUCTOR_FUNC_NAME, NULL, param);
				zval_ptr_dtor(&param);
#endif

				value = tmp;
				should_free = 1;
			}

			if (scheme->ce != Z_OBJCE_P(value)) {
#if ZEND_MODULE_API_NO >= 20151012
				zval_ptr_dtor(value);
#else
				zval_ptr_dtor(&value);
#endif
				zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "expected %s class. given %s class", scheme->ce->name, Z_OBJCE_P(value)->name);
				return;
			}

#if ZEND_MODULE_API_NO >= 20151012
			m = (php_protocolbuffers_message *) ((char*)Z_OBJ_P(value) - XtOffsetOf(php_protocolbuffers_message, zo)); 
#else
			m = PHP_PROTOCOLBUFFERS_GET_OBJECT(php_protocolbuffers_message, value);
#endif
			ZVAL_ZVAL(m->container, instance, 0, 0);
		}
	}

	php_protocolbuffers_message_get_hash_table_by_container(container, scheme, instance, &htt, &n, &n_len TSRMLS_CC);

#if ZEND_MODULE_API_NO >= 20151012
	if ((*e = zend_hash_str_find(htt, n, n_len)) != NULL) {
#else
	if (zend_hash_find(htt, n, n_len, (void **)&e) == SUCCESS) {
#endif
		zval *vl = NULL;

		if (container->use_single_property > 0) {
#if ZEND_MODULE_API_NO >= 20151012
			ZVAL_ZVAL(vl, *e, 1, 0);
			php_protocolbuffers_typeconvert(scheme, vl TSRMLS_CC);

			Z_ADDREF_P(vl);
			zend_string *name = zend_string_init(scheme->name, scheme->name_len, 0);
			zend_hash_update(htt, name, vl);
			zend_string_release(name);
			zval_ptr_dtor(vl);
#else
			MAKE_STD_ZVAL(vl);
			ZVAL_ZVAL(vl, *e, 1, 0);
			php_protocolbuffers_typeconvert(scheme, vl TSRMLS_CC);

			Z_ADDREF_P(vl);
			zend_hash_update(htt, scheme->name, scheme->name_len, (void **)&vl, sizeof(zval *), NULL);
			zval_ptr_dtor(&vl);
#endif
		} else {
			zval *garvage = *e;

#if ZEND_MODULE_API_NO >= 20151012
			ZVAL_ZVAL(vl, value, 1, 0);
#else
			MAKE_STD_ZVAL(vl);
			ZVAL_ZVAL(vl, value, 1, 0);
#endif

			Z_ADDREF_P(vl);
			php_protocolbuffers_typeconvert(scheme, vl TSRMLS_CC);
			*e = vl;
#if ZEND_MODULE_API_NO >= 20151012
			zval_ptr_dtor(garvage);
			zval_ptr_dtor(vl);
#else
			zval_ptr_dtor(&garvage);
			zval_ptr_dtor(&vl);
#endif
		}
		if (should_free) {
#if ZEND_MODULE_API_NO >= 20151012
			zval_ptr_dtor(value);
#else
			zval_ptr_dtor(&value);
#endif
		}
	}
}

static void php_protocolbuffers_message_clear(INTERNAL_FUNCTION_PARAMETERS, zval *instance, php_protocolbuffers_scheme_container *container, char *name, int name_len, char *name2, int name2_len)
{
	php_protocolbuffers_scheme *scheme;
	zval **e = NULL;
	HashTable *htt;
	char *n = {0};
	int n_len = 0;

	scheme = php_protocolbuffers_message_get_scheme_by_name(container, name, name_len, name2, name2_len);
	if (scheme == NULL) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "%s does not find", name);
		return;
	}

	if (scheme->is_extension) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "clear method can't use for extension value", name);
		return;
	}

	php_protocolbuffers_message_get_hash_table_by_container(container, scheme, instance, &htt, &n, &n_len TSRMLS_CC);
#if ZEND_MODULE_API_NO >= 20151012
	if ((*e = zend_hash_str_find(htt, n, n_len)) != NULL) {
		zval *vl = NULL;
		if (container->use_single_property > 0) {
			if (scheme->repeated > 0) {
				array_init(vl);
			} else {
				ZVAL_NULL(vl);
			}
			php_protocolbuffers_typeconvert(scheme, vl TSRMLS_CC);
			
			zend_string *name = zend_string_init(scheme->name, scheme->name_len, 0);
			zend_hash_update(htt, name, vl);
			zend_string_release(name);
		} else {
			zval *garvage = *e;

			if (scheme->repeated > 0) {
				array_init(vl);
			} else {
				ZVAL_NULL(vl);
			}

			*e = vl;
			zval_ptr_dtor(garvage);
		}
	}
#else
	if (zend_hash_find(htt, n, n_len, (void **)&e) == SUCCESS) {
		zval *vl = NULL;
		if (container->use_single_property > 0) {
			MAKE_STD_ZVAL(vl);
			if (scheme->repeated > 0) {
				array_init(vl);
			} else {
				ZVAL_NULL(vl);
			}
			php_protocolbuffers_typeconvert(scheme, vl TSRMLS_CC);

			zend_hash_update(htt, scheme->name, scheme->name_len, (void **)&vl, sizeof(zval *), NULL);
		} else {
			zval *garvage = *e;

			MAKE_STD_ZVAL(vl);
			if (scheme->repeated > 0) {
				array_init(vl);
			} else {
				ZVAL_NULL(vl);
			}

			*e = vl;
			zval_ptr_dtor(&garvage);
		}
	}
#endif
}

static void php_protocolbuffers_message_append(INTERNAL_FUNCTION_PARAMETERS, zval *instance, php_protocolbuffers_scheme_container *container, char *name, int name_len, char *name2, int name2_len, zval *value)
{
	php_protocolbuffers_scheme *scheme;
	php_protocolbuffers_message *m;
	zval **e = NULL;
	HashTable *htt;
	char *n = {0};
	int n_len = 0;

	scheme = php_protocolbuffers_message_get_scheme_by_name(container, name, name_len, name2, name2_len);
	if (scheme == NULL) {
#if ZEND_MODULE_API_NO >= 20151012
		zval_ptr_dtor(value);
#else
		zval_ptr_dtor(&value);
#endif
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "%s does not find", name);
		return;
	}

	if (scheme->is_extension) {
#if ZEND_MODULE_API_NO >= 20151012
		zval_ptr_dtor(value);
#else
		zval_ptr_dtor(&value);
#endif
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "append method can't use for extension value", name);
		return;
	}

	if (scheme->repeated < 1) {
#if ZEND_MODULE_API_NO >= 20151012
		zval_ptr_dtor(value);
#else
		zval_ptr_dtor(&value);
#endif
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "append method can't use for non repeated value", name);
		return;
	}

	if (Z_TYPE_P(value) == IS_NULL) {
#if ZEND_MODULE_API_NO >= 20151012
		zval_ptr_dtor(value);
#else
		zval_ptr_dtor(&value);
#endif
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "append method can't pass null value", name);
		return;
	}

	if (scheme->ce != NULL) {
		if (scheme->ce != Z_OBJCE_P(value)) {
#if ZEND_MODULE_API_NO >= 20151012
			zval_ptr_dtor(value);
#else
			zval_ptr_dtor(&value);
#endif
			zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "expected %s class. given %s class", scheme->ce->name, Z_OBJCE_P(value)->name);
			return;
		}
#if ZEND_MODULE_API_NO >= 20151012
		m = (php_protocolbuffers_message *) ((char*)Z_OBJ_P(value) - XtOffsetOf(php_protocolbuffers_message, zo)); 
#else
		m = PHP_PROTOCOLBUFFERS_GET_OBJECT(php_protocolbuffers_message, value);
#endif
		ZVAL_ZVAL(m->container, instance, 0, 0);
	}

	php_protocolbuffers_message_get_hash_table_by_container(container, scheme, instance, &htt, &n, &n_len TSRMLS_CC);

#if ZEND_MODULE_API_NO >= 20151012
	if (container->use_single_property > 0 && zend_hash_str_exists(htt, n, n_len) == 0) {
		zval_ptr_dtor(value);
		zend_error(E_ERROR, "not initialized");
		return;
	}
#else
	if (container->use_single_property > 0 && zend_hash_exists(htt, n, n_len) == 0) {
		zval_ptr_dtor(&value);
		zend_error(E_ERROR, "not initialized");
		return;
	}
#endif

#if ZEND_MODULE_API_NO >= 20151012
	if ((*e = zend_hash_str_find(htt, n, n_len)) != NULL) {
#else
	if (zend_hash_find(htt, n, n_len, (void **)&e) == SUCCESS) {
#endif
		zval *nval = NULL, *val = NULL;
		int flag = 0;

		if (Z_TYPE_PP(e) != IS_ARRAY) {
#if ZEND_MODULE_API_NO >= 20151012
			array_init(nval);
#else
			MAKE_STD_ZVAL(nval);
			array_init(nval);
#endif
			flag = 1;
		} else {
			nval = *e;
		}

#if ZEND_MODULE_API_NO >= 20151012
		ZVAL_ZVAL(val, value, 1, 0);
#else
		MAKE_STD_ZVAL(val);
		ZVAL_ZVAL(val, value, 1, 0);
#endif

		Z_ADDREF_P(nval);
		Z_ADDREF_P(val);
#if ZEND_MODULE_API_NO >= 20151012
		zend_hash_next_index_insert(Z_ARRVAL_P(nval), val);
		zend_string *zstr_n = zend_string_init(n, n_len, 0);
		zend_hash_update(htt, zstr_n, nval);
		zend_string_release(zstr_n);
		zval_ptr_dtor(val);
#else
		zend_hash_next_index_insert(Z_ARRVAL_P(nval), &val, sizeof(zval *), NULL);
		zend_hash_update(htt, n, n_len, (void **)&nval, sizeof(zval *), NULL);
		zval_ptr_dtor(&val);
#endif

		if (flag == 1) {
#if ZEND_MODULE_API_NO >= 20151012
			zval_ptr_dtor(nval);
#else
			zval_ptr_dtor(&nval);
#endif
		}
	}
}

static void php_protocolbuffers_message_has(INTERNAL_FUNCTION_PARAMETERS, zval *instance, php_protocolbuffers_scheme_container *container, char *name, int name_len, char *name2, int name2_len)
{
	char *n = {0};
	int n_len = 0;
	HashTable *htt = NULL;
	php_protocolbuffers_scheme *scheme;
	zval **e = NULL;

	scheme = php_protocolbuffers_message_get_scheme_by_name(container, name, name_len, name2, name2_len);
	if (scheme == NULL) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "%s does not find", name);
		return;
	}

	if (scheme->is_extension) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "has method can't use for extension value", name);
		return;
	}

	php_protocolbuffers_message_get_hash_table_by_container(container, scheme, instance, &htt, &n, &n_len TSRMLS_CC);
#if ZEND_MODULE_API_NO >= 20151012
	if ((*e = zend_hash_str_find(htt, n, n_len)) != NULL) {
#else
	if (zend_hash_find(htt, n, n_len, (void **)&e) == SUCCESS) {
#endif
		if (Z_TYPE_PP(e) == IS_NULL) {
			RETURN_FALSE;
		} else if (Z_TYPE_PP(e) == IS_ARRAY) {
			if (zend_hash_num_elements(Z_ARRVAL_PP(e)) < 1) {
				RETURN_FALSE;
			} else {
				RETURN_TRUE;
			}
		} else {
			RETURN_TRUE;
		}
	}
}

static int php_protocolbuffers_get_unknown_zval(zval **retval, php_protocolbuffers_scheme_container *container, zval *instance TSRMLS_DC)
{
	zval **unknown_fieldset = NULL;
	HashTable *target = NULL;
	char *unknown_name = {0};
	int free = 0, unknown_name_len = 0, result = 0;
	zend_string *zstr_unknown_name;

#if ZEND_MODULE_API_NO >= 20151012
	if (container->use_single_property > 0) {
		zval **unknown_property;
		if ((*unknown_property = zend_hash_str_find(Z_OBJPROP_P(instance), container->single_property_name, container->single_property_name_len)) == NULL) {
			return result;
		}

		unknown_name     = php_protocolbuffers_get_default_unknown_property_name();
		unknown_name_len = php_protocolbuffers_get_default_unknown_property_name_len();
		target = Z_ARRVAL_PP(unknown_property);

		if ((*unknown_fieldset = zend_hash_str_find(target, (char*)unknown_name, unknown_name_len)) != NULL) {
			*retval = *unknown_fieldset;
			result = 1;
		}

	} else {
		zstr_unknown_name = zend_mangle_property_name((char*)"*", 1, (char*)php_protocolbuffers_get_default_unknown_property_name(),
				php_protocolbuffers_get_default_unknown_property_name_len(), 0);
		target = Z_OBJPROP_P(instance);
		if ((*unknown_fieldset = zend_hash_find(target, zstr_unknown_name)) != NULL) {
			*retval = *unknown_fieldset;
			result = 1;
		}
		zend_string_free(zstr_unknown_name);
	}
#else
	if (container->use_single_property > 0) {
		zval **unknown_property;
		if (zend_hash_find(Z_OBJPROP_P(instance), container->single_property_name, container->single_property_name_len, (void **)&unknown_property) == FAILURE) {
			return result;
		}

		unknown_name     = php_protocolbuffers_get_default_unknown_property_name();
		unknown_name_len = php_protocolbuffers_get_default_unknown_property_name_len();
		target = Z_ARRVAL_PP(unknown_property);
	} else {
		zend_mangle_property_name(&unknown_name, &unknown_name_len, (char*)"*", 1, (char*)php_protocolbuffers_get_default_unknown_property_name(), php_protocolbuffers_get_default_unknown_property_name_len(), 0);
		target = Z_OBJPROP_P(instance);
		free = 1;
	}

	if (zend_hash_find(target, (char*)unknown_name, unknown_name_len, (void **)&unknown_fieldset) == SUCCESS) {
		*retval = *unknown_fieldset;
		result = 1;
	}

	if (free) {
		efree(unknown_name);
	}
#endif
	return result;
}

static enum ProtocolBuffers_MagicMethod php_protocolbuffers_parse_magic_method(const char *name, size_t name_len, smart_str *buf, smart_str *buf2)
{
	int i = 0;
	int last_is_capital = 0;
	enum ProtocolBuffers_MagicMethod flag = 0;

	for (i = 0; i < name_len; i++) {
		if (flag == 0) {
			if (i+2 < name_len && name[i] == 'g' && name[i+1] == 'e' && name[i+2] == 't') {
				i += 2;
				flag = MAGICMETHOD_GET;
				continue;
			} else if (i+2 < name_len && name[i] == 's' && name[i+1] == 'e' && name[i+2] == 't') {
				i += 2;
				flag = MAGICMETHOD_SET;
				continue;
			} else if (i+7 < name_len &&
				name[i] == 'm' &&
				name[i+1] == 'u' &&
				name[i+2] == 't' &&
				name[i+3] == 'a' &&
				name[i+4] == 'b' &&
				name[i+5] == 'l' &&
				name[i+6] == 'e'
			){
				i += 7;
				flag = MAGICMETHOD_MUTABLE;
			} else if (i+6 < name_len &&
				name[i] == 'a' &&
				name[i+1] == 'p' &&
				name[i+2] == 'p' &&
				name[i+3] == 'e' &&
				name[i+4] == 'n' &&
				name[i+5] == 'd'
			) {
				i += 6;
				flag = MAGICMETHOD_APPEND;
			} else if (i+5 < name_len &&
				name[i] == 'c' &&
				name[i+1] == 'l' &&
				name[i+2] == 'e' &&
				name[i+3] == 'a' &&
				name[i+4] == 'r') {
				i += 5;
				flag = MAGICMETHOD_CLEAR;
			} else if (i+3 < name_len &&
				name[i] == 'h' &&
				name[i+1] == 'a' &&
				name[i+2] == 's'
			) {
				i += 3;
				flag = MAGICMETHOD_HAS;
			} else {
				break;
			}
		}

		if (name[i] >= 'A' && name[i] <= 'Z') {
#if ZEND_MODULE_API_NO >= 20151012
			if (ZSTR_LEN(buf->s) > 0) {
				if ((last_is_capital == 0
					&& i+1 >= name_len)
					|| (i+1 < name_len && name[i+1] >= 'a' && name[i+1] <= 'z')) {
					smart_str_appendc(buf, '_');
				}
			}
#else
			if (buf->len > 0) {
				if ((last_is_capital == 0
					&& i+1 >= name_len)
					|| (i+1 < name_len && name[i+1] >= 'a' && name[i+1] <= 'z')) {
					smart_str_appendc(buf, '_');
				}
			}
#endif
			smart_str_appendc(buf, name[i] + ('a' - 'A'));
			smart_str_appendc(buf2, name[i]);
			last_is_capital = 1;
		} else {
			smart_str_appendc(buf, name[i]);
			smart_str_appendc(buf2, name[i]);
			last_is_capital = 0;
		}
	}

	smart_str_0(buf);
	smart_str_0(buf2);

	return flag;
}

static void php_protocolbuffers_set_from(INTERNAL_FUNCTION_PARAMETERS, zval *instance, zval *params)
{
	HashPosition pos;
	zval **param;
	php_protocolbuffers_scheme_container *container = NULL;
	char *key;
	uint key_len;
	unsigned long index= 0;

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME


#if ZEND_MODULE_API_NO >= 20151012
	for (zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(params), &pos);
		(*param = zend_hash_get_current_data_ex(Z_ARRVAL_P(params), &pos)) != NULL;
		zend_hash_move_forward_ex(Z_ARRVAL_P(params), &pos)) {

		zend_string *zstr_key = zend_string_init(key, key_len, 0);
		if (zend_hash_get_current_key_ex(Z_ARRVAL_P(params), &zstr_key, &index, &pos) == HASH_KEY_IS_STRING) {
			php_protocolbuffers_message_set(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, key, key_len, key, key_len, *param);
		}
		zend_string_release(zstr_key);
	}
#else
	for (zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(params), &pos);
		zend_hash_get_current_data_ex(Z_ARRVAL_P(params), (void **)&param, &pos) == SUCCESS;
		zend_hash_move_forward_ex(Z_ARRVAL_P(params), &pos)) {

		if (zend_hash_get_current_key_ex(Z_ARRVAL_P(params), &key, &key_len, &index, 0, &pos) == HASH_KEY_IS_STRING) {
			php_protocolbuffers_message_set(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, key, key_len, key, key_len, *param);
		}
	}
#endif
}

/* {{{ proto ProtocolBuffersMessage ProtocolBuffersMessage::__construct([array $params])
*/
PHP_METHOD(protocolbuffers_message, __construct)
{
	zval *params = NULL, *instance = getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"|a", &params) == FAILURE) {
		return;
	}

	if (php_protocolbuffers_properties_init(instance, Z_OBJCE_P(instance) TSRMLS_CC)) {
		return;
	}

	if (params != NULL) {
		php_protocolbuffers_set_from(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, params);
	}
}
/* }}} */

/* {{{ proto ProtocolBuffersMessage ProtocolBuffersMessage::setFrom(array $params)
*/
PHP_METHOD(protocolbuffers_message, setFrom)
{
	zval *params = NULL, *instance = getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"a", &params) == FAILURE) {
		return;
	}

	php_protocolbuffers_set_from(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, params);
}
/* }}} */


/* {{{ proto mixed ProtocolBuffersMessage::serializeToString()
*/
PHP_METHOD(protocolbuffers_message, serializeToString)
{
	zval *instance = getThis();

	php_protocolbuffers_encode(INTERNAL_FUNCTION_PARAM_PASSTHRU, Z_OBJCE_P(instance), instance);
}
/* }}} */

/* {{{ proto mixed ProtocolBuffersMessage::parseFromString()
*/
PHP_METHOD(protocolbuffers_message, parseFromString)
{
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 3)
	/* I tried to lookup current scope from EG(current_execute_data). but it doesn't have current scope. we can't do anymore */
	zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "ProtocolBuffersMessage::parseFromString can't work under PHP 5.3. please use ProtocolBuffers:decode directly");
	return;
#else
	const char *data;
	int data_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s", &data, &data_len) == FAILURE) {
		return;
	}
#if ZEND_MODULE_API_NO >= 20151012
	if (EG(current_execute_data)->called_scope) {
		php_protocolbuffers_decode(INTERNAL_FUNCTION_PARAM_PASSTHRU, data, data_len, ZSTR_VAL(EG(current_execute_data)->called_scope->name), ZSTR_LEN(EG(current_execute_data)->called_scope->name));
	} else {
		zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "Missing EG(current_scope). this is bug");
	}
#else
	if (EG(called_scope)) {
		php_protocolbuffers_decode(INTERNAL_FUNCTION_PARAM_PASSTHRU, data, data_len, EG(called_scope)->name, EG(called_scope)->name_length);
	} else {
		zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "Missing EG(current_scope). this is bug");
	}
#endif
#endif
}
/* }}} */

/* {{{ proto mixed ProtocolBuffersMessage::mergeFrom($message)
*/
PHP_METHOD(protocolbuffers_message, mergeFrom)
{
	zval *object = NULL, *instance = getThis();
	php_protocolbuffers_scheme_container *container = NULL;
	char *n = {0};
	int n_len = 0;
	HashTable *htt = NULL, *hts = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"z", &object) == FAILURE) {
		return;
	}

	if (Z_TYPE_P(object) != IS_OBJECT) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "%s::mergeFrom expects %s class", Z_OBJCE_P(instance)->name, Z_OBJCE_P(instance)->name);
		return;
	}

	if (Z_OBJCE_P(object) != Z_OBJCE_P(instance)) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "%s::mergeFrom expects %s class, but %s given", Z_OBJCE_P(instance)->name, Z_OBJCE_P(instance)->name, Z_OBJCE_P(object)->name);
		return;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME

	php_protocolbuffers_get_hash(container, container->scheme, instance, &n, &n_len, &htt TSRMLS_CC);
	php_protocolbuffers_get_hash(container, container->scheme, object, &n, &n_len, &hts TSRMLS_CC);
	php_protocolbuffers_message_merge_from(container, htt, hts TSRMLS_CC);
}
/* }}} */


/* {{{ proto mixed ProtocolBuffersMessage::current()
*/
PHP_METHOD(protocolbuffers_message, current)
{
	zval **tmp = NULL, *instance = getThis();
	php_protocolbuffers_scheme_container *container;
	php_protocolbuffers_message *message;
	const char *name;
	int name_len = 0;
	HashTable *hash;

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
#if ZEND_MODULE_API_NO >= 20151012
	message = (php_protocolbuffers_message *) ((char*)Z_OBJ_P(instance) - XtOffsetOf(php_protocolbuffers_message, zo)); 
#else
	message = PHP_PROTOCOLBUFFERS_GET_OBJECT(php_protocolbuffers_message, instance);
#endif

	if (container->use_single_property < 1) {
		name     = container->scheme[message->offset].mangled_name;
		name_len = container->scheme[message->offset].mangled_name_len;

		hash = Z_OBJPROP_P(instance);
	} else {
		zval **c;

		name     = container->scheme[message->offset].name;
		name_len = container->scheme[message->offset].name_len;

#if ZEND_MODULE_API_NO >= 20151012
		if ((*c = zend_hash_str_find(Z_OBJPROP_P(instance), container->single_property_name, container->single_property_name_len)) != NULL) {
#else
		if (zend_hash_find(Z_OBJPROP_P(instance), container->single_property_name, container->single_property_name_len, (void**)&c) == SUCCESS) {
#endif
			hash = Z_ARRVAL_PP(c);
		}

		hash = Z_OBJPROP_PP(c);
	}

#if ZEND_MODULE_API_NO >= 20151012
	if ((*tmp = zend_hash_str_find(hash, name, name_len)) != NULL) {
#else
	if (zend_hash_find(hash, name, name_len, (void **)&tmp) == SUCCESS) {
#endif
		RETVAL_ZVAL(*tmp, 1, 0);
	}
}
/* }}} */

/* {{{ proto mixed ProtocolBuffersMessage::key()
*/
PHP_METHOD(protocolbuffers_message, key)
{
	zval *instance = getThis();
	php_protocolbuffers_scheme_container *container;
	php_protocolbuffers_message *message;

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
#if ZEND_MODULE_API_NO >= 20151012
	message = (php_protocolbuffers_message *) ((char*)Z_OBJ_P(instance) - XtOffsetOf(php_protocolbuffers_message, zo)); 

	RETURN_STRING(container->scheme[message->offset].name);
#else
	message = PHP_PROTOCOLBUFFERS_GET_OBJECT(php_protocolbuffers_message, instance);

	RETURN_STRING(container->scheme[message->offset].name, 1);
#endif
}
/* }}} */

/* {{{ proto mixed ProtocolBuffersMessage::next()
*/
PHP_METHOD(protocolbuffers_message, next)
{
	zval *instance = getThis();
	php_protocolbuffers_scheme_container *container;
	php_protocolbuffers_message *message;

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
#if ZEND_MODULE_API_NO >= 20151012
	message = (php_protocolbuffers_message *) ((char*)Z_OBJ_P(instance) - XtOffsetOf(php_protocolbuffers_message, zo)); 
#else
	message = PHP_PROTOCOLBUFFERS_GET_OBJECT(php_protocolbuffers_message, instance);
#endif
	message->offset++;
}
/* }}} */

/* {{{ proto void ProtocolBuffersMessage::rewind()
*/
PHP_METHOD(protocolbuffers_message, rewind)
{
	zval *instance = getThis();
	php_protocolbuffers_scheme_container *container;
	php_protocolbuffers_message *message;

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
#if ZEND_MODULE_API_NO >= 20151012
	message = (php_protocolbuffers_message *) ((char*)Z_OBJ_P(instance) - XtOffsetOf(php_protocolbuffers_message, zo)); 
#else
	message = PHP_PROTOCOLBUFFERS_GET_OBJECT(php_protocolbuffers_message, instance);
#endif

	if (message->max == 0) {
		message->max = container->size;
	}
	message->offset = 0;
}
/* }}} */

/* {{{ proto bool ProtocolBuffersMessage::valid()
*/
PHP_METHOD(protocolbuffers_message, valid)
{
	zval *instance = getThis();
	php_protocolbuffers_scheme_container *container;
	php_protocolbuffers_message *message;

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
#if ZEND_MODULE_API_NO >= 20151012
	message = (php_protocolbuffers_message *) ((char*)Z_OBJ_P(instance) - XtOffsetOf(php_protocolbuffers_message, zo)); 
#else
	message = PHP_PROTOCOLBUFFERS_GET_OBJECT(php_protocolbuffers_message, instance);
#endif

	if (-1 < message->offset && message->offset < message->max) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto void ProtocolBuffersMessage::clear(string $name)
*/
PHP_METHOD(protocolbuffers_message, clear)
{
	zval *instance = getThis();
	char *name = {0};
	int name_len = 0;
	php_protocolbuffers_scheme_container *container;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s", &name, &name_len) == FAILURE) {
		return;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	php_protocolbuffers_message_clear(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, name, name_len, name, name_len);
}
/* }}} */

/* {{{ proto void ProtocolBuffersMessage::clearAll(bool $clear_unknown_fields = true)
*/
PHP_METHOD(protocolbuffers_message, clearAll)
{
	zval *instance = getThis();
	php_protocolbuffers_scheme_container *container;
	zend_bool clear_unknown_fields = 1;
	int i = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"|b", &clear_unknown_fields) == FAILURE) {
		return;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME

	for (i = 0; i < container->size; i++) {
		php_protocolbuffers_message_clear(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, container->scheme[i].name, container->scheme[i].name_len, NULL, 0);
	}

	if (clear_unknown_fields > 0 && container->process_unknown_fields > 0) {
		zval *unknown_fieldset = NULL;

		if (php_protocolbuffers_get_unknown_zval(&unknown_fieldset, container, instance TSRMLS_CC)) {
			php_protocolbuffers_unknown_field_clear(INTERNAL_FUNCTION_PARAM_PASSTHRU, unknown_fieldset);
		}
	}
}
/* }}} */

/* {{{ proto bool ProtocolBuffersMessage::__call()
*/
PHP_METHOD(protocolbuffers_message, __call)
{
	zval *params = NULL, *instance = getThis();
	php_protocolbuffers_scheme_container *container;
	char *name = {0};
	int flag = 0, name_len = 0;
	smart_str buf = {0};
	smart_str buf2 = {0};

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"sz", &name, &name_len, &params) == FAILURE) {
		return;
	}

#if ZEND_MODULE_API_NO >= 20151012
	flag = php_protocolbuffers_parse_magic_method(name, name_len, &buf, &buf2);
	if (flag == 0) {
		zend_error(E_ERROR, "Call to undefined method %s::%s()", Z_OBJCE_P(instance)->name, name);
		return;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	switch (flag) {
		case MAGICMETHOD_GET:
		{
			zval **tmp = NULL;
			if (params != NULL && Z_TYPE_P(params) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(params)) > 0) {
				zend_hash_get_current_data(Z_ARRVAL_P(params));
				php_protocolbuffers_message_get(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, ZSTR_VAL(buf.s), ZSTR_LEN(buf.s), ZSTR_VAL(buf2.s), ZSTR_LEN(buf2.s), *tmp);
			} else {
				php_protocolbuffers_message_get(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, ZSTR_VAL(buf.s), ZSTR_LEN(buf.s), ZSTR_VAL(buf2.s), ZSTR_LEN(buf2.s), NULL);
			}
		}
		break;
		case MAGICMETHOD_MUTABLE:
		{
			php_protocolbuffers_message_get(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, ZSTR_VAL(buf.s), ZSTR_LEN(buf.s), ZSTR_VAL(buf2.s), ZSTR_LEN(buf2.s), NULL);
			php_protocolbuffers_message_set(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, ZSTR_VAL(buf.s), ZSTR_LEN(buf.s), ZSTR_VAL(buf2.s), ZSTR_LEN(buf2.s), return_value);
		}
		break;
		case MAGICMETHOD_SET:
		{
			zval **tmp = NULL;

			zend_hash_get_current_data(Z_ARRVAL_P(params));
			php_protocolbuffers_message_set(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, ZSTR_VAL(buf.s), ZSTR_LEN(buf.s), ZSTR_VAL(buf2.s), ZSTR_LEN(buf2.s), *tmp);
		}
		break;
		case MAGICMETHOD_APPEND:
		{
			zval **tmp = NULL;

			zend_hash_get_current_data(Z_ARRVAL_P(params));
			php_protocolbuffers_message_append(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, ZSTR_VAL(buf.s), ZSTR_LEN(buf.s), ZSTR_VAL(buf2.s), ZSTR_LEN(buf2.s), *tmp);
		}
		break;
		case MAGICMETHOD_CLEAR:
			php_protocolbuffers_message_clear(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, ZSTR_VAL(buf.s), ZSTR_LEN(buf.s), ZSTR_VAL(buf2.s), ZSTR_LEN(buf2.s));
		case MAGICMETHOD_HAS:
			php_protocolbuffers_message_has(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, ZSTR_VAL(buf.s), ZSTR_LEN(buf.s), ZSTR_VAL(buf2.s), ZSTR_LEN(buf2.s));
		break;
	}
#else
	flag = php_protocolbuffers_parse_magic_method(name, name_len, &buf, &buf2);
	if (flag == 0) {
		zend_error(E_ERROR, "Call to undefined method %s::%s()", Z_OBJCE_P(instance)->name, name);
		return;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	switch (flag) {
		case MAGICMETHOD_GET:
		{
			zval **tmp = NULL;
			if (params != NULL && Z_TYPE_P(params) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(params)) > 0) {
				zend_hash_get_current_data(Z_ARRVAL_P(params), (void **)&tmp);
				php_protocolbuffers_message_get(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, buf.c, buf.len, buf2.c, buf2.len, *tmp);
			} else {
				php_protocolbuffers_message_get(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, buf.c, buf.len, buf2.c, buf2.len, NULL);
			}
		}
		break;
		case MAGICMETHOD_MUTABLE:
		{
			php_protocolbuffers_message_get(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, buf.c, buf.len, buf2.c, buf2.len, NULL);
			php_protocolbuffers_message_set(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, buf.c, buf.len, buf2.c, buf2.len, return_value);
		}
		break;
		case MAGICMETHOD_SET:
		{
			zval **tmp = NULL;

			zend_hash_get_current_data(Z_ARRVAL_P(params), (void **)&tmp);
			php_protocolbuffers_message_set(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, buf.c, buf.len, buf2.c, buf2.len, *tmp);
		}
		break;
		case MAGICMETHOD_APPEND:
		{
			zval **tmp = NULL;

			zend_hash_get_current_data(Z_ARRVAL_P(params), (void **)&tmp);
			php_protocolbuffers_message_append(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, buf.c, buf.len, buf2.c, buf2.len, *tmp);
		}
		break;
		case MAGICMETHOD_CLEAR:
			php_protocolbuffers_message_clear(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, buf.c, buf.len, buf2.c, buf2.len);
		case MAGICMETHOD_HAS:
			php_protocolbuffers_message_has(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, buf.c, buf.len, buf2.c, buf2.len);
		break;
	}
#endif

	smart_str_free(&buf);
	smart_str_free(&buf2);
}
/* }}} */

/* {{{ proto ProtocolBuffers_FieldDescriptor ProtocolBuffersMessage::has($key)
*/
PHP_METHOD(protocolbuffers_message, has)
{
	zval *instance = getThis();
	char *name = {0};
	int name_len = 0;
	php_protocolbuffers_scheme_container *container;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s", &name, &name_len) == FAILURE) {
		return;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	php_protocolbuffers_message_has(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, name, name_len, name, name_len);
}
/* }}} */

/* {{{ proto ProtocolBuffers_FieldDescriptor ProtocolBuffersMessage::get($key)
*/
PHP_METHOD(protocolbuffers_message, get)
{
	zval *instance = getThis();
	char *name = {0};
	int name_len = 0;
	php_protocolbuffers_scheme_container *container;
	zval *params = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s|z", &name, &name_len, &params) == FAILURE) {
		return;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	php_protocolbuffers_message_get(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, name, name_len, name, name_len, params);
}
/* }}} */

/* {{{ proto ProtocolBuffersMessage ProtocolBuffersMessage::mutable($key)
*/
PHP_METHOD(protocolbuffers_message, mutable)
{
	zval *instance = getThis();
	char *name = {0};
	int name_len = 0;
	php_protocolbuffers_scheme_container *container;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s", &name, &name_len) == FAILURE) {
		return;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	php_protocolbuffers_message_get(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, name, name_len, name, name_len, NULL);
	Z_ADDREF_P(return_value);
	php_protocolbuffers_message_set(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, name, name_len, name, name_len, return_value);
}
/* }}} */

/* {{{ proto void ProtocolBuffersMessage::set($name, $value)
*/
PHP_METHOD(protocolbuffers_message, set)
{
	zval *value = NULL, *instance = getThis();
	char *name = {0};
	int name_len = 0;
	php_protocolbuffers_scheme_container *container;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"sz", &name, &name_len, &value) == FAILURE) {
		return;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	php_protocolbuffers_message_set(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, name, name_len, name, name_len, value);
}


/* {{{ proto void ProtocolBuffersMessage::append($name, $value)
*/
PHP_METHOD(protocolbuffers_message, append)
{
	zval *value = NULL, *instance = getThis();
	char *name = {0};
	int name_len = 0;
	php_protocolbuffers_scheme_container *container;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"sz", &name, &name_len, &value) == FAILURE) {
		return;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	php_protocolbuffers_message_append(INTERNAL_FUNCTION_PARAM_PASSTHRU, instance, container, name, name_len, name, name_len, value);
}



/* {{{ proto mixed ProtocolBuffersMessage::getExtension(string $name)
*/
PHP_METHOD(protocolbuffers_message, getExtension)
{
	zval *instance = getThis();
	zval *registry = php_protocolbuffers_extension_registry_get_instance(TSRMLS_C);
	zval **e = {0}, **b = {0}, *field_descriptor = NULL, *extension_registry = NULL;
	zend_class_entry *ce;
	HashTable *htt = NULL;
	char *name = {0}, *n = {0};
	int name_len = 0, n_len = 0;
	php_protocolbuffers_scheme_container *container;
	int is_mangled = 0;
	zend_string *zstr_n;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s", &name, &name_len) == FAILURE) {
		return;
	}

	ce = Z_OBJCE_P(instance);
#if ZEND_MODULE_API_NO >= 20151012
	if (!php_protocolbuffers_extension_registry_get_registry(registry, ZSTR_VAL(ce->name), ZSTR_LEN(ce->name), &extension_registry TSRMLS_CC)) {
		goto err;
	}

	if (!php_protocolbuffers_extension_registry_get_descriptor_by_name(extension_registry, name, name_len, &field_descriptor TSRMLS_CC)) {
		goto err;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	if (container->use_single_property > 0) {
		if ((*b = zend_hash_str_find(Z_OBJPROP_P(instance), container->single_property_name, container->single_property_name_len)) == NULL) {
			return;
		}

		n = name;
		n_len = name_len;
		htt = Z_ARRVAL_PP(b);
		if ((*e = zend_hash_str_find(htt, n, n_len)) != NULL) {
			if (Z_TYPE_PP(e) == IS_NULL) {
				int x = 0;
				for (x = 0; x < container->size; x++) {
					php_protocolbuffers_scheme *scheme;

					scheme = &container->scheme[x];
					if (scheme->ce != NULL && strcmp(scheme->name, name) == 0) {
						zval *tmp;
						object_init_ex(tmp, scheme->ce);
						php_protocolbuffers_properties_init(tmp, scheme->ce TSRMLS_CC);

						RETURN_ZVAL(tmp, 0, 1);
						break;
					}
				}
			}

			RETURN_ZVAL(*e, 1, 0);
		}
	} else {
		htt = Z_OBJPROP_P(instance);
		zstr_n = zend_mangle_property_name((char*)"*", 1, (char*)name, name_len+1, 0);
		if ((*e = zend_hash_find(htt, zstr_n)) != NULL) {
			zend_string_free(zstr_n);
			if (Z_TYPE_PP(e) == IS_NULL) {
				int x = 0;
				for (x = 0; x < container->size; x++) {
					php_protocolbuffers_scheme *scheme;

					scheme = &container->scheme[x];
					if (scheme->ce != NULL && strcmp(scheme->name, name) == 0) {
						zval *tmp;
						object_init_ex(tmp, scheme->ce);
						php_protocolbuffers_properties_init(tmp, scheme->ce TSRMLS_CC);

						RETURN_ZVAL(tmp, 0, 1);
						break;
					}
				}
			}

			RETURN_ZVAL(*e, 1, 0);
		}
	}
#else
	if (!php_protocolbuffers_extension_registry_get_registry(registry, ce->name, ce->name_length, &extension_registry TSRMLS_CC)) {
		goto err;
	}

	if (!php_protocolbuffers_extension_registry_get_descriptor_by_name(extension_registry, name, name_len, &field_descriptor TSRMLS_CC)) {
		goto err;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	if (container->use_single_property > 0) {
		if (zend_hash_find(Z_OBJPROP_P(instance), container->single_property_name, container->single_property_name_len, (void **)&b) == FAILURE) {
			return;
		}

		n = name;
		n_len = name_len;
		htt = Z_ARRVAL_PP(b);
	} else {
		htt = Z_OBJPROP_P(instance);
		zend_mangle_property_name(&n, &n_len, (char*)"*", 1, (char*)name, name_len+1, 0);
		is_mangled = 1;
	}

	if (zend_hash_find(htt, n, n_len, (void **)&e) == SUCCESS) {
		if (is_mangled) {
			efree(n);
		}

		if (Z_TYPE_PP(e) == IS_NULL) {
			int x = 0;
			for (x = 0; x < container->size; x++) {
				php_protocolbuffers_scheme *scheme;

				scheme = &container->scheme[x];
				if (scheme->ce != NULL && strcmp(scheme->name, name) == 0) {
					zval *tmp;
					MAKE_STD_ZVAL(tmp);
					object_init_ex(tmp, scheme->ce);
					php_protocolbuffers_properties_init(tmp, scheme->ce TSRMLS_CC);

					RETURN_ZVAL(tmp, 0, 1);
					break;
				}
			}
		}

		RETURN_ZVAL(*e, 1, 0);
	}
#endif
	return;
err:
	zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "extension %s does not find", name);
}
/* }}} */

/* {{{ proto bool ProtocolBuffersMessage::hasExtension(string $name)
*/
PHP_METHOD(protocolbuffers_message, hasExtension)
{
	zval *instance = getThis();
	zval *registry = php_protocolbuffers_extension_registry_get_instance(TSRMLS_C);
	zval **e = NULL, **b = NULL, *field_descriptor = NULL, *extension_registry = NULL;
	zend_class_entry *ce;
	HashTable *htt = NULL;
	char *name = {0}, *n = {0};
	int name_len = 0, n_len = 0;
	php_protocolbuffers_scheme_container *container;
	int is_mangled = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s", &name, &name_len) == FAILURE) {
		return;
	}

	ce = Z_OBJCE_P(instance);

#if ZEND_MODULE_API_NO >= 20151012
	zend_string *zstr_n;
	if (!php_protocolbuffers_extension_registry_get_registry(registry, ZSTR_VAL(ce->name), ZSTR_LEN(ce->name), &extension_registry TSRMLS_CC)) {
		goto err;
	}

	if (!php_protocolbuffers_extension_registry_get_descriptor_by_name(extension_registry, name, name_len, &field_descriptor TSRMLS_CC)) {
		goto err;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	if (container->use_single_property > 0) {
		if ((*b = zend_hash_str_find(Z_OBJPROP_P(instance), container->single_property_name, container->single_property_name_len)) == NULL) {
			return;
		}

		n = name;
		n_len = name_len;
		htt = Z_ARRVAL_PP(b);
		if ((*e = zend_hash_str_find(htt, n, n_len)) != NULL) {
			if (Z_TYPE_PP(e) == IS_FALSE) {
				RETURN_FALSE;
			} else {
				RETURN_TRUE;
			}
		} else {
			RETURN_FALSE;
		}
	} else {
		htt = Z_OBJPROP_P(instance);
		zstr_n = zend_mangle_property_name((char*)"*", 1, (char*)name, name_len+1, 0);
		if ((*e = zend_hash_find(htt, zstr_n)) != NULL) {
			zend_string_free(zstr_n);
			if (Z_TYPE_PP(e) == IS_FALSE) {
				RETURN_FALSE;
			} else {
				RETURN_TRUE;
			}
		} else {
			RETURN_FALSE;
		}
	}
#else
	if (!php_protocolbuffers_extension_registry_get_registry(registry, ce->name, ce->name_length, &extension_registry TSRMLS_CC)) {
		goto err;
	}

	if (!php_protocolbuffers_extension_registry_get_descriptor_by_name(extension_registry, name, name_len, &field_descriptor TSRMLS_CC)) {
		goto err;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	if (container->use_single_property > 0) {
		if (zend_hash_find(Z_OBJPROP_P(instance), container->single_property_name, container->single_property_name_len, (void **)&b) == FAILURE) {
			return;
		}

		n = name;
		n_len = name_len;
		htt = Z_ARRVAL_PP(b);
	} else {
		htt = Z_OBJPROP_P(instance);
		zend_mangle_property_name(&n, &n_len, (char*)"*", 1, (char*)name, name_len+1, 0);
		is_mangled = 1;
	}

	if (zend_hash_find(htt, n, n_len, (void **)&e) == SUCCESS) {
		if (is_mangled) {
			efree(n);
		}
		if (Z_TYPE_PP(e) == IS_NULL) {
			RETURN_FALSE;
		} else {
			RETURN_TRUE;
		}
	} else {
		RETURN_FALSE;
	}
#endif
err:
	zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "extension %s does not find", name);
}
/* }}} */

/* {{{ proto void ProtocolBuffersMessage::setExtension(string $name, mixed $value)
*/
PHP_METHOD(protocolbuffers_message, setExtension)
{
	zval *instance = getThis();
	zval *registry = php_protocolbuffers_extension_registry_get_instance(TSRMLS_C);
	zval *field_descriptor = NULL, *extension_registry = NULL, *value = NULL, **b = NULL;
	zend_class_entry *ce;
	HashTable *htt = NULL;
	char *name = {0}, *n = {0};
	int name_len = 0, n_len = 0;
	php_protocolbuffers_scheme_container *container;
	int is_mangled = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"sz", &name, &name_len, &value) == FAILURE) {
		return;
	}

	ce = Z_OBJCE_P(instance);

#if ZEND_MODULE_API_NO >= 20151012
	if (!php_protocolbuffers_extension_registry_get_registry(registry, ZSTR_VAL(ce->name), ZSTR_LEN(ce->name), &extension_registry TSRMLS_CC)) {
		goto err;
	}

	if (!php_protocolbuffers_extension_registry_get_descriptor_by_name(extension_registry, name, name_len, &field_descriptor TSRMLS_CC)) {
		goto err;
	}

	zend_string *zstr_n;
	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	if (container->use_single_property > 0) {
		if ((*b = zend_hash_str_find(Z_OBJPROP_P(instance), container->single_property_name, container->single_property_name_len)) == NULL) {
			return;
		}

		n = name;
		n_len = name_len+1;
		htt = Z_ARRVAL_PP(b);
		Z_ADDREF_P(value);
		zstr_n = zend_string_init(n, n_len, 0);
		zend_hash_update(htt, zstr_n, value);
		zend_string_release(zstr_n);
	} else {
		htt = Z_OBJPROP_P(instance);
		zstr_n = zend_mangle_property_name((char*)"*", 1, (char*)name, name_len+1, 0);
		Z_ADDREF_P(value);
		zend_hash_update(htt, zstr_n, value);
	}
#else
	if (!php_protocolbuffers_extension_registry_get_registry(registry, ce->name, ce->name_length, &extension_registry TSRMLS_CC)) {
		goto err;
	}

	if (!php_protocolbuffers_extension_registry_get_descriptor_by_name(extension_registry, name, name_len, &field_descriptor TSRMLS_CC)) {
		goto err;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	if (container->use_single_property > 0) {
		if (zend_hash_find(Z_OBJPROP_P(instance), container->single_property_name, container->single_property_name_len, (void **)&b) == FAILURE) {
			return;
		}

		n = name;
		n_len = name_len+1;
		htt = Z_ARRVAL_PP(b);
	} else {
		htt = Z_OBJPROP_P(instance);
		zend_mangle_property_name(&n, &n_len, (char*)"*", 1, (char*)name, name_len+1, 0);
		is_mangled = 1;
	}

	Z_ADDREF_P(value);
	zend_hash_update(htt, n, n_len, (void **)&value, sizeof(zval *), NULL);
	if (is_mangled) {
		efree(n);
	}
#endif
	return;
err:
	zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "extension %s does not find", name);
}
/* }}} */

/* {{{ proto void ProtocolBuffersMessage::setExtension(string $name)
*/
PHP_METHOD(protocolbuffers_message, clearExtension)
{
	zval *instance = getThis();
	zval *registry = php_protocolbuffers_extension_registry_get_instance(TSRMLS_C);
	zval **e = {0}, **b = {0}, *field_descriptor = NULL, *extension_registry = NULL;
	zend_class_entry *ce;
	HashTable *htt = NULL;
	char *name = {0}, *n = {0};
	int name_len = 0, n_len = 0;
	php_protocolbuffers_scheme_container *container;
	int is_mangled = 0;
	zend_string *zstr_n;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s", &name, &name_len) == FAILURE) {
		return;
	}

	ce = Z_OBJCE_P(instance);

#if ZEND_MODULE_API_NO >= 20151012
	if (!php_protocolbuffers_extension_registry_get_registry(registry, ZSTR_VAL(ce->name), ZSTR_LEN(ce->name), &extension_registry TSRMLS_CC)) {
		goto err;
	}

	if (!php_protocolbuffers_extension_registry_get_descriptor_by_name(extension_registry, name, name_len, &field_descriptor TSRMLS_CC)) {
		goto err;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	if (container->use_single_property > 0) {
		if ((*b = zend_hash_str_find(Z_OBJPROP_P(instance), container->single_property_name, container->single_property_name_len)) == NULL) {
			return;
		}

		n = name;
		n_len = name_len;
		htt = Z_ARRVAL_PP(b);
		if ((*e = zend_hash_str_find(htt, n, n_len)) != NULL) {
			zval *tmp;
			zval_ptr_dtor(*e);
			ZVAL_NULL(tmp);
			*e = tmp;
			RETURN_NULL();
		}
	} else {
		htt = Z_OBJPROP_P(instance);
		zstr_n = zend_mangle_property_name((char*)"*", 1, (char*)name, name_len+1, 0);
		if ((*e = zend_hash_find(htt, zstr_n)) != NULL) {
			zval *tmp;
			zend_string_free(zstr_n);
			zval_ptr_dtor(*e);
			ZVAL_NULL(tmp);
			*e = tmp;
			RETURN_NULL();
		}
	}

#else
	if (!php_protocolbuffers_extension_registry_get_registry(registry, ce->name, ce->name_length, &extension_registry TSRMLS_CC)) {
		goto err;
	}

	if (!php_protocolbuffers_extension_registry_get_descriptor_by_name(extension_registry, name, name_len, &field_descriptor TSRMLS_CC)) {
		goto err;
	}

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	if (container->use_single_property > 0) {
		if (zend_hash_find(Z_OBJPROP_P(instance), container->single_property_name, container->single_property_name_len, (void **)&b) == FAILURE) {
			return;
		}

		n = name;
		n_len = name_len;
		htt = Z_ARRVAL_PP(b);
	} else {
		htt = Z_OBJPROP_P(instance);
		zend_mangle_property_name(&n, &n_len, (char*)"*", 1, (char*)name, name_len+1, 0);
		is_mangled = 1;
	}

	if (zend_hash_find(htt, n, n_len, (void **)&e) == SUCCESS) {
		zval *tmp;
		if (is_mangled) {
			efree(n);
		}
		zval_ptr_dtor(e);
		MAKE_STD_ZVAL(tmp);
		ZVAL_NULL(tmp);
		*e = tmp;
		RETURN_NULL();
	}
#endif

	return;
err:
	zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "extension %s does not find", name);
}
/* }}} */

/* {{{ proto void ProtocolBuffersMessage::discardUnknownFields()
*/
PHP_METHOD(protocolbuffers_message, discardUnknownFields)
{
	zval *unknown_fieldset, *instance = getThis();
	php_protocolbuffers_scheme_container *container;

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME
	if (container->process_unknown_fields > 0) {
		if (php_protocolbuffers_get_unknown_zval(&unknown_fieldset, container, instance TSRMLS_CC)) {
			php_protocolbuffers_unknown_field_clear(INTERNAL_FUNCTION_PARAM_PASSTHRU, unknown_fieldset);
		}
	}
}
/* }}} */

/* {{{ proto ProtocolBuffersUnknownFieldSet ProtocolBuffersMessage::getUnknownFieldSet()
*/
PHP_METHOD(protocolbuffers_message, getUnknownFieldSet)
{
	zval *unknown_fieldset = NULL, *instance = getThis();
	php_protocolbuffers_scheme_container *container;

	PHP_PROTOCOLBUFFERS_MESSAGE_CHECK_SCHEME

	if (container->process_unknown_fields < 1) {
		zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "process unknown fields flag seems false");
		return;
	}

	if (php_protocolbuffers_get_unknown_zval(&unknown_fieldset, container, instance TSRMLS_CC)) {
		RETVAL_ZVAL(unknown_fieldset, 1, 0);
	} else {
		zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "unknown field property does not find");
	}
}
/* }}} */


/* {{{ proto ProtocolBuffersMessage ProtocolBuffersMessage::containerOf()
*/
PHP_METHOD(protocolbuffers_message, containerOf)
{
	zval *instance = getThis();
	php_protocolbuffers_message *m;

#if ZEND_MODULE_API_NO >= 20151012
	m = (php_protocolbuffers_message *) ((char*)Z_OBJ_P(instance) - XtOffsetOf(php_protocolbuffers_message, zo)); 
#else
	m = PHP_PROTOCOLBUFFERS_GET_OBJECT(php_protocolbuffers_message, instance);
#endif

	if (m->container != NULL) {
		RETURN_ZVAL(m->container, 1, 0);
	}
}
/* }}} */

/* {{{ proto array ProtocolBuffersMessage::jsonSerialize()
*/
PHP_METHOD(protocolbuffers_message, jsonSerialize)
{
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4)
	zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "JsonSerializable does not support on this version");
#else
	zval *instance = getThis(), *result = NULL;
	zend_class_entry **json;

	if (json_serializable_checked == 0) {
		/* checks JsonSerializable class (for json dynamic module)*/
#if ZEND_MODULE_API_NO >= 20151012
		zend_string *json_class = zend_string_init(ZEND_STRS("JsonSerializable"), 0);
		if ((*json = zend_lookup_class(json_class)) != NULL) {
			if (!instanceof_function(php_protocol_buffers_message_class_entry, *json TSRMLS_CC)) {
				zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "JsonSerializable does not support on this version (probably json module doesn't load)");
				zend_string_release(json_class);
				return;
			}
		}
		zend_string_release(json_class);
#else
		if (zend_lookup_class("JsonSerializable", sizeof("JsonSerializable")-1, &json TSRMLS_CC) != FAILURE) {
			if (!instanceof_function(php_protocol_buffers_message_class_entry, *json TSRMLS_CC)) {
				zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "JsonSerializable does not support on this version (probably json module doesn't load)");
				return;
			}
		}
#endif
		json_serializable_checked = 1;
	}

	if (php_protocolbuffers_jsonserialize(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, Z_OBJCE_P(instance), instance, &result) == 0) {
		RETURN_ZVAL(result, 0, 1);
	}
	return;
#endif
}
/* }}} */

/* {{{ proto array ProtocolBuffersMessage::toArray()
*/
PHP_METHOD(protocolbuffers_message, toArray)
{
	zval *instance = getThis(), *result = NULL;

	if (php_protocolbuffers_jsonserialize(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1, Z_OBJCE_P(instance), instance, &result) == 0) {
		RETURN_ZVAL(result, 0, 1);
	}
	return;
}
/* }}} */

static zend_function_entry php_protocolbuffers_message_methods[] = {
	PHP_ME(protocolbuffers_message, __construct,          arginfo_protocolbuffers_message___construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(protocolbuffers_message, serializeToString,    arginfo_protocolbuffers_message_serialize_to_string, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, parseFromString,      arginfo_protocolbuffers_message_parse_from_string, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(protocolbuffers_message, mergeFrom,            arginfo_protocolbuffers_message_merge_from, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, clear,                arginfo_protocolbuffers_message_clear, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, clearAll,             NULL, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, has,                  arginfo_protocolbuffers_message_has, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, get,                  arginfo_protocolbuffers_message_get, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, mutable,              arginfo_protocolbuffers_message_mutable, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, set,                  arginfo_protocolbuffers_message_set, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, setFrom,              arginfo_protocolbuffers_message_set_from, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, append,               arginfo_protocolbuffers_message_append, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, hasExtension,         arginfo_protocolbuffers_message_has_extension, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, getExtension,         arginfo_protocolbuffers_message_get_extension, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, setExtension,         arginfo_protocolbuffers_message_set_extension, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, clearExtension,       arginfo_protocolbuffers_message_clear_extension, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, discardUnknownFields, arginfo_protocolbuffers_message_discard_unknown_fields, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, getUnknownFieldSet,   NULL, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, containerOf,          NULL, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, jsonSerialize,        NULL, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, toArray,        NULL, ZEND_ACC_PUBLIC)
		/* iterator */
	PHP_ME(protocolbuffers_message, current,   NULL, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, key,       NULL, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, next,      NULL, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, rewind,    NULL, ZEND_ACC_PUBLIC)
	PHP_ME(protocolbuffers_message, valid,     NULL, ZEND_ACC_PUBLIC)
	/* magic method */
	PHP_ME(protocolbuffers_message, __call,   arginfo_protocolbuffers_message___call, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

void php_protocolbuffers_message_class(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "ProtocolBuffersMessage", php_protocolbuffers_message_methods);
	php_protocol_buffers_message_class_entry = zend_register_internal_class(&ce TSRMLS_CC);
	zend_class_implements(php_protocol_buffers_message_class_entry TSRMLS_CC, 1, zend_ce_iterator);
	zend_class_implements(php_protocol_buffers_message_class_entry TSRMLS_CC, 1, php_protocol_buffers_serializable_class_entry);

// Note(chobie): Don't implement JsonSerializable here as some environment uses json shared module.
//#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION == 4) && (PHP_RELEASE_VERSION >= 26)) || ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION == 5) && (PHP_RELEASE_VERSION >= 10))
//	zend_class_implements(php_protocol_buffers_message_class_entry TSRMLS_CC, 1, php_json_serializable_ce);
//#endif

	php_protocol_buffers_message_class_entry->ce_flags |= ZEND_ACC_EXPLICIT_ABSTRACT_CLASS;
	php_protocol_buffers_message_class_entry->create_object = php_protocolbuffers_message_new;

	memcpy(&php_protocolbuffers_message_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	PHP_PROTOCOLBUFFERS_REGISTER_NS_CLASS_ALIAS(PHP_PROTOCOLBUFFERS_NAMESPACE, "Message", php_protocol_buffers_message_class_entry);
}
