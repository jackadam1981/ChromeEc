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
	X(REGISTERS)

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

int panic_cbor_str_label(const char *label);
int _panic_cbor_str_label(const void *label);
int panic_cbor_enum_label(enum panic_cbor_label label);
int _panic_cbor_enum_label(const void *label);

int panic_cbor_int32(int32_t value);
int _panic_cbor_int32(const void *value);
int panic_cbor_int64(int64_t value);
int panic_cbor_uint32(uint32_t value);
int _panic_cbor_uint32(const void *value);
int panic_cbor_uint64(uint64_t value);
int panic_cbor_bool(bool value);
int _panic_cbor_bool(const void *value);
int panic_cbor_str(const char *value);
int _panic_cbor_str(const void *value);
int panic_cbor_int32_array(const int32_t *value, size_t len);
int panic_cbor_int64_array(const int64_t *value, size_t len);
int panic_cbor_uint32_array(const uint32_t *value, size_t len);
int panic_cbor_uint64_array(const uint64_t *value, size_t len);

int panic_cbor_str_label_int32(const char *label, int32_t value);
int panic_cbor_str_label_int64(const char *label, int64_t value);
int panic_cbor_str_label_uint32(const char *label, uint32_t value);
int panic_cbor_str_label_uint64(const char *label, uint64_t value);

int panic_cbor_enum_label_int32(enum panic_cbor_label label, int32_t value);
int panic_cbor_enum_label_int64(enum panic_cbor_label label, int64_t value);
int panic_cbor_enum_label_uint32(enum panic_cbor_label label, uint32_t value);
int panic_cbor_enum_label_uint64(enum panic_cbor_label label, uint64_t value);

int panic_cbor_map_start(void);
int panic_cbor_map_end(void);

int panic_cbor_list_start(void);
int panic_cbor_list_end(void);

int panic_cbor_transaction(int (*funcs[])(const void *), const void *args[],
			   size_t len);

__attribute__((error("unhandled panic cbor type"))) extern void
unhandled_panic_cbor_type(void);

#define PANIC_CBOR_LABEL_FUNC(label)                 \
	_Generic((label),                            \
		int: _panic_cbor_enum_label,         \
		char *: _panic_cbor_str_label,       \
		const char *: _panic_cbor_str_label, \
		default: unhandled_panic_cbor_type)

#define PANIC_CBOR_VALUE_FUNC(value)           \
	_Generic((value),                      \
		int32_t: _panic_cbor_int32,    \
		uint32_t: _panic_cbor_uint32,  \
		uint8_t: _panic_cbor_uint32,   \
		bool: _panic_cbor_bool,        \
		char *: _panic_cbor_str,       \
		const char *: _panic_cbor_str, \
		default: unhandled_panic_cbor_type)

#define PANIC_CBOR_VOID_STAR_CAST(x)                        \
	_Generic((x),                                       \
		int32_t: (const void *)(uintptr_t)(x),      \
		uint32_t: (const void *)(uintptr_t)(x),     \
		uint8_t: (const void *)(uintptr_t)(x),      \
		bool: (const void *)(uintptr_t)(x),         \
		char *: (const void *)(uintptr_t)(x),       \
		const char *: (const void *)(uintptr_t)(x), \
		default: (x))

#define PANIC_CBOR_LABEL_VALUE(label, value)                                \
	panic_cbor_transaction(                                             \
		(int (*[2])(const void *)){ PANIC_CBOR_LABEL_FUNC(label),   \
					    PANIC_CBOR_VALUE_FUNC(value) }, \
		(const void *[2]){ PANIC_CBOR_VOID_STAR_CAST(label),        \
				   PANIC_CBOR_VOID_STAR_CAST(value) },      \
		2)

#endif /* _PANIC_CBOR_H_ */
