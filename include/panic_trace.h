/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PANIC_TRACE_H
#define __CROS_EC_PANIC_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

enum panic_trace_tag_t {
	PANIC_TRACE_TAG_NULL,
	PANIC_TRACE_TAG_EXTRA_BYTE,
	PANIC_TRACE_TAG_IRQ_START,
	PANIC_TRACE_TAG_IRQ_END,
	PANIC_TRACE_TAG_SVC_CALL,
	PANIC_TRACE_TAG_TASK_SWITCH_IN,
	PANIC_TRACE_TAG_TASK_SWITCH_OUT,
	PANIC_TRACE_TAG_TASK_SLEEP_ENTER,
	PANIC_TRACE_TAG_TASK_SLEEP_EXIT,
	PANIC_TRACE_TAG_TASK_BUSY_WAIT_ENTER,
	PANIC_TRACE_TAG_TASK_BUSY_WAIT_EXIT,
	PANIC_TRACE_TAG_TASK_YIELD,
	PANIC_TRACE_TAG_TASK_SUSPEND,
	PANIC_TRACE_TAG_TASK_RESUME,
	PANIC_TRACE_TAG_TASK_WAKEUP,
	PANIC_TRACE_TAG_TASK_READY,
	PANIC_TRACE_TAG_TASK_PEND,
	PANIC_TRACE_TAG_TASK_SET_EVENT,
	PANIC_TRACE_TAG_TICK,
	PANIC_TRACE_TAG_HOST_CMD,
	PANIC_TRACE_TAG_HOST_EVENT_SET,
	PANIC_TRACE_TAG_HOST_EVENT_CLEAR,
	PANIC_TRACE_TAG_MUTEX_LOCK_ENTER,
	PANIC_TRACE_TAG_MUTEX_LOCK_BLOCKING,
	PANIC_TRACE_TAG_MUTEX_LOCK_EXIT,
	PANIC_TRACE_TAG_MUTEX_UNLOCK_ENTER,
	PANIC_TRACE_TAG_MUTEX_UNLOCK_EXIT,
	PANIC_TRACE_TAG_IDLE,
	PANIC_TRACE_TAG_WATCHDOG_RELOAD,
	PANIC_TRACE_TAG_TASK_ENTRY,
	PANIC_TRACE_TAG_TASK_READY_BITMASK,
	PANIC_TRACE_TAG_ELAPSED_US,
	PANIC_TRACE_TAG_ELAPSED_OVERFLOW,
	PANIC_TRACE_TAG_COUNT,
};

#ifdef SECTION_IS_RO

static inline void panic_trace_write_0(uint8_t tag)
{
}

static inline void panic_trace_write_1(uint8_t tag, uint8_t value)
{
}

static inline bool panic_trace_tag_is_enabled(uint8_t tag)
{
	return false;
}

static inline void panic_trace_write_2(uint8_t tag, uint8_t value0,
				       uint8_t value1)
{
}

static inline void panic_trace_write_3(uint8_t tag, uint8_t value0,
				       uint8_t value1, uint8_t value2)
{
}

static inline void panic_trace_write_4(uint8_t tag, uint8_t value0,
				       uint8_t value1, uint8_t value2,
				       uint8_t value3)
{
}

static inline void panic_trace_write_uint16(uint8_t tag, uint16_t value)
{
}

static inline void panic_trace_write_uint32(uint8_t tag, uint32_t value)
{
}

static inline void panic_trace_dump(void)
{
}

#else

void panic_trace_write_0(uint8_t tag);

void panic_trace_write_1(uint8_t tag, uint8_t value);

bool panic_trace_tag_is_enabled(uint8_t tag);

void panic_trace_write_2(uint8_t tag, uint8_t value0, uint8_t value1);

void panic_trace_write_3(uint8_t tag, uint8_t value0, uint8_t value1,
			 uint8_t value2);

void panic_trace_write_4(uint8_t tag, uint8_t value0, uint8_t value1,
			 uint8_t value2, uint8_t value3);

void panic_trace_write_uint16(uint8_t tag, uint16_t value);

void panic_trace_write_uint32(uint8_t tag, uint32_t value);

void panic_trace_dump(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __CROS_EC_PANIC_TRACE_H */
