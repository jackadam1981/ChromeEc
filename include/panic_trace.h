
/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PANIC_TRACE_H
#define __CROS_EC_PANIC_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

enum panic_trace_tag_t {
    PANIC_TRACE_TAG_NULL = 0,
    PANIC_TRACE_TAG_BYTE0,
    PANIC_TRACE_TAG_BYTE1,
    PANIC_TRACE_TAG_BYTE2,
    PANIC_TRACE_TAG_BYTE3,
    PANIC_TRACE_TAG_IRQ,
    PANIC_TRACE_TAG_TASK_SWITCH,
    PANIC_TRACE_TAG_TASK_SET_EVENT,
    PANIC_TRACE_TAG_TICK,
    PANIC_TRACE_TAG_HOST_CMD,
    PANIC_TRACE_TAG_MUTEX_LOCK,
    PANIC_TRACE_TAG_MUTEX_UNLOCK,
};

void panic_trace_init(void);

void panic_trace_add_0(uint8_t tag);

void panic_trace_add_1(uint8_t tag, uint8_t value);

void panic_trace_add_2(uint8_t tag, uint8_t value0, uint8_t value1);

void panic_trace_add_3(uint8_t tag, uint8_t value0, uint8_t value1, uint8_t value2);

void panic_trace_add_4(uint8_t tag, uint8_t value0, uint8_t value1, uint8_t value2, uint8_t value3);

void panic_trace_add_uint16_t(uint8_t tag, uint16_t value);

void panic_trace_add_uint32_t(uint8_t tag, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* __CROS_EC_PANIC_TRACE_H */