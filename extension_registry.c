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
#include "extension_registry.h"
#include "field_descriptor.h"
#include "descriptor_builder.h"

zval *php_protocolbuffers_extension_registry_get_instance(TSRMLS_D)
{
	if (PBG(extension_registry) == NULL) {
		zval *extension_registry = NULL;

#if ZEND_MODULE_API_NO >= 20151012
		array_init(extension_registry);
#else
		MAKE_STD_ZVAL(extension_registry);
#endif
		object_init_ex(extension_registry, php_protocol_buffers_extension_registry_class_entry);
		PBG(extension_registry) = extension_registry;
	}

	return PBG(extension_registry);
}

int php_protocolbuffers_extension_registry_get_registry(zval *instance, const char* message_class, size_t message_class_len, zval **result TSRMLS_DC)
{
	zval **bucket = NULL;
	php_protocolbuffers_extension_registry *registry;
#if ZEND_MODULE_API_NO >= 20151012
	registry = (php_protocolbuffers_extension_registry *) ((char*)Z_OBJ_P(instance) - XtOffsetOf(php_protocolbuffers_extension_registry, zo)); 

	if ((*bucket = zend_hash_str_find(registry->registry, message_class, message_class_len)) != NULL) {
#else
	registry = PHP_PROTOCOLBUFFERS_GET_OBJECT(php_protocolbuffers_extension_registry, instance);

	if (zend_hash_find(registry->registry, message_class, message_class_len, (void **)&bucket) == SUCCESS) {
#endif
		*result = *bucket;
		return 1;
	}

	return 0;
}

int php_protocolbuffers_extension_registry_get_descriptor_by_name(zval *hash, const char* name, size_t name_len, zval **result TSRMLS_DC)
{
	zval **bucket = NULL, **bucket2 = NULL;

#if ZEND_MODULE_API_NO >= 20151012
	if ((*bucket = zend_hash_str_find(Z_ARRVAL_P(hash), ZEND_STRS("map"))) != NULL) {
		if ((*bucket2 = zend_hash_str_find(Z_ARRVAL_PP(bucket), name, name_len+1)) != NULL) {
			*result = *bucket2;
			return 1;
		}
	}

#else
	if (zend_hash_find(Z_ARRVAL_P(hash), ZEND_STRS("map"), (void **)&bucket) == SUCCESS) {
		if (zend_hash_find(Z_ARRVAL_PP(bucket), name, name_len+1, (void **)&bucket2) == SUCCESS) {
			*result = *bucket2;
			return 1;
		}
	}

#endif
	return 0;
}

static void php_protocolbuffers_extension_registry_free_storage(php_protocolbuffers_extension_registry *object TSRMLS_DC)
{
	if (object->registry != NULL) {
		HashPosition pos = 0;
		int n = 0;
		zval **element = {0};

#if ZEND_MODULE_API_NO >= 20151012
		for(n = 0, zend_hash_internal_pointer_reset_ex(object->registry, &pos);
			(*element = zend_hash_get_current_data_ex(object->registry, &pos)) != NULL;
			zend_hash_move_forward_ex(object->registry, &pos), n++
		) {
            zval_ptr_dtor(*element);
		}
#else
		for(n = 0, zend_hash_internal_pointer_reset_ex(object->registry, &pos);
			zend_hash_get_current_data_ex(object->registry, (void **)&element, &pos) == SUCCESS;
			zend_hash_move_forward_ex(object->registry, &pos), n++
		) {
            zval_ptr_dtor(element);
		}
#endif

		zend_hash_destroy(object->registry);
		efree(object->registry);
		object->registry = NULL;
	}

	zend_object_std_dtor(&object->zo TSRMLS_CC);
	efree(object);
}

static int sort_cb(const void *a, const void *b)
{
    return ((php_protocolbuffers_scheme*)a)->tag - ((php_protocolbuffers_scheme*)b)->tag;
}

#if ZEND_MODULE_API_NO >= 20151012
//static zend_object_handlers registry_object_handlers;
zend_object *php_protocolbuffers_extension_registry_new(zend_class_entry *ce TSRMLS_DC)
{
    php_protocolbuffers_extension_registry *registry_object;

    registry_object = ecalloc(1, sizeof(php_protocolbuffers_extension_registry) + zend_object_properties_size(ce));
    registry_object->zo.ce = ce;

    zend_object_std_init(&registry_object->zo, ce);
    object_properties_init(&registry_object->zo, ce);
    registry_object->zo.handlers = zend_get_std_object_handlers();
    return &registry_object->zo;

}
#else
zend_object_value php_protocolbuffers_extension_registry_new(zend_class_entry *ce TSRMLS_DC)
{
	zend_object retval;
	PHP_PROTOCOLBUFFERS_STD_CREATE_OBJECT(php_protocolbuffers_extension_registry);

	ALLOC_HASHTABLE(object->registry);
	zend_hash_init(object->registry, 0, NULL, NULL, 0);

	return retval;
}
#endif

ZEND_BEGIN_ARG_INFO_EX(arginfo_protocolbuffers_extension_registry_add, 0, 0, 3)
	ZEND_ARG_INFO(0, message_class_name)
	ZEND_ARG_INFO(0, extension)
	ZEND_ARG_INFO(0, descriptor)
ZEND_END_ARG_INFO()


/* {{{ proto ProtocolBuffersExtensionRegistry ProtocolBuffersExtensionRegistry::__construct()
*/
PHP_METHOD(protocolbuffers_extension_registry, __construct)
{
	/* Currently, nothing to do. */
}
/* }}} */

/* {{{ proto ProtocolBuffersExtensionRegistry ProtocolBuffersExtensionRegistry::getInstance()
*/
PHP_METHOD(protocolbuffers_extension_registry, getInstance)
{
	zval *extension_registry;

	extension_registry = php_protocolbuffers_extension_registry_get_instance(TSRMLS_C);
	RETURN_ZVAL(extension_registry, 1, 0);
}
/* }}} */

/* {{{ proto void ProtocolBuffersExtensionRegistry::add(string $message_class_name, long $extension, ProtocolBuffersFieldDescriptor $descriptor)
*/
PHP_METHOD(protocolbuffers_extension_registry, add)
{
	zval *instance = getThis();
	char *message_class_name = {0};
	long message_class_name_len = 0;
	zval *descriptor, **bucket;
	zend_class_entry **ce;
	php_protocolbuffers_scheme_container *container = NULL;
	long extension = 0;
	php_protocolbuffers_extension_registry *registry;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"slO", &message_class_name, &message_class_name_len, &extension, &descriptor, php_protocol_buffers_field_descriptor_class_entry) == FAILURE) {
		return;
	}

	if (message_class_name[0] == '\\') {
		int x;
		for (x = 0; x < message_class_name_len-1; x++) {
			message_class_name[x] = message_class_name[x+1];
		}
		message_class_name[message_class_name_len-1] = '\0';
		message_class_name_len--;
	}


#if ZEND_MODULE_API_NO >= 20151012
	zend_string *zstr_mcn = zend_string_init(message_class_name, message_class_name_len, 0);
	if ((*ce = zend_lookup_class(zstr_mcn)) == NULL) {
		// TODO(chobie): check the class which has getDescriptor method.
		zend_string_release(zstr_mcn);
		zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "%s class does not find", message_class_name);
		return;
	}
	registry = (php_protocolbuffers_extension_registry *) ((char*)Z_OBJ_P(instance) - XtOffsetOf(php_protocolbuffers_extension_registry, zo)); 
	if (!zend_hash_exists(registry->registry, zstr_mcn)) {
		zval *p = NULL, *p2 = NULL, *p3 = NULL, *p4 = NULL;
		array_init(p);

		array_init(p2);

		array_init(p3);

		array_init(p4);

		zend_string *map = zend_string_init(ZEND_STRS("map"), 0);
		zend_string *index = zend_string_init(ZEND_STRS("index"), 0);
		zend_string *emap = zend_string_init(ZEND_STRS("emap"), 0);
		zend_hash_update(Z_ARRVAL_P(p), map, p2);
		zend_hash_update(Z_ARRVAL_P(p), index, p3);
		zend_hash_update(Z_ARRVAL_P(p), emap, p4);
		zend_string_release(map);
		zend_string_release(index);
		zend_string_release(emap);

		Z_ADDREF_P(p);
		zend_hash_update(registry->registry, zstr_mcn, p);
		zval_ptr_dtor(p);
		p = NULL;
	}

	if ((*bucket = zend_hash_find(registry->registry, zstr_mcn)) != NULL) {
		zval **bucket2 = NULL;
		char *name = NULL;
		int len = 0;

		if ((*bucket2 = zend_hash_str_find(Z_ARRVAL_PP(bucket), ZEND_STRS("index"))) != NULL) {
			if (zend_hash_index_exists(Z_ARRVAL_PP(bucket), extension)) {
				zend_string_release(zstr_mcn);
				zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "can't override specified extension number: already exists");
				return;
			}

			Z_ADDREF_P(descriptor);
			zend_hash_index_update(Z_ARRVAL_PP(bucket2), extension, descriptor);
		}

		if ((*bucket2 = zend_hash_str_find(Z_ARRVAL_PP(bucket), ZEND_STRS("map"))) != NULL) {
			php_protocolbuffers_field_descriptor_get_name(descriptor, &name, &len TSRMLS_CC);

			if (zend_hash_str_exists(Z_ARRVAL_PP(bucket), name, len)) {
				zend_string_release(zstr_mcn);
				zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "can't override specified extension number: already exists");
				return;
			}

			Z_ADDREF_P(descriptor);
			zend_string *zstr_name = zend_string_init(name, len, 0);
			zend_hash_update(Z_ARRVAL_PP(bucket2), zstr_name, descriptor);

			if ((*bucket2 = zend_hash_str_find(Z_ARRVAL_PP(bucket), ZEND_STRS("emap"))) != NULL) {
				zval *nm = NULL;
				ZVAL_LONG(nm, extension);

				zend_hash_update(Z_ARRVAL_PP(bucket2), zstr_name, nm);
			}
			zend_string_release(zstr_name);
		}

		zend_declare_property_null(*ce, name, len-1, ZEND_ACC_PROTECTED TSRMLS_CC);
		php_protocolbuffers_get_scheme_container(ZSTR_VAL((*ce)->name), ZSTR_LEN((*ce)->name), &container TSRMLS_CC);
		container->scheme = (php_protocolbuffers_scheme*)erealloc(container->scheme, sizeof(php_protocolbuffers_scheme) * (container->size + 1));
		if (php_protocolbuffers_init_scheme_with_zval(&container->scheme[container->size], extension, descriptor TSRMLS_CC)) {
			container->scheme[container->size].is_extension = 1;
			container->size++;
			qsort(container->scheme, container->size, sizeof(php_protocolbuffers_scheme), sort_cb);
		}
	}
	zend_string_release(zstr_mcn);
#else
	if (zend_lookup_class((char*)message_class_name, message_class_name_len, &ce TSRMLS_CC) == FAILURE) {
		// TODO(chobie): check the class which has getDescriptor method.
		zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "%s class does not find", message_class_name);
		return;
	}

	registry = PHP_PROTOCOLBUFFERS_GET_OBJECT(php_protocolbuffers_extension_registry, instance);
	if (!zend_hash_exists(registry->registry, message_class_name, message_class_name_len)) {
		zval *p = NULL, *p2 = NULL, *p3 = NULL, *p4 = NULL;

		MAKE_STD_ZVAL(p);
		array_init(p);

		MAKE_STD_ZVAL(p2);
		array_init(p2);

		MAKE_STD_ZVAL(p3);
		array_init(p3);

		MAKE_STD_ZVAL(p4);
		array_init(p4);

		zend_hash_update(Z_ARRVAL_P(p), "map", sizeof("map"), (void **)&p2, sizeof(zval*), NULL);
		zend_hash_update(Z_ARRVAL_P(p), "index", sizeof("index"), (void **)&p3, sizeof(zval*), NULL);
		zend_hash_update(Z_ARRVAL_P(p), "emap", sizeof("emap"), (void **)&p4, sizeof(zval*), NULL);

		Z_ADDREF_P(p);
		zend_hash_update(registry->registry, message_class_name, message_class_name_len, (void **)&p, sizeof(zval*), NULL);
		zval_ptr_dtor(&p);
		p = NULL;
	}

	if (zend_hash_find(registry->registry, message_class_name, message_class_name_len, (void **)&bucket) == SUCCESS) {
		zval **bucket2 = NULL;
		char *name = NULL;
		int len = 0;

		if (zend_hash_find(Z_ARRVAL_PP(bucket), ZEND_STRS("index"), (void **)&bucket2) == SUCCESS) {
			if (zend_hash_index_exists(Z_ARRVAL_PP(bucket), extension)) {
				zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "can't override specified extension number: already exists");
				return;
			}

			Z_ADDREF_P(descriptor);
			zend_hash_index_update(Z_ARRVAL_PP(bucket2), extension, (void **)&descriptor, sizeof(zval*), NULL);
		}

		if (zend_hash_find(Z_ARRVAL_PP(bucket), ZEND_STRS("map"), (void **)&bucket2) == SUCCESS) {
			php_protocolbuffers_field_descriptor_get_name(descriptor, &name, &len TSRMLS_CC);

			if (zend_hash_exists(Z_ARRVAL_PP(bucket), name, len)) {
				zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC, "can't override specified extension number: already exists");
				return;
			}

			Z_ADDREF_P(descriptor);
			zend_hash_update(Z_ARRVAL_PP(bucket2), name, len, (void **)&descriptor, sizeof(zval*), NULL);

			if (zend_hash_find(Z_ARRVAL_PP(bucket), ZEND_STRS("emap"), (void **)&bucket2) == SUCCESS) {
				zval *nm = NULL;

				MAKE_STD_ZVAL(nm);
				ZVAL_LONG(nm, extension);

				zend_hash_update(Z_ARRVAL_PP(bucket2), name, len, (void **)&nm, sizeof(zval*), NULL);
			}
		}

		zend_declare_property_null(*ce, name, len-1, ZEND_ACC_PROTECTED TSRMLS_CC);
		php_protocolbuffers_get_scheme_container((*ce)->name, (*ce)->name_length, &container TSRMLS_CC);
		container->scheme = (php_protocolbuffers_scheme*)erealloc(container->scheme, sizeof(php_protocolbuffers_scheme) * (container->size + 1));
		if (php_protocolbuffers_init_scheme_with_zval(&container->scheme[container->size], extension, descriptor TSRMLS_CC)) {
			container->scheme[container->size].is_extension = 1;
			container->size++;
			qsort(container->scheme, container->size, sizeof(php_protocolbuffers_scheme), sort_cb);
		}
	}
#endif
}
/* }}} */

static zend_function_entry php_protocolbuffers_extension_registry_methods[] = {
	PHP_ME(protocolbuffers_extension_registry, __construct,  NULL, ZEND_ACC_PRIVATE | ZEND_ACC_CTOR)
	PHP_ME(protocolbuffers_extension_registry, getInstance,  NULL, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(protocolbuffers_extension_registry, add,  arginfo_protocolbuffers_extension_registry_add, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

void php_protocolbuffers_extension_registry_class(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "ProtocolBuffersExtensionRegistry", php_protocolbuffers_extension_registry_methods);
	php_protocol_buffers_extension_registry_class_entry = zend_register_internal_class(&ce TSRMLS_CC);
	php_protocol_buffers_extension_registry_class_entry->create_object = php_protocolbuffers_extension_registry_new;
#if ZEND_MODULE_API_NO >= 20151012
	// http://php.net/ChangeLog-7.php
	// Removed ZEND_ACC_FINAL_CLASS, promoting ZEND_ACC_FINAL as final class modifier.
	php_protocol_buffers_extension_registry_class_entry->ce_flags |= ZEND_ACC_FINAL;
#else
	php_protocol_buffers_extension_registry_class_entry->ce_flags |= ZEND_ACC_FINAL_CLASS;
#endif

	PHP_PROTOCOLBUFFERS_REGISTER_NS_CLASS_ALIAS(PHP_PROTOCOLBUFFERS_NAMESPACE, "ExtensionRegistry", php_protocol_buffers_extension_registry_class_entry);
}
