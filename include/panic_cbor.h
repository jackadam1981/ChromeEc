/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _PANIC_CBOR_H_
#define _PANIC_CBOR_H_

#include "panic_defs.h"

#include <stdbool.h>
#include <stddef.h>

/* Upto 16 bits */
#define PANIC_CBOR_FLAG_OPENED BIT(0)
#define PANIC_CBOR_FLAG_CLOSED BIT(1)
#define PANIC_CBOR_FLAG_UNEXPECTED_INPUT BIT(2)
#define PANIC_CBOR_FLAG_INVALID_INPUT BIT(3)
#define PANIC_CBOR_FLAG_ENCODE_ERROR BIT(4)
#define PANIC_CBOR_FLAG_OVERFLOW BIT(5)

#define PANIC_CBOR_LABELS    \
	X(ACTIVE_IMAGE)      \
	X(LID_OPEN)          \
	X(CURRENT_TASK)      \
	X(UPTIME)            \
	X(RESET_FLAGS)       \
	X(EXECUTION_CONTEXT) \
	X(RW_VERSION)        \
	X(RO_VERSION)        \
	X(TASK_INFO)         \
	X(POWER_STATE)       \
	X(STACK)             \
	X(IRQ_DIST)          \
	X(ID)                \
	X(REGISTERS)         \
	X(GPIO)              \
	X(PANIC_TYPE)

enum panic_cbor_label {
#define X(label) PANIC_CBOR_LABEL_##label,
	PANIC_CBOR_LABELS
#undef X
		PANIC_CBOR_LABEL_COUNT,
};

#ifdef CONFIG_PANIC_CBOR_DEBUG

__maybe_unused static inline const char *
panic_cbor_label_to_string(enum panic_cbor_label label)
{
	const char *_panic_cbor_label_to_string[] = {
#define X(tag) #tag,
		PANIC_CBOR_LABELS
#undef X
	};
	return _panic_cbor_label_to_string[label];
}

#endif

int panic_cbor_open(struct panic_data *pdata);
int panic_cbor_close(struct panic_data *pdata);
int panic_cbor_dump(struct panic_data *pdata);
int panic_cbor_fill_common(void);

/* Primitive Value */
int panic_cbor_int32(int32_t value);
int panic_cbor_int64(int64_t value);
int panic_cbor_uint32(uint32_t value);
int panic_cbor_uint64(uint64_t value);
int panic_cbor_bool(bool value);
int panic_cbor_null(void *value);
int panic_cbor_str(const char *value);

/* Array Value */
int panic_cbor_int8_array(const int8_t *value, size_t len);
int panic_cbor_uint8_array(const uint8_t *value, size_t len);
int panic_cbor_int16_array(const int16_t *value, size_t len);
int panic_cbor_uint16_array(const uint16_t *value, size_t len);
int panic_cbor_int32_array(const int32_t *value, size_t len);
int panic_cbor_uint32_array(const uint32_t *value, size_t len);
int panic_cbor_int64_array(const int64_t *value, size_t len);
int panic_cbor_uint64_array(const uint64_t *value, size_t len);

/* Collection */
int panic_cbor_map_start(void);
int panic_cbor_map_end(void);
int panic_cbor_list_start(void);
int panic_cbor_list_end(void);

/* Transaxtion */
int panic_cbor_start_transaction(void);
int panic_cbor_commit_transaction(void);
int panic_cbor_cancel_transaction(void);

__attribute__((error("unhandled panic cbor type"))) extern void
unhandled_panic_cbor_type(void);

#define PANIC_CBOR_LABEL_FUNC(label)          \
	_Generic((label),                     \
		int: panic_cbor_uint32,       \
		char *: panic_cbor_str,       \
		const char *: panic_cbor_str, \
		default: unhandled_panic_cbor_type)

#define PANIC_CBOR_VALUE_FUNC(value)          \
	_Generic((value),                     \
		int8_t: panic_cbor_int32,     \
		uint8_t: panic_cbor_uint32,   \
		int16_t: panic_cbor_int32,    \
		uint16_t: panic_cbor_uint32,  \
		int32_t: panic_cbor_int32,    \
		uint32_t: panic_cbor_uint32,  \
		int64_t: panic_cbor_int64,    \
		uint64_t: panic_cbor_uint64,  \
		bool: panic_cbor_bool,        \
		char *: panic_cbor_str,       \
		const char *: panic_cbor_str, \
		default: unhandled_panic_cbor_type)

#define PANIC_CBOR_ARRAY_FUNC(array)                 \
	_Generic((array),                            \
		int8_t *: panic_cbor_int8_array,     \
		uint8_t *: panic_cbor_uint8_array,   \
		int16_t *: panic_cbor_int16_array,   \
		uint16_t *: panic_cbor_uint16_array, \
		int32_t *: panic_cbor_int32_array,   \
		uint32_t *: panic_cbor_uint32_array, \
		int64_t *: panic_cbor_int64_array,   \
		uint64_t *: panic_cbor_uint64_array, \
		default: unhandled_panic_cbor_type)

#define PANIC_CBOR_LABEL(label) PANIC_CBOR_LABEL_FUNC(label)(label)

#define PANIC_CBOR_VALUE(value) PANIC_CBOR_VALUE_FUNC(value)(value)

#define PANIC_CBOR_ARRAY(array, len) PANIC_CBOR_ARRAY_FUNC(array)(array, len)

#define PANIC_CBOR_LABEL_VALUE(label, value)                                   \
	({                                                                     \
		int _rv = panic_cbor_start_transaction();                      \
		if (_rv == EC_SUCCESS) {                                       \
			_rv = PANIC_CBOR_LABEL(label);                         \
			if (_rv == EC_SUCCESS) {                               \
				_rv = PANIC_CBOR_VALUE(value);                 \
				if (_rv == EC_SUCCESS)                         \
					_rv = panic_cbor_commit_transaction(); \
				else                                           \
					_rv = panic_cbor_cancel_transaction(); \
			} else                                                 \
				_rv = panic_cbor_cancel_transaction();         \
		}                                                              \
		_rv;                                                           \
	})

#define PANIC_CBOR_LABEL_ARRAY(label, array, len)                              \
	({                                                                     \
		int _rv = panic_cbor_start_transaction();                      \
		if (_rv == EC_SUCCESS) {                                       \
			_rv = PANIC_CBOR_LABEL(label);                         \
			if (_rv == EC_SUCCESS) {                               \
				_rv = PANIC_CBOR_ARRAY(array, len);            \
				if (_rv == EC_SUCCESS)                         \
					_rv = panic_cbor_commit_transaction(); \
				else                                           \
					_rv = panic_cbor_cancel_transaction(); \
			} else                                                 \
				_rv = panic_cbor_cancel_transaction();         \
		}                                                              \
		_rv;                                                           \
	})

#endif /* _PANIC_CBOR_H_ */
