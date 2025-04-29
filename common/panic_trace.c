/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "panic.h"
#include "panic_trace.h"
#include "preserved_ring_buf.h"
#include "time.h"
#include "util.h"
#include "watchdog.h"

typedef union {
	struct {
		uint8_t tag;
		uint8_t value;
	} entry;
	uint16_t val;
} panic_trace_entry_t;

/* Panic trace defaults to frozen */
test_export_static bool panic_trace_frozen = true;
test_export_static bool panic_trace_initialized = false;
test_export_static uint8_t panic_trace_mask[PANIC_TRACE_TAG_COUNT] = {
	[PANIC_TRACE_TAG_BYTE0] = 1,
	[PANIC_TRACE_TAG_BYTE1] = 1,
	[PANIC_TRACE_TAG_BYTE2] = 1,
	[PANIC_TRACE_TAG_BYTE3] = 1,
	[PANIC_TRACE_TAG_BYTE4] = 1,
	[PANIC_TRACE_TAG_BYTE5] = 1,
	[PANIC_TRACE_TAG_BYTE6] = 1,
	[PANIC_TRACE_TAG_IRQ_START] = 1,
	[PANIC_TRACE_TAG_IRQ_END] = 1,
	[PANIC_TRACE_TAG_SVC_CALL] = 1,
	[PANIC_TRACE_TAG_TASK_SWITCH] = 1,
	[PANIC_TRACE_TAG_TASK_SET_EVENT] = 1,
	[PANIC_TRACE_TAG_TICK] = 1,
	[PANIC_TRACE_TAG_HOST_CMD] = 1,
	[PANIC_TRACE_TAG_HOST_EVENT_SET] = 1,
	[PANIC_TRACE_TAG_HOST_EVENT_CLEAR] = 1,
	[PANIC_TRACE_TAG_MUTEX_LOCK] = 1,
	[PANIC_TRACE_TAG_MUTEX_UNLOCK] = 1,
};

/* Update if the schema of the panic trace changes */
#define PANIC_TRACE_VERSION 1

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

	if (!panic_trace_is_valid()) {
		if (IS_ENABLED(CONFIG_panic_trace_DEBUG))
			ccprintf(
				"Panic trace is invalid, resetting and unfreezing panic trace\n");
		panic_trace_reset();
		freeze = false;
	} else if (panic_data_is_new()) {
		if (IS_ENABLED(CONFIG_panic_trace_DEBUG))
			ccprintf(
				"New panic detected, keeping panic trace frozen\n");
		freeze = true;
	} else {
		if (IS_ENABLED(CONFIG_panic_trace_DEBUG))
			ccprintf("No new panic, unfreezing panic trace\n");
		freeze = false;
	}
	panic_trace_freeze(freeze);
}
DECLARE_HOOK(HOOK_INIT_EARLY, panic_trace_init, HOOK_PRIO_DEFAULT - 1);

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
	preserved_ring_buf_write(panic_trace, entry.val);
	panic_trace_atomic_end(key);
}

void panic_trace_write_2(uint8_t tag, uint8_t value0, uint8_t value1)
{
	panic_trace_entry_t entries[] = { {
						  .entry.tag =
							  PANIC_TRACE_TAG_BYTE0,
						  .entry.value = value0,
					  },
					  {
						  .entry.tag = tag,
						  .entry.value = value1,
					  } };
	if (panic_trace_frozen || !panic_trace_mask[tag])
		return;
	int key = panic_trace_atomic_begin();
	preserved_ring_buf_write(panic_trace, entries[0].val);
	preserved_ring_buf_write(panic_trace, entries[1].val);
	panic_trace_atomic_end(key);
}

void panic_trace_write_3(uint8_t tag, uint8_t value0, uint8_t value1,
			 uint8_t value2)
{
	panic_trace_entry_t entries[] = {
		{
			.entry.tag = PANIC_TRACE_TAG_BYTE0,
			.entry.value = value0,
		},
		{
			.entry.tag = PANIC_TRACE_TAG_BYTE1,
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
			.entry.tag = PANIC_TRACE_TAG_BYTE0,
			.entry.value = value0,
		},
		{
			.entry.tag = PANIC_TRACE_TAG_BYTE1,
			.entry.value = value1,
		},
		{
			.entry.tag = PANIC_TRACE_TAG_BYTE2,
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

/**
 * Periodically adds a ms timestamp to the panic trace.
 */
static void panic_trace_tick(void)
{
	panic_trace_write_uint32(PANIC_TRACE_TAG_TICK, clock());
}
DECLARE_HOOK(HOOK_TICK, panic_trace_tick, HOOK_PRIO_DEFAULT);

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
		panic_trace_freeze(orig_frozen);
	} else if (argc == 2 && !strcasecmp(argv[1], "dump")) {
		uint8_t extra_bytes[8] = { 0 };
		bool orig_frozen = panic_trace_freeze(true);
		panic_printf("=== Panic Trace Start ===\n");
		for (int i = 0; i < panic_trace_len(); i++) {
			panic_trace_entry_t entry;
			entry.val = panic_trace_read(i);
			uint8_t value = entry.entry.value;
			uint8_t tag = entry.entry.tag;
			switch (tag) {
			case PANIC_TRACE_TAG_NULL:
				ccprintf("Null Tag\n");
				break;
			case PANIC_TRACE_TAG_BYTE0:
				extra_bytes[0] = value;
				break;
			case PANIC_TRACE_TAG_BYTE1:
				extra_bytes[1] = value;
				break;
			case PANIC_TRACE_TAG_BYTE2:
				extra_bytes[2] = value;
				break;
			case PANIC_TRACE_TAG_BYTE3:
				extra_bytes[3] = value;
				break;
			case PANIC_TRACE_TAG_BYTE4:
				extra_bytes[4] = value;
				break;
			case PANIC_TRACE_TAG_BYTE5:
				extra_bytes[5] = value;
				break;
			case PANIC_TRACE_TAG_BYTE6:
				extra_bytes[6] = value;
				break;
			case PANIC_TRACE_TAG_IRQ_START:
				ccprintf("IRQ Start: %d\n", value);
				break;
			case PANIC_TRACE_TAG_IRQ_END:
				ccprintf("IRQ End: %d\n", value);
				break;
			case PANIC_TRACE_TAG_SVC_CALL:
				ccprintf("Service Call\n");
				break;
			case PANIC_TRACE_TAG_TASK_SWITCH:
				ccprintf("Task switch: %d\n", value);
				break;
			case PANIC_TRACE_TAG_TASK_SET_EVENT:
				ccprintf("Task Event: task=%d event=%x\n",
					 extra_bytes[0], 1 << value);
				break;
			case PANIC_TRACE_TAG_TICK:
				uint32_t clock_value = extra_bytes[0] << 24 |
						       extra_bytes[1] << 16 |
						       extra_bytes[2] << 8 |
						       value;
				ccprintf("Tick: %d\n", clock_value);
				break;
			case PANIC_TRACE_TAG_HOST_CMD:
				uint16_t cmd = extra_bytes[0] << 8 | value;
				ccprintf("Host Cmd: %d\n", cmd);
				break;
			case PANIC_TRACE_TAG_MUTEX_LOCK:
				ccprintf("Mutex Lock: %d\n", value);
				break;
			case PANIC_TRACE_TAG_MUTEX_UNLOCK:
				ccprintf("Mutex Unlock: %d\n", value);
				break;
			case PANIC_TRACE_TAG_HOST_EVENT_SET:
				ccprintf("Host Event Set: %llx\n",
					 (uint64_t)1 << value);
				break;
			case PANIC_TRACE_TAG_HOST_EVENT_CLEAR:
				ccprintf("Host Event Clear: %llx\n",
					 (uint64_t)1 << value);
				break;
			default:
				ccprintf("Unknown: tag=0x%x value=0x%x\n", tag,
					 value);
				break;
			}
			cflush();
			watchdog_reload();
		}
		panic_trace_freeze(orig_frozen);
		panic_printf("\n=== Panic Trace End ===\n");
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
