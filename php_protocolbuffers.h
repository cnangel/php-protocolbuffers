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

#ifndef PHP_PROTOCOLBUFFERS_H
#define PHP_PROTOCOLBUFFERS_H

#define PHP_PROTOCOLBUFFERS_EXTNAME "protocolbuffers"
#define PHP_PROTOCOLBUFFERS_VERSION "0.3.0"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "zend.h"
#include "zend_interfaces.h"
#include "zend_exceptions.h"
#if ZEND_MODULE_API_NO >= 20151012
#include "Zend/zend_smart_str.h"
#else
#include "ext/standard/php_smart_str.h"
#endif
#include "ext/spl/spl_exceptions.h"
#include "ext/standard/php_var.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_incomplete_class.h"
#include "ext/standard/info.h"
#include "ext/standard/php_array.h"
#include "limits.h"

#include <stdlib.h>

/* Define the entry point symbol
 * Zend will use when loading this module
 */
extern zend_module_entry protocolbuffers_module_entry;
#define phpext_protocolbuffers_ptr &protocolbuffers_module_entry

extern zend_class_entry *protocol_buffers_class_entry;
extern zend_class_entry *php_protocol_buffers_descriptor_class_entry;
extern zend_class_entry *php_protocol_buffers_field_descriptor_class_entry;

extern zend_class_entry *php_protocol_buffers_serializable_class_entry;
extern zend_class_entry *php_protocol_buffers_enum_describable_class_entry;

extern zend_class_entry *php_protocol_buffers_message_class_entry;
extern zend_class_entry *php_protocol_buffers_message_options_class_entry;
extern zend_class_entry *php_protocol_buffers_enum_class_entry;
extern zend_class_entry *php_protocol_buffers_enum_descriptor_class_entry;
extern zend_class_entry *php_protocol_buffers_enum_value_descriptor_class_entry;
extern zend_class_entry *php_protocol_buffers_enum_descriptor_builder_class_entry;
extern zend_class_entry *php_protocol_buffers_descriptor_builder_class_entry;
extern zend_class_entry *php_protocol_buffers_unknown_field_class_entry;
extern zend_class_entry *php_protocol_buffers_unknown_field_set_class_entry;
extern zend_class_entry *php_protocol_buffers_helper_class_entry;
extern zend_class_entry *php_protocol_buffers_extension_registry_class_entry;

extern zend_class_entry *php_protocol_buffers_php_message_options_class_entry;

extern zend_class_entry *php_protocol_buffers_invalid_byte_sequence_class_entry;
extern zend_class_entry *php_protocol_buffers_invalid_protocolbuffers_exception_class_entry;
extern zend_class_entry *php_protocol_buffers_uninitialized_message_exception_class_entry;

ZEND_BEGIN_MODULE_GLOBALS(protocolbuffers)
	HashTable *messages;
	HashTable *classes;
	zval *extension_registry;
	long strict_mode;
	long validate_string;
	long native_scalars;
ZEND_END_MODULE_GLOBALS(protocolbuffers)

ZEND_EXTERN_MODULE_GLOBALS(protocolbuffers)

#ifdef ZTS
#define PBG(v) TSRMG(protocolbuffers_globals_id, zend_protocolbuffers_globals *, v)
#else
#define PBG(v) (protocolbuffers_globals.v)
#endif

#if ZEND_MODULE_API_NO >= 20151012
#ifndef Z_TYPE_PP
#define Z_TYPE_PP(v)            Z_TYPE(**v)
#endif

#ifndef Z_ARRVAL_PP
#define Z_ARRVAL_PP(zval_pp)    Z_ARRVAL(**zval_pp)
#endif

#ifndef Z_LVAL_PP
#define Z_LVAL_PP(zval_pp)      Z_LVAL(**zval_pp)
#endif

#ifndef Z_BVAL
#define Z_BVAL(zval)            ((zend_bool)(zval).value.lval)
#endif

#ifndef Z_BVAL_P
#define Z_BVAL_P(zval_p)		Z_BVAL(*zval_p)
#endif

#ifndef Z_BVAL_PP
#define Z_BVAL_PP(zval_pp)      Z_BVAL(**zval_pp)
#endif

#ifndef Z_DVAL_PP
#define Z_DVAL_PP(zval_pp)      Z_DVAL(**zval_pp)
#endif

#ifndef Z_STRVAL_PP
#define Z_STRVAL_PP(zval_pp)    Z_STRVAL(**zval_pp)
#endif

#ifndef Z_STRLEN_PP
#define Z_STRLEN_PP(zval_pp)    Z_STRLEN(**zval_pp)
#endif

#ifndef Z_STR_PP
#define Z_STR_PP(zval_pp)		Z_STR(**zval_pp)
#endif

#ifndef Z_OBJPROP_PP 
#define Z_OBJPROP_PP(zval_pp)   Z_OBJPROP(**zval_pp)
#endif

#ifndef PZVAL_IS_REF
#define PZVAL_IS_REF(z)			Z_ISREF_P(z)
#endif

#ifndef Z_REFCOUNT_PP 
#define Z_REFCOUNT_PP(ppz)		Z_REFCOUNT_P(*(ppz))
#endif

#ifndef Z_OBJCE_PP
#define Z_OBJCE_PP(zval_pp)		Z_OBJCE(**zval_pp)
#endif

#ifndef Z_ADDREF_PP
#define Z_ADDREF_PP(ppz)		Z_ADDREF_P(*(ppz))
#endif

#endif

#endif /* PHP_PROTOCOLBUFFERS_H */
