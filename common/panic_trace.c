/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "ec_tasks.h"
#include "panic.h"
#include "panic_trace.h"
#include "preserved_ring_buf.h"
#include "time.h"
#include "util.h"
#include "watchdog.h"

#include <zephyr/sys/time_units.h>

#include <wait_q.h>

#ifdef CONFIG_RISCV
#include <zephyr/drivers/interrupt_controller/riscv_plic.h>
#endif

typedef union {
	struct {
		uint8_t tag;
		uint8_t value;
	} entry;
	uint16_t val;
} panic_trace_entry_t;

static uint32_t last_timestamp;

/* Panic trace defaults to frozen */
test_export_static bool panic_trace_frozen = true;
test_export_static bool panic_trace_initialized = false;
test_export_static uint8_t panic_trace_mask[PANIC_TRACE_TAG_COUNT] = {
	[PANIC_TRACE_TAG_EXTRA_BYTE] = 1,
	[PANIC_TRACE_TAG_IRQ_START] = 1,
	[PANIC_TRACE_TAG_IRQ_END] = 1,
	[PANIC_TRACE_TAG_SVC_CALL] = 1,
	[PANIC_TRACE_TAG_TASK_SWITCH_IN] = 1,
	[PANIC_TRACE_TAG_TASK_SWITCH_OUT] = 0,
	[PANIC_TRACE_TAG_TASK_SET_EVENT] = 1,
	[PANIC_TRACE_TAG_TASK_BUSY_WAIT_ENTER] = 0,
	[PANIC_TRACE_TAG_TASK_BUSY_WAIT_EXIT] = 0,
	[PANIC_TRACE_TAG_TASK_SLEEP_ENTER] = 1,
	[PANIC_TRACE_TAG_TASK_SLEEP_EXIT] = 1,
	[PANIC_TRACE_TAG_TASK_READY] = 1,
	[PANIC_TRACE_TAG_TASK_PEND] = 1,
	[PANIC_TRACE_TAG_TICK] = 1,
	[PANIC_TRACE_TAG_HOST_CMD] = 1,
	[PANIC_TRACE_TAG_HOST_EVENT_SET] = 1,
	[PANIC_TRACE_TAG_HOST_EVENT_CLEAR] = 1,
	[PANIC_TRACE_TAG_MUTEX_LOCK_ENTER] = 1,
	[PANIC_TRACE_TAG_MUTEX_LOCK_BLOCKING] = 1,
	[PANIC_TRACE_TAG_MUTEX_LOCK_EXIT] = 1,
	[PANIC_TRACE_TAG_MUTEX_UNLOCK_ENTER] = 1,
	[PANIC_TRACE_TAG_MUTEX_UNLOCK_EXIT] = 1,
	[PANIC_TRACE_TAG_IDLE] = 0,
	[PANIC_TRACE_TAG_WATCHDOG_RESET] = 1,
	[PANIC_TRACE_TAG_TASK_ENTRY] = 1,
	[PANIC_TRACE_TAG_TASK_READY_BITMASK] = 1,
	[PANIC_TRACE_TAG_ELAPSED_US] = 1,
	[PANIC_TRACE_TAG_ELAPSED_MS] = 1,
	[PANIC_TRACE_TAG_ELAPSED_SEC] = 1,
	[PANIC_TRACE_TAG_ELAPSED_OVERFLOW] = 1,
	[PANIC_TRACE_TAG_TASK_YIELD] = 1,
	[PANIC_TRACE_TAG_TASK_SUSPEND] = 1,
	[PANIC_TRACE_TAG_TASK_RESUME] = 1,
	[PANIC_TRACE_TAG_TASK_WAKEUP] = 1,
};

/* Update if the schema of the panic trace changes */
#define PANIC_TRACE_VERSION 2

/* Declare preserved ring buffer */
DECLARE_PRESERVED_RING_BUF(uint16_t, panic_trace, CONFIG_PANIC_TRACE_SIZE,
			   PANIC_TRACE_VERSION);

/* Basic access functions */
test_export_static uint32_t panic_trace_len(void)
{
	return preserved_ring_buf_len(panic_trace);
}

test_export_static uint32_t panic_trace_capacity(void)
{
	return preserved_ring_buf_capacity(panic_trace);
}

test_export_static uint32_t panic_trace_version(void)
{
	return preserved_ring_buf_version(panic_trace);
}

test_export_static bool panic_trace_is_valid(void)
{
	return preserved_ring_buf_verify(panic_trace);
}

test_export_static uint16_t panic_trace_read(uint32_t offset)
{
	return preserved_ring_buf_read(panic_trace, offset);
}

static inline int panic_trace_atomic_begin(void)
{
	return irq_lock();
}

static inline void panic_trace_atomic_end(int key)
{
	irq_unlock(key);
}

/* Returns the panic trace freeze state before applying the requested freeze
 * state
 */
test_export_static bool panic_trace_freeze(bool freeze)
{
	bool orig_frozen = panic_trace_frozen;
	panic_trace_frozen = freeze;
	return orig_frozen;
}

/* Resets the panic trace and defaults to frozen */
test_export_static void panic_trace_reset(void)
{
	panic_trace_frozen = true;
	preserved_ring_buf_reset(panic_trace);
}

/* Check if panic data is new */
static bool panic_data_is_new(void)
{
	struct panic_data *pdata = panic_get_data();
	return !!pdata && !(pdata->flags & PANIC_DATA_FLAG_OLD_HOSTCMD);
}

test_export_static void panic_trace_init(void)
{
	bool freeze;
	/* Initialize may only run once */
	if (panic_trace_initialized)
		return;
	panic_trace_initialized = true;

	/* Add idle thread to map at index 0 */
	thread_to_id_map[thread_id_index++] = get_idle_thread();

	if (!panic_trace_is_valid()) {
		if (IS_ENABLED(CONFIG_PANIC_TRACE_DEBUG))
			ccprintf(
				"Panic trace is invalid, resetting and unfreezing panic trace\n");
		panic_trace_reset();
		freeze = false;
	} else if (panic_data_is_new()) {
		if (IS_ENABLED(CONFIG_PANIC_TRACE_DEBUG))
			ccprintf(
				"New panic detected, keeping panic trace frozen\n");
		freeze = true;
	} else {
		if (IS_ENABLED(CONFIG_PANIC_TRACE_DEBUG))
			ccprintf("No new panic, unfreezing panic trace\n");
		freeze = false;
	}
	panic_trace_freeze(freeze);
}
DECLARE_HOOK(HOOK_INIT_EARLY, panic_trace_init, HOOK_PRIO_DEFAULT - 1);

void panic_trace_write_elapsed(void)
{
	panic_trace_entry_t entry0;
	panic_trace_entry_t entry1;
	bool extra_entry = false;
	uint32_t now = get_time().le.lo;
	uint32_t elapsed_us = now - last_timestamp;
	uint32_t elapsed_ms = elapsed_us / 1000;
	uint32_t elapsed_s = elapsed_ms / 1000;
	last_timestamp = now;
	if (elapsed_us == 0)
		return;
	if (elapsed_us < 256) {
		return;
	} else if (elapsed_us < 65536) {
		entry0.entry.tag = PANIC_TRACE_TAG_EXTRA_BYTE;
		entry0.entry.value = (elapsed_us >> 8) & 0xff;
		entry1.entry.tag = PANIC_TRACE_TAG_ELAPSED_US;
		entry1.entry.value = elapsed_us & 0xFF;
		extra_entry = true;
	} else if (elapsed_ms < 256) {
		entry0.entry.tag = PANIC_TRACE_TAG_ELAPSED_MS;
		entry0.entry.value = elapsed_ms;
	} else if (elapsed_ms < 65536) {
		entry0.entry.tag = PANIC_TRACE_TAG_EXTRA_BYTE;
		entry0.entry.value = (elapsed_ms >> 8) & 0xff;
		entry1.entry.tag = PANIC_TRACE_TAG_ELAPSED_MS;
		entry1.entry.value = elapsed_ms & 0xFF;
		extra_entry = true;
	} else if (elapsed_s < 256) {
		entry0.entry.tag = PANIC_TRACE_TAG_ELAPSED_SEC;
		entry0.entry.value = elapsed_s;
	} else {
		entry0.entry.tag = PANIC_TRACE_TAG_ELAPSED_OVERFLOW;
		entry0.entry.value = 0;
	}

	preserved_ring_buf_write(panic_trace, entry0.val);
	if (extra_entry) {
		preserved_ring_buf_write(panic_trace, entry1.val);
	}
}

/**
 * @brief Writes a panic trace entry with only a tag.
 *
 * This function adds a panic trace entry to the panic trace buffer. The entry
 * consists of a tag. The tag identifies the type of event that
 * is being traced.
 *
 * @param tag The tag of the panic trace entry.
 */
void panic_trace_write_0(uint8_t tag)
{
	panic_trace_write_1(tag, 0);
}

/**
 * @brief Adds a panic trace entry with a tag and a value.
 *
 * This function adds a panic trace entry to the panic trace buffer. The entry
 * consists of a tag and a value. The tag identifies the type of event that
 * is being traced, and the value provides additional tag specific information
 * about the event.
 *
 * @param tag The tag of the panic trace entry.
 * @param value The value of the panic trace entry.
 */
void panic_trace_write_1(uint8_t tag, uint8_t value)
{
	panic_trace_entry_t entry = {
		.entry.tag = tag,
		.entry.value = value,
	};
	if (panic_trace_frozen || !panic_trace_mask[tag])
		return;
	int key = panic_trace_atomic_begin();
	panic_trace_write_elapsed();
	preserved_ring_buf_write(panic_trace, entry.val);
	panic_trace_atomic_end(key);
}

void panic_trace_write_2(uint8_t tag, uint8_t value0, uint8_t value1)
{
	panic_trace_entry_t entries[] = {
		{
			.entry.tag = PANIC_TRACE_TAG_EXTRA_BYTE,
			.entry.value = value0,
		},
		{
			.entry.tag = tag,
			.entry.value = value1,
		}
	};
	if (panic_trace_frozen || !panic_trace_mask[tag])
		return;
	int key = panic_trace_atomic_begin();
	panic_trace_write_elapsed();
	preserved_ring_buf_write(panic_trace, entries[0].val);
	preserved_ring_buf_write(panic_trace, entries[1].val);
	panic_trace_atomic_end(key);
}

void panic_trace_write_3(uint8_t tag, uint8_t value0, uint8_t value1,
			 uint8_t value2)
{
	panic_trace_entry_t entries[] = {
		{
			.entry.tag = PANIC_TRACE_TAG_EXTRA_BYTE,
			.entry.value = value0,
		},
		{
			.entry.tag = PANIC_TRACE_TAG_EXTRA_BYTE,
			.entry.value = value1,
		},
		{
			.entry.tag = tag,
			.entry.value = value2,
		}
	};
	if (panic_trace_frozen || !panic_trace_mask[tag])
		return;
	int key = panic_trace_atomic_begin();
	panic_trace_write_elapsed();
	preserved_ring_buf_write(panic_trace, entries[0].val);
	preserved_ring_buf_write(panic_trace, entries[1].val);
	preserved_ring_buf_write(panic_trace, entries[2].val);
	panic_trace_atomic_end(key);
}

void panic_trace_write_4(uint8_t tag, uint8_t value0, uint8_t value1,
			 uint8_t value2, uint8_t value3)
{
	panic_trace_entry_t entries[] = {
		{
			.entry.tag = PANIC_TRACE_TAG_EXTRA_BYTE,
			.entry.value = value0,
		},
		{
			.entry.tag = PANIC_TRACE_TAG_EXTRA_BYTE,
			.entry.value = value1,
		},
		{
			.entry.tag = PANIC_TRACE_TAG_EXTRA_BYTE,
			.entry.value = value2,
		},
		{
			.entry.tag = tag,
			.entry.value = value3,
		}
	};
	if (panic_trace_frozen || !panic_trace_mask[tag])
		return;
	int key = panic_trace_atomic_begin();
	panic_trace_write_elapsed();
	preserved_ring_buf_write(panic_trace, entries[0].val);
	preserved_ring_buf_write(panic_trace, entries[1].val);
	preserved_ring_buf_write(panic_trace, entries[2].val);
	preserved_ring_buf_write(panic_trace, entries[3].val);
	panic_trace_atomic_end(key);
}

void panic_trace_write_uint16(uint8_t tag, uint16_t value)
{
	panic_trace_write_2(tag, value >> 8, value & 0xff);
}

void panic_trace_write_uint32(uint8_t tag, uint32_t value)
{
	panic_trace_write_4(tag, value >> 24, value >> 16, value >> 8,
			    value & 0xff);
}

static uint32_t current_thread_index = -1;
static k_tid_t thread_to_id_map[32] = { 0 };
static uint32_t thread_id_index = 0;

/**
 * Periodically adds a ms timestamp to the panic trace.
 */
static void panic_trace_tick(void)
{
	uint32_t now = clock();
	panic_trace_write_uint32(PANIC_TRACE_TAG_TICK, now);
}
DECLARE_HOOK(HOOK_TICK, panic_trace_tick, HOOK_PRIO_DEFAULT);

void sys_trace_thread_create_user(struct k_thread *new_thread)
{
	int thread_index = -1;
	if (thread_id_index < ARRAY_SIZE(thread_to_id_map)) {
		thread_index = thread_id_index;
		thread_to_id_map[thread_id_index++] = new_thread;
	}
	if (thread_index >= 0) {
#if defined(CONFIG_THREAD_MONITOR)
		panic_trace_write_1(PANIC_TRACE_TAG_TASK_ENTRY, thread_index);
		panic_trace_write_uint32(
			PANIC_TRACE_TAG_TASK_ENTRY,
			(uint32_t)(uintptr_t)new_thread->entry.pEntry);
#endif
	}
}

int thread_to_index(struct k_thread *thread)
{
	for (int i = 0; i < thread_id_index; i++) {
		if (thread_to_id_map[i] == thread) {
			return i;
		}
	}
	return -1;
}

void sys_trace_thread_switched_in_user(void)
{
	if (panic_trace_frozen)
		return;
	k_tid_t current_thread = k_current_get();
	int thread_index = thread_to_index(current_thread);
	current_thread_index = thread_index;
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_SWITCH_IN,
			    current_thread_index);
}

void sys_trace_thread_switched_out_user(void)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_SWITCH_OUT,
			    current_thread_index);
}

void sys_trace_thread_sleep_enter_user(k_timeout_t timeout)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_SLEEP_ENTER,
			    current_thread_index);
}

void sys_trace_thread_sleep_exit_user(k_timeout_t timeout, int ret)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_SLEEP_EXIT,
			    current_thread_index);
}

void sys_trace_thread_msleep_enter_user(int32_t ms)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_SLEEP_ENTER,
			    current_thread_index);
}

void sys_trace_thread_msleep_exit_user(int32_t ms, int ret)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_SLEEP_EXIT,
			    current_thread_index);
}

void sys_trace_thread_usleep_enter_user(int32_t us)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_SLEEP_ENTER,
			    current_thread_index);
}

void sys_trace_thread_usleep_exit_user(int32_t us, int ret)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_SLEEP_EXIT,
			    current_thread_index);
}

void sys_trace_thread_busy_wait_enter_user(uint32_t usec_to_wait)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_BUSY_WAIT_ENTER,
			    current_thread_index);
}

void sys_trace_thread_busy_wait_exit_user(uint32_t usec_to_wait)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_BUSY_WAIT_EXIT,
			    current_thread_index);
}

void sys_trace_thread_wakeup_user(struct k_thread *thread)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_WAKEUP,
			    thread_to_index(thread));
}

void sys_trace_thread_yield_user(void)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_YIELD, current_thread_index);
}

void sys_trace_thread_suspend_enter_user(struct k_thread *thread)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_SUSPEND,
			    thread_to_index(thread));
}

void sys_trace_thread_resume_enter_user(struct k_thread *thread)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_RESUME,
			    thread_to_index(thread));
}

void sys_port_trace_k_thread_sched_wakeup_user(struct k_thread *thread)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_WAKEUP,
			    thread_to_index(thread));
}

void sys_port_trace_k_thread_sched_ready_user(struct k_thread *thread)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_READY,
			    thread_to_index(thread));
}

void sys_port_trace_k_thread_sched_pend_user(struct k_thread *thread)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_PEND, thread_to_index(thread));
}

void sys_port_trace_k_thread_sched_resume_user(struct k_thread *thread)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_RESUME,
			    thread_to_index(thread));
}

void sys_port_trace_k_thread_sched_suspend_user(struct k_thread *thread)
{
	panic_trace_write_1(PANIC_TRACE_TAG_TASK_SUSPEND,
			    thread_to_index(thread));
}

#ifdef CONFIG_RISCV
void sys_trace_isr_enter_user(void)
{
	uint64_t mcause;

	mcause = csr_read(mcause);
	mcause &= CONFIG_RISCV_MCAUSE_EXCEPTION_MASK;

	if (mcause == RISCV_IRQ_MEXT) {
		return;
	}
	panic_trace_write_1(PANIC_TRACE_TAG_IRQ_START, mcause);
}

void sys_trace_isr_exit_user(void)
{
	uint64_t mcause;

	mcause = csr_read(mcause);
	mcause &= CONFIG_RISCV_MCAUSE_EXCEPTION_MASK;

	if (mcause == RISCV_IRQ_MEXT) {
		return;
	}
	panic_trace_write_1(PANIC_TRACE_TAG_IRQ_END, mcause);
}
#else
void sys_trace_isr_enter_user(void)
{
}

void sys_trace_isr_exit_user(void)
{
}
#endif

void sys_trace_idle_user(void)
{
	panic_trace_write_0(PANIC_TRACE_TAG_IDLE);
}

void sys_trace_mutex_lock_enter_user(struct k_mutex *mutex, k_timeout_t timeout)
{
	panic_trace_write_uint32(PANIC_TRACE_TAG_MUTEX_LOCK_ENTER,
				 (uint32_t)(uintptr_t)mutex);
}

void sys_trace_mutex_lock_blocking_user(struct k_mutex *mutex,
					k_timeout_t timeout)
{
	panic_trace_write_uint32(PANIC_TRACE_TAG_MUTEX_LOCK_BLOCKING,
				 (uint32_t)(uintptr_t)mutex);
}

void sys_trace_mutex_lock_exit_user(struct k_mutex *mutex, k_timeout_t timeout,
				    int ret)
{
	panic_trace_write_uint32(PANIC_TRACE_TAG_MUTEX_LOCK_EXIT,
				 (uint32_t)(uintptr_t)mutex);
}

void sys_trace_mutex_unlock_enter_user(struct k_mutex *mutex)
{
	panic_trace_write_uint32(PANIC_TRACE_TAG_MUTEX_UNLOCK_ENTER,
				 (uint32_t)(uintptr_t)mutex);
}

void sys_trace_mutex_unlock_exit_user(struct k_mutex *mutex, int ret)
{
	panic_trace_write_uint32(PANIC_TRACE_TAG_MUTEX_UNLOCK_EXIT,
				 (uint32_t)(uintptr_t)mutex);
}

void panic_trace_dump(void)
{
	uint8_t extra_bytes_count = 0;
	uint8_t extra_bytes[8] = { 0 };
	bool orig_frozen = panic_trace_freeze(true);
	k_tid_t thread;
	struct k_mutex *mutex;
	panic_printf("=== Panic Trace Start ===\n");
	watchdog_reload();
	for (int i = 0; i < panic_trace_len(); i++) {
		panic_trace_entry_t entry;
		entry.val = panic_trace_read(i);
		uint8_t value = entry.entry.value;
		uint8_t tag = entry.entry.tag;
		if (i % 16 == 0)
			watchdog_reload();
		switch (tag) {
		case PANIC_TRACE_TAG_EXTRA_BYTE:
			extra_bytes[extra_bytes_count++] = value;
			break;
		case PANIC_TRACE_TAG_ELAPSED_US:
			if (extra_bytes_count == 0) {
				panic_printf("%dus ", value);
			} else if (extra_bytes_count == 1) {
				panic_printf("%dus ",
					     extra_bytes[0] << 8 | value);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_ELAPSED_MS:
			if (extra_bytes_count == 0) {
				panic_printf("%dms ", value);
			} else if (extra_bytes_count == 1) {
				panic_printf("%dms ",
					     extra_bytes[0] << 8 | value);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_ELAPSED_SEC:
			if (extra_bytes_count == 0) {
				panic_printf("%ds ", value);
			} else if (extra_bytes_count == 1) {
				panic_printf("%ds ",
					     extra_bytes[0] << 8 | value);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_ELAPSED_OVERFLOW:
			panic_printf("Elapsed: OVERFLOW\n");
			break;
		case PANIC_TRACE_TAG_IRQ_START:
			panic_printf("IRQ Start: %d\n", value);
			break;
		case PANIC_TRACE_TAG_IRQ_END:
			panic_printf("IRQ End: %d\n", value);
			break;
		case PANIC_TRACE_TAG_SVC_CALL:
			panic_printf("Service Call\n");
			break;
		case PANIC_TRACE_TAG_TASK_SWITCH_IN:
			thread = (value < ARRAY_SIZE(thread_to_id_map)) ?
					 thread_to_id_map[value] :
					 NULL;
			if (thread == NULL) {
				panic_printf("Thread Switch: %d\n", value);
			} else {
				panic_printf("Thread Switch: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_SWITCH_OUT:
			thread = (value < ARRAY_SIZE(thread_to_id_map)) ?
					 thread_to_id_map[value] :
					 NULL;
			if (thread == NULL) {
				panic_printf("Thread Switch Out: %d\n", value);
			} else {
				panic_printf("Thread Switch Out: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_SLEEP_ENTER:
			thread = (value < ARRAY_SIZE(thread_to_id_map)) ?
					 thread_to_id_map[value] :
					 NULL;
			if (thread == NULL) {
				panic_printf("Task sleep enter: %d\n", value);
			} else {
				panic_printf("Task sleep enter: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_SLEEP_EXIT:
			thread = (value < ARRAY_SIZE(thread_to_id_map)) ?
					 thread_to_id_map[value] :
					 NULL;
			if (thread == NULL) {
				panic_printf("Task sleep exit: %d\n", value);
			} else {
				panic_printf("Task sleep exit: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_BUSY_WAIT_ENTER:
			thread = (value < ARRAY_SIZE(thread_to_id_map)) ?
					 thread_to_id_map[value] :
					 NULL;
			if (thread == NULL) {
				panic_printf("Task busy wait enter: %d\n",
					     value);
			} else {
				panic_printf("Task busy wait enter: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_BUSY_WAIT_EXIT:
			thread = (value < ARRAY_SIZE(thread_to_id_map)) ?
					 thread_to_id_map[value] :
					 NULL;
			if (thread == NULL) {
				panic_printf("Task busy wait exit: %d\n",
					     value);
			} else {
				panic_printf("Task busy wait exit: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_YIELD:
			panic_printf("Task yield\n");
			break;
		case PANIC_TRACE_TAG_TASK_SUSPEND:
			thread = (value < ARRAY_SIZE(thread_to_id_map)) ?
					 thread_to_id_map[value] :
					 NULL;
			if (thread == NULL) {
				panic_printf("Task suspend: %d\n", value);
			} else {
				panic_printf("Task suspend: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_RESUME:
			thread = (value < ARRAY_SIZE(thread_to_id_map)) ?
					 thread_to_id_map[value] :
					 NULL;
			if (thread == NULL) {
				panic_printf("Task resume: %d\n", value);
			} else {
				panic_printf("Task resume: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_WAKEUP:
			thread = (value < ARRAY_SIZE(thread_to_id_map)) ?
					 thread_to_id_map[value] :
					 NULL;
			if (thread == NULL) {
				panic_printf("Task wakeup: %d\n", value);
			} else {
				panic_printf("Task wakeup: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_READY:
			thread = (value < ARRAY_SIZE(thread_to_id_map)) ?
					 thread_to_id_map[value] :
					 NULL;
			if (thread == NULL) {
				panic_printf("Task ready: %d\n", value);
			} else {
				panic_printf("Task ready: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_PEND:
			thread = (value < ARRAY_SIZE(thread_to_id_map)) ?
					 thread_to_id_map[value] :
					 NULL;
			if (thread == NULL) {
				panic_printf("Task pend: %d\n", value);
			} else {
				panic_printf("Task pend: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_SET_EVENT:
			if (extra_bytes_count == 1) {
				panic_printf("Task Event: task=%d event=%x\n",
					     extra_bytes[0], 1 << value);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_TICK:
			if (extra_bytes_count == 3) {
				uint32_t clock_value = extra_bytes[0] << 24 |
						       extra_bytes[1] << 16 |
						       extra_bytes[2] << 8 |
						       value;
				panic_printf("Tick: %d\n", clock_value);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_HOST_CMD:
			if (extra_bytes_count == 1) {
				uint16_t cmd = extra_bytes[0] << 8 | value;
				panic_printf("Host Cmd: %d\n", cmd);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_MUTEX_LOCK_ENTER:
			if (extra_bytes_count == 3) {
				mutex = (struct k_mutex *)(extra_bytes[0]
								   << 24 |
							   extra_bytes[1]
								   << 16 |
							   extra_bytes[2] << 8 |
							   value);
				panic_printf("Mutex Lock Enter: %p\n", mutex);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_MUTEX_LOCK_BLOCKING:
			if (extra_bytes_count == 3) {
				mutex = (struct k_mutex *)(extra_bytes[0]
								   << 24 |
							   extra_bytes[1]
								   << 16 |
							   extra_bytes[2] << 8 |
							   value);
				panic_printf("Mutex Lock Blocking: %p\n",
					     mutex);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_MUTEX_LOCK_EXIT:
			if (extra_bytes_count == 3) {
				mutex = (struct k_mutex *)(extra_bytes[0]
								   << 24 |
							   extra_bytes[1]
								   << 16 |
							   extra_bytes[2] << 8 |
							   value);
				panic_printf("Mutex Lock Exit: %p\n", mutex);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_MUTEX_UNLOCK_ENTER:
			if (extra_bytes_count == 3) {
				mutex = (struct k_mutex *)(extra_bytes[0]
								   << 24 |
							   extra_bytes[1]
								   << 16 |
							   extra_bytes[2] << 8 |
							   value);
				panic_printf("Mutex Unlock Enter: %p\n", mutex);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_MUTEX_UNLOCK_EXIT:
			if (extra_bytes_count == 3) {
				mutex = (struct k_mutex *)(extra_bytes[0]
								   << 24 |
							   extra_bytes[1]
								   << 16 |
							   extra_bytes[2] << 8 |
							   value);
				panic_printf("Mutex Unlock Exit: %p\n", mutex);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_HOST_EVENT_SET:
			if (extra_bytes_count == 1) {
				panic_printf("Host Event Set: %llx\n",
					     (uint64_t)1 << value);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_HOST_EVENT_CLEAR:
			if (extra_bytes_count == 1) {
				panic_printf("Host Event Clear: %llx\n",
					     (uint64_t)1 << value);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_IDLE:
			panic_printf("Idle\n");
			break;
		case PANIC_TRACE_TAG_WATCHDOG_RESET:
			panic_printf("Watchdog Reset\n");
			break;
		case PANIC_TRACE_TAG_TASK_ENTRY:
			if (extra_bytes_count == 0) {
				panic_printf("Task Entry: task=%d ", value);
			} else if (extra_bytes_count == 3) {
				uint32_t entry_addr = extra_bytes[0] << 24 |
						      extra_bytes[1] << 16 |
						      extra_bytes[2] << 8 |
						      value;
				panic_printf("addr=%p\n",
					     (void *)(uintptr_t)entry_addr);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_TASK_READY_BITMASK:
			if (extra_bytes_count == 3) {
				uint32_t mask = extra_bytes[0] << 24 |
						extra_bytes[1] << 16 |
						extra_bytes[2] << 8 | value;
				panic_printf("Ready Mask: 0x%x\n", mask);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		default:
			if (extra_bytes_count == 0) {
				panic_printf("Unknown: tag=0x%x value=0x%x\n",
					     tag, value);
			} else {
				panic_printf(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		}
		if (tag != PANIC_TRACE_TAG_EXTRA_BYTE) {
			extra_bytes_count = 0;
		}
		cflush();
	}
	panic_trace_freeze(orig_frozen);
}

#if defined(CONFIG_PANIC_TRACE_DEBUG)

test_export_static void panic_trace_corrupt(void)
{
	panic_trace->properties->checksum = -1;
}

static int command_panic_trace(int argc, const char **argv)
{
	if ((argc == 1) || (argc == 2 && !strcasecmp(argv[1], "info"))) {
		bool orig_frozen = panic_trace_freeze(true);
		ccprintf("Valid: %d\n", panic_trace_is_valid());
		ccprintf("Frozen: %d\n", orig_frozen);
		ccprintf("Length: %d\n", panic_trace_len());
		ccprintf("Capacity: %d\n", panic_trace_capacity());
		ccprintf("Version: %d\n", panic_trace_version());
		panic_trace_dump();
		panic_trace_freeze(orig_frozen);
	} else if (argc == 2 && !strcasecmp(argv[1], "dump")) {
		panic_trace_dump();
	} else if (argc == 2 && !strcasecmp(argv[1], "reset")) {
		bool orig_frozen = panic_trace_freeze(true);
		panic_trace_reset();
		panic_trace_freeze(orig_frozen);
		ccprintf("Panic trace reset, currently %s\n",
			 orig_frozen ? "frozen" : "unfrozen");
	} else if (argc == 2 && !strcasecmp(argv[1], "freeze")) {
		bool orig_frozen = panic_trace_freeze(true);
		if (orig_frozen) {
			ccprintf("Panic trace already frozen\n");
		} else {
			ccprintf("Panic trace frozen\n");
		}
	} else if (argc == 2 && !strcasecmp(argv[1], "unfreeze")) {
		bool orig_frozen = panic_trace_freeze(false);
		if (!orig_frozen) {
			ccprintf("Panic trace already unfrozen\n");
		} else {
			ccprintf("Panic trace unfrozen\n");
		}
	} else if (argc == 2 && !strcasecmp(argv[1], "corrupt")) {
		panic_trace_corrupt();
		ccprintf("Panic trace corrupted\n");
	} else {
		return EC_ERROR_PARAM1;
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(panictrace, command_panic_trace,
			"[info | dump | reset | freeze | unfreeze | corrupt]",
			"Panic trace");

#endif /* CONFIG_PANIC_TRACE_DEBUG */
