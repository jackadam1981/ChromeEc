/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _PANIC_CBOR_H_
#define _PANIC_CBOR_H_

#include "panic_defs.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
	uint8_t version; /* Semantic version */
	uint16_t capacity; /* Total capacity */
	uint16_t length; /* Encoded length */
	uint8_t status; /* PANIC_CBOR_STATUS_ flags */
	uint8_t errors; /* PANIC_CBOR_ERROR_ flags */
	uint8_t data[CONFIG_PANIC_CBOR_CAPACITY];
	uint32_t checksum; /* Data checksum */
} __packed panic_cbor_t;

/* Up to 8 status flags */
#define PANIC_CBOR_STATUS_OPENED BIT(0)
#define PANIC_CBOR_STATUS_APPENDED BIT(1)
#define PANIC_CBOR_STATUS_CLOSED BIT(2)

/* Up to 8 error flags */
#define PANIC_CBOR_ERROR_INVALID_INPUT BIT(0)
#define PANIC_CBOR_ERROR_ENCODE_ERROR BIT(1)
#define PANIC_CBOR_ERROR_OVERFLOW BIT(2)

enum panic_cbor_label {
	PANIC_CBOR_LABEL_ACTIVE_IMAGE = 0,
	PANIC_CBOR_LABEL_LID_OPEN,
	PANIC_CBOR_LABEL_CURRENT_TASK,
	PANIC_CBOR_LABEL_UPTIME_US,
	PANIC_CBOR_LABEL_RESET_FLAGS,
	PANIC_CBOR_LABEL_EXECUTION_CONTEXT,
	PANIC_CBOR_LABEL_RW_VERSION,
	PANIC_CBOR_LABEL_RO_VERSION,
	PANIC_CBOR_LABEL_TASK_INFO,
	PANIC_CBOR_LABEL_STACK_INFO,
	PANIC_CBOR_LABEL_IRQ_DIST,
	PANIC_CBOR_LABEL_REGISTERS,
	PANIC_CBOR_LABEL_GPIOS,
	PANIC_CBOR_LABEL_SW_PANIC_REASON,
	PANIC_CBOR_LABEL_SW_PANIC_INFO,
	PANIC_CBOR_LABEL_POWER_STATE,
	PANIC_CBOR_LABEL_POST_WATCHDOG_RESET,
	PANIC_CBOR_LABEL_POWER_STATE_NAME,
	PANIC_CBOR_LABEL_POWER_SIGNALS,
	PANIC_CBOR_LABEL_CHARGE_STATE,
	PANIC_CBOR_LABEL_COUNT
};

int panic_cbor_open(void);
int panic_cbor_open_append(void);
int panic_cbor_close(void);
int panic_cbor_dump(void);
int panic_cbor_fill_common(void);

/* Primitive Value */
int panic_cbor_int32(int32_t value);
int panic_cbor_int64(int64_t value);
int panic_cbor_uint32(uint32_t value);
int panic_cbor_uint64(uint64_t value);
int panic_cbor_bool(bool value);
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

/* Transactions */
int panic_cbor_start_transaction(void);
int panic_cbor_commit_transaction(void);
int panic_cbor_cancel_transaction(void);

__attribute__((error("unhandled panic cbor type"))) extern void
unhandled_panic_cbor_type(void);

#define _PANIC_CBOR_LABEL_FUNC(label)         \
	_Generic((label),                     \
		int: panic_cbor_uint32,       \
		char *: panic_cbor_str,       \
		const char *: panic_cbor_str, \
		default: unhandled_panic_cbor_type)

#define _PANIC_CBOR_VALUE_FUNC(value)         \
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

#define _PANIC_CBOR_ARRAY_FUNC(array)                \
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

#define PANIC_CBOR_LABEL(label) _PANIC_CBOR_LABEL_FUNC(label)(label)

#define PANIC_CBOR_VALUE(value) _PANIC_CBOR_VALUE_FUNC(value)(value)

#define PANIC_CBOR_ARRAY(array, len) _PANIC_CBOR_ARRAY_FUNC(array)(array, len)

#define PANIC_CBOR_MAP_END() panic_cbor_map_end()

#define PANIC_CBOR_LABEL_VALUE_TRANSACTION(label, value)                       \
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

#define PANIC_CBOR_LABEL_VALUE(label, value)           \
	({                                             \
		int _rv = PANIC_CBOR_LABEL(label);     \
		if (_rv == EC_SUCCESS) {               \
			_rv = PANIC_CBOR_VALUE(value); \
		}                                      \
		_rv;                                   \
	})

#define PANIC_CBOR_LABEL_ARRAY_TRANSACTION(label, array, len)                  \
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

#define PANIC_CBOR_LABEL_ARRAY(label, array, len)           \
	({                                                  \
		int _rv = PANIC_CBOR_LABEL(label);          \
		if (_rv == EC_SUCCESS) {                    \
			_rv = PANIC_CBOR_ARRAY(array, len); \
		}                                           \
		_rv;                                        \
	})

#define PANIC_CBOR_LABEL_MAP_START_TRANSACTION(label)                          \
	({                                                                     \
		int _rv = panic_cbor_start_transaction();                      \
		if (_rv == EC_SUCCESS) {                                       \
			_rv = PANIC_CBOR_LABEL(label);                         \
			if (_rv == EC_SUCCESS) {                               \
				_rv = panic_cbor_map_start();                  \
				if (_rv == EC_SUCCESS)                         \
					_rv = panic_cbor_commit_transaction(); \
				else                                           \
					_rv = panic_cbor_cancel_transaction(); \
			} else                                                 \
				_rv = panic_cbor_cancel_transaction();         \
		}                                                              \
		_rv;                                                           \
	})

#define PANIC_CBOR_LABEL_MAP_START(label)             \
	({                                            \
		int _rv = PANIC_CBOR_LABEL(label);    \
		if (_rv == EC_SUCCESS) {              \
			_rv = panic_cbor_map_start(); \
		}                                     \
		_rv;                                  \
	})

#endif /* _PANIC_CBOR_H_ */
