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

#define PANIC_TRACE_TAG_BIT(tag, index) \
	((tag) / 32 == (index) ? BIT((tag) % 32) : 0)

#define PANIC_TRACE_ENABLED_TAGS                                              \
	PANIC_TRACE_TAG_EXTRA_BYTE, \
	PANIC_TRACE_TAG_IRQ_START, \
	PANIC_TRACE_TAG_IRQ_END, \
	PANIC_TRACE_TAG_SVC_CALL, \
	PANIC_TRACE_TAG_TASK_SWITCH_IN, \
	PANIC_TRACE_TAG_TASK_SWITCH_OUT, \
	PANIC_TRACE_TAG_TASK_SET_EVENT, \
	PANIC_TRACE_TAG_TASK_SLEEP_ENTER, \
	PANIC_TRACE_TAG_TASK_SLEEP_EXIT, \
	PANIC_TRACE_TAG_TASK_BUSY_WAIT_ENTER, \
	PANIC_TRACE_TAG_TASK_BUSY_WAIT_EXIT, \
	PANIC_TRACE_TAG_TASK_READY, \
	PANIC_TRACE_TAG_TASK_PEND, \
	PANIC_TRACE_TAG_TICK, \
	PANIC_TRACE_TAG_HOST_CMD, \
	PANIC_TRACE_TAG_HOST_EVENT_SET, \
	PANIC_TRACE_TAG_HOST_EVENT_CLEAR, \
	PANIC_TRACE_TAG_MUTEX_LOCK_ENTER, \
	PANIC_TRACE_TAG_MUTEX_LOCK_BLOCKING, \
	PANIC_TRACE_TAG_MUTEX_LOCK_EXIT, \
	PANIC_TRACE_TAG_MUTEX_UNLOCK_ENTER, \
	PANIC_TRACE_TAG_MUTEX_UNLOCK_EXIT, \
	PANIC_TRACE_TAG_IDLE, \
	PANIC_TRACE_TAG_WATCHDOG_RELOAD, \
	PANIC_TRACE_TAG_TASK_ENTRY, \
	PANIC_TRACE_TAG_TASK_READY_BITMASK, \
	PANIC_TRACE_TAG_ELAPSED_US, \
	PANIC_TRACE_TAG_ELAPSED_OVERFLOW, \
	PANIC_TRACE_TAG_TASK_YIELD, \
	PANIC_TRACE_TAG_TASK_SUSPEND, \
	PANIC_TRACE_TAG_TASK_RESUME, \
	PANIC_TRACE_TAG_TASK_WAKEUP

#define PANIC_TRACE_FULL_MASK(index)                                          \
	(0 | FOR_EACH_FIXED_ARG(PANIC_TRACE_TAG_BIT, (|), index,              \
				PANIC_TRACE_ENABLED_TAGS))

static uint64_t last_timestamp;


static uint32_t panic_trace_mask[8] = {
	[0] = PANIC_TRACE_FULL_MASK(0), [1] = PANIC_TRACE_FULL_MASK(1),
	[2] = PANIC_TRACE_FULL_MASK(2), [3] = PANIC_TRACE_FULL_MASK(3),
	[4] = PANIC_TRACE_FULL_MASK(4), [5] = PANIC_TRACE_FULL_MASK(5),
	[6] = PANIC_TRACE_FULL_MASK(6), [7] = PANIC_TRACE_FULL_MASK(7),
};

bool panic_trace_tag_is_enabled(uint8_t tag)
{
	return panic_trace_mask[tag / 32] & BIT(tag % 32);
}


/* Panic trace defaults to frozen */
test_export_static bool panic_trace_frozen = true;
test_export_static bool panic_trace_initialized = false;

/* Update if the schema of the panic trace changes */
#define PANIC_TRACE_VERSION 3

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

static struct k_spinlock panic_trace_lock;

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

static uint32_t current_thread_index = -1;
#define THREAD_MAP_SIZE 32
static k_tid_t thread_to_id_map[THREAD_MAP_SIZE] = { 0 };
static uint32_t thread_id_index = 0;

#define HASH_MAP_SIZE 64
static struct k_tid_t thread_index_hash[HASH_MAP_SIZE];
static uint32_t hash_multiplier;
#define HASH_SHIFT 26 /* 32 - 6 for size 64 */

static void compute_perfect_hash(void)
{
	uint32_t m;
	int i;
	int j;

	/* Try to find a multiplier that creates no collisions. Assume 4 byte aligned addresses. */
	for (m = 1; m < 0xFFFF; m += 2) {
		bool collision = false;

		for (i = 0; i < thread_id_index; i++) {
			k_tid_t tid = thread_to_id_map[i];
			/*
			 * Perfect hash function:
			 * index = ((tid >> 2) * m) >> 26
			 */
			uint32_t idx = (((uintptr_t)tid >> 2) * m) >>
				       HASH_SHIFT;
			idx &= (HASH_MAP_SIZE - 1);

			if (thread_index_hash[idx] != NULL) {
				/* Clear previous entries */
				for (j = 0; j < HASH_MAP_SIZE; j++) {
					thread_index_hash[j] = NULL;
				}
				collision = true;
				break;
			}
			thread_index_hash[idx] = tid;
		}

		if (!collision) {
			printk("Perfect hash multiplier: %x\n", m);
			hash_multiplier = m;
			return;
		} else {
			printk("No perfect hash multiplier found\n");
		}
	}
}

static void panic_trace_add_thread(const struct k_thread *thread, void *user_data)
{
	/* Linear map population only - hash computed later */
	if (thread_id_index < ARRAY_SIZE(thread_to_id_map)) {
		thread_to_id_map[thread_id_index++] = (k_tid_t)thread;
	}
}

test_export_static void panic_trace_init(void)
{
	bool freeze;

	BUILD_ASSERT(IS_ENABLED(CONFIG_THREAD_MONITOR),
		     "Panic trace requires CONFIG_THREAD_MONITOR");

	/* Initialize may only run once */
	/* Initialize may only run once */
	if (panic_trace_initialized)
		return;
	panic_trace_initialized = true;

	/* Populate thread map */
	k_thread_foreach(panic_trace_add_thread, NULL);
	compute_perfect_hash();

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

/**
 * @brief Writes the elapsed time since the last call to the panic trace buffer.
 *
 * This function calculates the time elapsed since the last call to this function
 * and writes it to the panic trace buffer. The elapsed time is written in
 * microseconds, milliseconds, or seconds depending on the magnitude of the
 * elapsed time. If the elapsed time is less than ELAPSED_THRESHOLD_US, it is not
 * written to the panic trace buffer.
 */
#define ELAPSED_THRESHOLD_US 64
void panic_trace_write_elapsed(void)
{
	panic_trace_entry_t entry;
	int64_t now = k_uptime_get();
	int64_t elapsed_us = now - last_timestamp;
	int extra_bytes;

	if (elapsed_us < ELAPSED_THRESHOLD_US) {
		return;
	}
	last_timestamp = now;

	/* Calculate extra bytes needed: (bits + 7) / 8 - 1 */
	extra_bytes = DIV_ROUND_UP(64 - __builtin_clzll(elapsed_us), 8) - 1;

	for (int i = extra_bytes; i > 0; i--) {
		entry.entry.tag = PANIC_TRACE_TAG_EXTRA_BYTE;
		entry.entry.value = (elapsed_us >> (i * 8)) & 0xff;
		preserved_ring_buf_write(panic_trace, entry.val);
	}

	entry.entry.tag = PANIC_TRACE_TAG_ELAPSED_US;
	entry.entry.value = elapsed_us & 0xFF;
	preserved_ring_buf_write(panic_trace, entry.val);
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
	if (!panic_trace_tag_is_enabled(tag))
		return;
	if (panic_trace_frozen)
		return;
	K_SPINLOCK(&panic_trace_lock) {
		panic_trace_write_elapsed();
		preserved_ring_buf_write(panic_trace, entry.val);
	}
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
	if (!panic_trace_tag_is_enabled(tag))
		return;
	if (panic_trace_frozen)
		return;
	K_SPINLOCK(&panic_trace_lock) {
		panic_trace_write_elapsed();
		preserved_ring_buf_write(panic_trace, entries[0].val);
		preserved_ring_buf_write(panic_trace, entries[1].val);
	}
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
	if (!panic_trace_tag_is_enabled(tag))
		return;
	if (panic_trace_frozen)
		return;
	K_SPINLOCK(&panic_trace_lock) {
		panic_trace_write_elapsed();
		preserved_ring_buf_write(panic_trace, entries[0].val);
		preserved_ring_buf_write(panic_trace, entries[1].val);
		preserved_ring_buf_write(panic_trace, entries[2].val);
	}
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
	if (!panic_trace_tag_is_enabled(tag))
		return;
	if (panic_trace_frozen)
		return;
	K_SPINLOCK(&panic_trace_lock) {
		panic_trace_write_elapsed();
		preserved_ring_buf_write(panic_trace, entries[0].val);
		preserved_ring_buf_write(panic_trace, entries[1].val);
		preserved_ring_buf_write(panic_trace, entries[2].val);
		preserved_ring_buf_write(panic_trace, entries[3].val);
	}
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

/**
 * Periodically adds a ms timestamp to the panic trace.
 */
static void panic_trace_tick(void)
{
	uint32_t now = clock();
	panic_trace_write_uint32(PANIC_TRACE_TAG_TICK, now);
}
DECLARE_HOOK(HOOK_TICK, panic_trace_tick, HOOK_PRIO_DEFAULT);

int thread_to_index(struct k_thread *thread)
{
	uint32_t idx = (((uintptr_t)thread >> 2) * hash_multiplier) >> HASH_SHIFT;
	idx &= (HASH_MAP_SIZE - 1); /* Safety mask */

	return idx;
}

void sys_trace_thread_switched_in_user(void)
{
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
	panic_trace_write_1(PANIC_TRACE_TAG_IRQ_START, 0);
}

void sys_trace_isr_exit_user(void)
{
	panic_trace_write_1(PANIC_TRACE_TAG_IRQ_END, 0);
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

static struct k_thread *panic_trace_get_thread(uint32_t index)
{
	if (index < ARRAY_SIZE(thread_to_id_map))
		return thread_to_id_map[index];
	return NULL;
}

static uint64_t panic_trace_get_val(const uint8_t *extra_bytes, uint8_t extra_bytes_count,
				    uint8_t value)
{
	uint64_t result = value;

	for (int i = 0; i < extra_bytes_count; i++) {
		result |= (uint64_t)extra_bytes[i]
			  << ((extra_bytes_count - i) * 8);
	}
	return result;
}

#if defined(CONFIG_PANIC_TRACE_DEBUG)

void panic_trace_dump(void)
{
	uint8_t extra_bytes_count = 0;
	uint8_t extra_bytes[8] = { 0 };
	bool orig_frozen = panic_trace_freeze(true);
	bool first_entry = true;
	k_tid_t thread;
	struct k_mutex *mutex;
	printk("=== Panic Trace Start ===\n");
	for (int i = 0; i < panic_trace_len(); i++) {
		panic_trace_entry_t entry;
		entry.val = panic_trace_read(i);
		uint8_t value = entry.entry.value;
		uint8_t tag = entry.entry.tag;
		/* Always skip the first tagged entry,
		 * since the extra bytes may have been truncated.
		 */
		if (first_entry) {
			if (tag != PANIC_TRACE_TAG_EXTRA_BYTE) {
				first_entry = false;
			}
			continue;
		}
		switch (tag) {
		case PANIC_TRACE_TAG_EXTRA_BYTE:
			extra_bytes[extra_bytes_count++] = value;
			break;
		case PANIC_TRACE_TAG_ELAPSED_US:
			if (extra_bytes_count <= 7) {
				printk("Elapsed: %lluus\n", panic_trace_get_val(
							  extra_bytes,
							  extra_bytes_count,
							  value));
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_ELAPSED_OVERFLOW:
			printk("Elapsed: OVERFLOW\n");
			break;
		case PANIC_TRACE_TAG_IRQ_START:
			printk("IRQ Start: %d\n", value);
			break;
		case PANIC_TRACE_TAG_IRQ_END:
			printk("IRQ End: %d\n", value);
			break;
		case PANIC_TRACE_TAG_SVC_CALL:
			printk("Service Call\n");
			break;
		case PANIC_TRACE_TAG_TASK_SWITCH_IN:
			thread = panic_trace_get_thread(value);
			if (thread == NULL) {
				printk("Thread Switch In: %d\n", value);
			} else {
				printk("Thread Switch In: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_SWITCH_OUT:
			thread = panic_trace_get_thread(value);
			if (thread == NULL) {
				printk("Thread Switch Out: %d\n", value);
			} else {
				printk("Thread Switch Out: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_SLEEP_ENTER:
			thread = panic_trace_get_thread(value);
			if (thread == NULL) {
				printk("Task sleep enter: %d\n", value);
			} else {
				printk("Task sleep enter: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_SLEEP_EXIT:
			thread = panic_trace_get_thread(value);
			if (thread == NULL) {
				printk("Task sleep exit: %d\n", value);
			} else {
				printk("Task sleep exit: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_BUSY_WAIT_ENTER:
			thread = panic_trace_get_thread(value);
			if (thread == NULL) {
				printk("Task busy wait enter: %d\n",
					     value);
			} else {
				printk("Task busy wait enter: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_BUSY_WAIT_EXIT:
			thread = panic_trace_get_thread(value);
			if (thread == NULL) {
				printk("Task busy wait exit: %d\n",
					     value);
			} else {
				printk("Task busy wait exit: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_YIELD:
			printk("Task yield\n");
			break;
		case PANIC_TRACE_TAG_TASK_SUSPEND:
			thread = panic_trace_get_thread(value);
			if (thread == NULL) {
				printk("Task suspend: %d\n", value);
			} else {
				printk("Task suspend: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_RESUME:
			thread = panic_trace_get_thread(value);
			if (thread == NULL) {
				printk("Task resume: %d\n", value);
			} else {
				printk("Task resume: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_WAKEUP:
			thread = panic_trace_get_thread(value);
			if (thread == NULL) {
				printk("Task wakeup: %d\n", value);
			} else {
				printk("Task wakeup: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_READY:
			thread = panic_trace_get_thread(value);
			if (thread == NULL) {
				printk("Task ready: %d\n", value);
			} else {
				printk("Task ready: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_PEND:
			thread = panic_trace_get_thread(value);
			if (thread == NULL) {
				printk("Task pend: %d\n", value);
			} else {
				printk("Task pend: %s\n",
					     k_thread_name_get(thread));
			}
			break;
		case PANIC_TRACE_TAG_TASK_SET_EVENT:
			if (extra_bytes_count == 1) {
				printk("Task Event: task=%d event=%x\n",
					     extra_bytes[0], 1 << value);
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_TICK:
			if (extra_bytes_count == 3) {
				uint32_t clock_value =
					panic_trace_get_val(extra_bytes, extra_bytes_count,
							    value);
				printk("Tick: %d\n", clock_value);
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_HOST_CMD:
			if (extra_bytes_count == 1) {
				uint16_t cmd =
					panic_trace_get_val(extra_bytes, extra_bytes_count, value);
				printk("Host Cmd: %d\n", cmd);
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_MUTEX_LOCK_ENTER:
			if (extra_bytes_count == 3) {
				mutex = (struct k_mutex *)(uintptr_t)
					panic_trace_get_val(extra_bytes, extra_bytes_count, value);
				printk("Mutex Lock Enter: %p\n", mutex);
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_MUTEX_LOCK_BLOCKING:
			if (extra_bytes_count == 3) {
				mutex = (struct k_mutex *)(uintptr_t)
					panic_trace_get_val(extra_bytes, extra_bytes_count, value);
				printk("Mutex Lock Blocking: %p\n", mutex);
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_MUTEX_LOCK_EXIT:
			if (extra_bytes_count == 3) {
				mutex = (struct k_mutex *)(uintptr_t)
					panic_trace_get_val(extra_bytes, extra_bytes_count, value);
				printk("Mutex Lock Exit: %p\n", mutex);
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_MUTEX_UNLOCK_ENTER:
			if (extra_bytes_count == 3) {
				mutex = (struct k_mutex *)(uintptr_t)
					panic_trace_get_val(extra_bytes, extra_bytes_count, value);
				printk("Mutex Unlock Enter: %p\n", mutex);
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_MUTEX_UNLOCK_EXIT:
			if (extra_bytes_count == 3) {
				mutex = (struct k_mutex *)(uintptr_t)
					panic_trace_get_val(extra_bytes, extra_bytes_count, value);
				printk("Mutex Unlock Exit: %p\n", mutex);
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_HOST_EVENT_SET:
			if (extra_bytes_count == 1) {
				printk("Host Event Set: %llx\n",
					     (uint64_t)1 << value);
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_HOST_EVENT_CLEAR:
			if (extra_bytes_count == 1) {
				printk("Host Event Clear: %llx\n",
					     (uint64_t)1 << value);
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_IDLE:
			printk("Idle\n");
			break;
		case PANIC_TRACE_TAG_WATCHDOG_RELOAD:
			printk("Watchdog Reload\n");
			break;
		case PANIC_TRACE_TAG_TASK_ENTRY:
			if (extra_bytes_count == 0) {
				printk("Task Entry: task=%d ", value);
			} else if (extra_bytes_count == 3) {
				uint32_t entry_addr =
					panic_trace_get_val(extra_bytes, extra_bytes_count, value);
				printk("addr=%p\n",
				       (void *)(uintptr_t)entry_addr);
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		case PANIC_TRACE_TAG_TASK_READY_BITMASK:
			if (extra_bytes_count == 3) {
				uint32_t mask =
					panic_trace_get_val(extra_bytes, extra_bytes_count, value);
				printk("Ready Mask: 0x%x\n", mask);
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		default:
			if (extra_bytes_count == 0) {
				printk("Unknown: tag=0x%x value=0x%x\n",
					     tag, value);
			} else {
				printk(
					"Unexpected extra bytes count: %d\n",
					extra_bytes_count);
			}
			break;
		}
		if (tag != PANIC_TRACE_TAG_EXTRA_BYTE) {
			extra_bytes_count = 0;
		}
		cflush();
		watchdog_reload();
	}
	panic_trace_freeze(orig_frozen);
}

test_export_static void panic_trace_corrupt(void)
{
	panic_trace->properties->checksum = -1;
}

static int command_panic_trace(int argc, const char **argv)
{
	if ((argc == 1) || (argc == 2 && !strcasecmp(argv[1], "info"))) {
		bool orig_frozen = panic_trace_freeze(true);
		printk("Valid: %d\n", panic_trace_is_valid());
		printk("Frozen: %d\n", orig_frozen);
		printk("Length: %d\n", panic_trace_len());
		printk("Capacity: %d\n", panic_trace_capacity());
		printk("Version: %d\n", panic_trace_version());
		for (int i = 0; i < 8; i++) {
			printk("Enable Mask[%d]: 0x%x\n", i, panic_trace_mask[i]);
		}
		panic_trace_dump();
		panic_trace_freeze(orig_frozen);
	} else if (argc == 2 && !strcasecmp(argv[1], "dump")) {
		panic_trace_dump();
	} else if (argc == 2 && !strcasecmp(argv[1], "reset")) {
		bool orig_frozen = panic_trace_freeze(true);
		panic_trace_reset();
		panic_trace_freeze(orig_frozen);
		printk("Panic trace reset, currently %s\n",
			 orig_frozen ? "frozen" : "unfrozen");
	} else if (argc == 2 && !strcasecmp(argv[1], "freeze")) {
		bool orig_frozen = panic_trace_freeze(true);
		if (orig_frozen) {
			printk("Panic trace already frozen\n");
		} else {
			printk("Panic trace frozen\n");
		}
	} else if (argc == 2 && !strcasecmp(argv[1], "unfreeze")) {
		bool orig_frozen = panic_trace_freeze(false);
		if (!orig_frozen) {
			printk("Panic trace already unfrozen\n");
		} else {
			printk("Panic trace unfrozen\n");
		}
	} else if (argc == 2 && !strcasecmp(argv[1], "corrupt")) {
		panic_trace_corrupt();
		printk("Panic trace corrupted\n");
	} else {
		return EC_ERROR_PARAM1;
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(panictrace, command_panic_trace,
			"[info | dump | reset | freeze | unfreeze | corrupt]",
			"Panic trace");

#endif /* CONFIG_PANIC_TRACE_DEBUG */
