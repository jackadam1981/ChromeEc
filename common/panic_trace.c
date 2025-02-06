/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "assert.h"
#include "console.h"
#include "link_defs.h"
#include "panic_trace.h"
#include "panic.h"
#include "power.h"
#include "preserved_ram_buf.h"
#include "time.h"
#include "util.h"
#include "watchdog.h"

#define PANIC_TRACE_DEBUG

DEFINE_PRESERVED_RAM_RING_BUF(uint16_t);
DECLARE_PRESERVED_RAM_RING_BUF(panic_trace, uint16_t, CONFIG_PANIC_TRACE_LEN, 1);

typedef union {
    struct {
        uint8_t tag;
        uint8_t value;
    } entry;
    uint16_t val;
} panic_trace_entry_t;

#ifdef SECTION_IS_RO
#error Panic trace should not be enabled in RO images.
#endif

static int frozen = true;

static bool panic_data_is_fresh(void)
{
	struct panic_data *pdata = panic_get_data();
	return !!pdata && !(pdata->flags & PANIC_DATA_FLAG_OLD_HOSTCMD);
}

void panic_trace_init(void)
{
	if (!preserved_ram_ring_buf_verify_uint16_t(panic_trace)) {
		ccprintf("Panic trace is invalid, resetting\n");
		preserved_ram_ring_buf_reset_uint16_t(panic_trace);
	} else if (panic_data_is_fresh()) {
		ccprintf("New panic detected, freezing panic trace\n");
		frozen = true;
		return;
	} else {
		ccprintf("No new panic, unfreezing panic trace\n");
	}
	frozen = false;
}

static inline int panic_trace_atomic_begin(void)
{
	return irq_lock();
}

static inline void panic_trace_atomic_end(int key)
{
	irq_unlock(key);
}

/**
 * @brief Freezes or unfreezes the panic trace buffer atomically.
 *
 * When the buffer is frozen, new entries are discarded. This is used to
 * preserve the trace after a panic.
 *
 * @param freeze True to freeze the buffer, false to unfreeze it.
 * @return True if the buffer was previously frozen, false otherwise.
 */
static inline bool panic_trace_freeze(bool freeze)
{
	int key = panic_trace_atomic_begin();
	int cur_frozen = frozen;
	frozen = freeze;
	panic_trace_atomic_end(key);
	return cur_frozen;
}

/**
 * @brief Adds a panic trace entry with only a tag.
 *
 * This function adds a panic trace entry to the panic trace buffer. The entry
 * consists of a tag. The tag identifies the type of event that
 * is being traced.
 *
 * @param tag The tag of the panic trace entry.
 */
void panic_trace_add_0(uint8_t tag)
{
	panic_trace_add_1(tag, 0);
}

/**
 * @brief Adds a panic trace entry with a tag and a value.
 *
 * This function adds a panic trace entry to the panic trace buffer. The entry
 * consists of a tag and a value. The tag identifies the type of event that
 * is being traced, and the value provides additional information about the
 * event.
 *
 * @param tag The tag of the panic trace entry.
 * @param value The value of the panic trace entry.
 */

void panic_trace_add_1(uint8_t tag, uint8_t value)
{
	panic_trace_entry_t entry = {
		.entry.tag = tag,
		.entry.value = value,
	};
	int key = panic_trace_atomic_begin();
	if (!frozen)
		preserved_ram_ring_buf_write_uint16_t(panic_trace, entry.val);
	panic_trace_atomic_end(key);
}

void panic_trace_add_2(uint8_t tag, uint8_t value0, uint8_t value1)
{
	panic_trace_entry_t entries[] = {
        {
            .entry.tag = PANIC_TRACE_TAG_BYTE0,
            .entry.value = value0,
        },
        {
            .entry.tag = tag,
            .entry.value = value1,
        }
    };
	int key = panic_trace_atomic_begin();
	if (!frozen) {
		preserved_ram_ring_buf_write_uint16_t(panic_trace,
						      entries[0].val);
		preserved_ram_ring_buf_write_uint16_t(panic_trace,
						      entries[1].val);
	}
	panic_trace_atomic_end(key);
}

void panic_trace_add_3(uint8_t tag, uint8_t value0, uint8_t value1,
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
	int key = panic_trace_atomic_begin();
	if (!frozen) {
		preserved_ram_ring_buf_write_uint16_t(panic_trace,
						      entries[0].val);
		preserved_ram_ring_buf_write_uint16_t(panic_trace,
						      entries[1].val);
		preserved_ram_ring_buf_write_uint16_t(panic_trace,
						      entries[2].val);
	}
	panic_trace_atomic_end(key);
}

void panic_trace_add_4(uint8_t tag, uint8_t value0, uint8_t value1,
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
		{ .entry.tag = tag, .entry.value = value3 }
	};
	int key = panic_trace_atomic_begin();
	if (!frozen) {
		preserved_ram_ring_buf_write_uint16_t(panic_trace,
						      entries[0].val);
		preserved_ram_ring_buf_write_uint16_t(panic_trace,
						      entries[1].val);
		preserved_ram_ring_buf_write_uint16_t(panic_trace,
						      entries[2].val);
		preserved_ram_ring_buf_write_uint16_t(panic_trace,
						      entries[3].val);
	}
	panic_trace_atomic_end(key);
}

void panic_trace_add_uint8_t(uint8_t tag, uint8_t value)
{
	panic_trace_entry_t entry = {
		.entry.tag = tag,
		.entry.value = value,
	};
	int key = panic_trace_atomic_begin();
	if (!frozen)
		preserved_ram_ring_buf_write_uint16_t(panic_trace, entry.val);
	panic_trace_atomic_end(key);
}

void panic_trace_add_uint16_t(uint8_t tag, uint16_t value)
{
	panic_trace_add_2(tag, value >> 8, value & 0xff);
}

void panic_trace_add_uint32_t(uint8_t tag, uint32_t value)
{
	panic_trace_add_4(tag, value >> 24, value >> 16, value >> 8,
			  value & 0xff);
}

/**
 * Periodically adds a ms timestamp to the panic trace.
 */
static void panic_trace_tick(void)
{
	panic_trace_add_uint32_t(PANIC_TRACE_TAG_TICK, clock());
}
DECLARE_HOOK(HOOK_TICK, panic_trace_tick, HOOK_PRIO_DEFAULT);

#ifdef PANIC_TRACE_DEBUG

/***************** DEBUG UTILS **************************/

void panic_trace_tick_measure(uint8_t unused)
{
	panic_trace_tick();
}

void panic_trace_add_uint8_t_measure(uint8_t value)
{
	panic_trace_add_uint8_t(PANIC_TRACE_TAG_NULL, value);
}

void panic_trace_add_uint16_t_measure(uint16_t value)
{
	panic_trace_add_uint16_t(PANIC_TRACE_TAG_NULL, value);
}

void panic_trace_add_uint32_t_measure(uint32_t value)
{
	panic_trace_add_uint32_t(PANIC_TRACE_TAG_NULL, value);
}

void panic_trace_add_0_measure(uint8_t unused)
{
	panic_trace_add_0(PANIC_TRACE_TAG_NULL);
}

void panic_trace_add_1_measure(uint8_t value)
{
	panic_trace_add_1(PANIC_TRACE_TAG_NULL, value);
}

void panic_trace_add_2_meaasure(uint8_t value)
{
	panic_trace_add_2(PANIC_TRACE_TAG_NULL, value, value);
}

void panic_trace_add_3_measure(uint8_t value)
{
	panic_trace_add_3(PANIC_TRACE_TAG_NULL, value, value, value);
}

void panic_trace_add_4_measure(uint8_t value)
{
	panic_trace_add_4(PANIC_TRACE_TAG_NULL, value, value, value, value);
}

#define MEASURE(func, arg, count, disable_irq)                                \
	do {                                                                  \
		int key;                                                      \
		ccprintf("Func: %s Count: %d Interrupts: %d\n", #func, count, \
			 !disable_irq);                                       \
		if (disable_irq) {                                            \
			key = irq_lock();                                     \
		}                                                             \
		timestamp_t start = get_time();                               \
		for (int i = 0; i < count; i++) {                             \
			func(arg);                                            \
		}                                                             \
		uint32_t elapsed = time_since32(start);                       \
		if (disable_irq) {                                            \
			irq_unlock(key);                                      \
		}                                                             \
		ccprintf("\tElapsed: %d us, Avg: %d us/op\n", elapsed,        \
			 elapsed / count);                                    \
	} while (0);

static int command_panic_trace(int argc, const char **argv)
{
	if (argc == 1) {
		bool cur_frozen = panic_trace_freeze(true);
		uint8_t extra_bytes[8] = { 0 };
		ccprintf("vvvvvvvvvvvvv PANIC TRACE vvvvvvvvvvvvv\n");
		for (int i = 0;
		     i < preserved_ram_ring_buf_len_uint16_t(panic_trace);
		     i++) {
			panic_trace_entry_t entry;
			entry.val = preserved_ram_ring_buf_read_uint16_t(
				panic_trace, i);
			uint8_t value = entry.entry.value;
			uint8_t tag = entry.entry.tag;
			switch (tag) {
			case PANIC_TRACE_TAG_NULL:
				ccprintf("Null tag\n");
				break;
			case PANIC_TRACE_TAG_IRQ:
				ccprintf("IRQ: %d\n", value);
				break;
			case PANIC_TRACE_TAG_TASK_SWITCH:
				ccprintf("Task switch: %d\n", value);
				break;
			case PANIC_TRACE_TAG_TASK_SET_EVENT:
				ccprintf("Task set event: task=%d event=%x\n",
					 extra_bytes[0], 1 << value);
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
			case PANIC_TRACE_TAG_TICK:
				uint32_t clock_value = extra_bytes[0] << 24 |
						       extra_bytes[1] << 16 |
						       extra_bytes[2] << 8 |
						       value;
				ccprintf("Tick: %d\n", clock_value);
				break;
            case PANIC_TRACE_TAG_HOST_CMD:
                uint16_t cmd = extra_bytes[0] << 8 | value;
                ccprintf("Host cmd: %d\n", cmd);
                break;
            case PANIC_TRACE_TAG_MUTEX_LOCK:
                ccprintf("Mutex lock: %d\n", value);
                break;
            case PANIC_TRACE_TAG_MUTEX_UNLOCK:
                ccprintf("Mutex unlock: %d\n", value);
                break;
			default:
				ccprintf("Unknown: tag=0x%x value=0x%x\n", tag,
					 value);
				break;
			}
			cflush();
			watchdog_reload();
		}
		ccprintf("\n^^^^^^^^^^^^^ PANIC TRACE  ^^^^^^^^^^^^^\n");
		ccprintf("Valid: %d\n",
			 preserved_ram_ring_buf_verify_uint16_t(panic_trace));
		ccprintf("Frozen: %d\n", cur_frozen);
		ccprintf("Head: %d\n", panic_trace->noinit->head);
		ccprintf("Checksum: %d\n", panic_trace->noinit->checksum);
		ccprintf("Length: %d\n",
			 preserved_ram_ring_buf_len_uint16_t(panic_trace));
		ccprintf("Buffer Size: %d\n", panic_trace->noinit->buffer_size);
		ccprintf("Version: %d\n", panic_trace->noinit->version);
		ccprintf("Calc Checksum: %d\n",
			 preserved_ram_ring_buf_calc_checksum_uint16_t(
				 panic_trace));
		panic_trace_freeze(cur_frozen);
	} else if (argc >= 2) {
		if (!strcasecmp(argv[1], "reset")) {
			panic_trace_freeze(true);
			preserved_ram_ring_buf_reset_uint16_t(panic_trace);
			panic_trace_freeze(false);
		} else if (!strcasecmp(argv[1], "freeze")) {
			panic_trace_freeze(true);
		} else if (!strcasecmp(argv[1], "unfreeze")) {
			panic_trace_freeze(false);
		} else if (!strcasecmp(argv[1], "corrupt")) {
			panic_trace->noinit->checksum = -1;
		} else if (!strcasecmp(argv[1], "measure")) {
			char *e;
			int measure_count = panic_trace->noinit->buffer_size;
			if (argc == 3) {
				measure_count = strtoi(argv[2], &e, 10);
			}
			bool disable_interrupts = true;
			MEASURE(panic_trace_add_uint8_t_measure, 0x12,
				measure_count, disable_interrupts);
			MEASURE(panic_trace_add_uint16_t_measure, 0x1234,
				measure_count, disable_interrupts);
			MEASURE(panic_trace_add_uint32_t_measure, 0x12346789,
				measure_count, disable_interrupts);
			MEASURE(panic_trace_add_0_measure, 0x12, measure_count,
				disable_interrupts);
			MEASURE(panic_trace_add_1_measure, 0x12, measure_count,
				disable_interrupts);
			MEASURE(panic_trace_add_2_meaasure, 0x12, measure_count,
				disable_interrupts);
			MEASURE(panic_trace_add_3_measure, 0x12, measure_count,
				disable_interrupts);
			MEASURE(panic_trace_add_4_measure, 0x12, measure_count,
				disable_interrupts);
			MEASURE(panic_trace_tick_measure, 0x12, measure_count,
				disable_interrupts);

		} else {
			return EC_ERROR_PARAM1;
		}
	} else {
		return EC_ERROR_PARAM_COUNT;
	}

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(panictrace, command_panic_trace,
			"[reset | freeze | unfreeze | corrupt | measure]",
			"Panic Trace");

#endif
