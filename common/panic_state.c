/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "assert.h"
#include "console.h"
#include "link_defs.h"
#include "panic.h"
#include "panic_state.h"
#include "preserved_ram_buf.h"
#include "time.h"
#include "util.h"
#include "watchdog.h"

#define PANIC_STATE_DEBUG

#define CONFIG_PANIC_STATE_LEN 32

BUILD_ASSERT(CONFIG_PANIC_STATE_LEN >= PANIC_STATE_TAG_COUNT);

DEFINE_PRESERVED_RAM_BUF(uint32_t);
DECLARE_PRESERVED_RAM_BUF(panic_state, uint32_t, CONFIG_PANIC_STATE_LEN, 1);

static bool frozen = true;

static bool panic_data_is_fresh(void)
{
	struct panic_data *pdata = panic_get_data();
	return !!pdata && !(pdata->flags & PANIC_DATA_FLAG_OLD_HOSTCMD);
}

void panic_state_init(void)
{
	if (!preserved_ram_buf_verify_uint32_t(panic_state)) {
		ccprintf("Panic state is invalid, resetting panic state\n");
		preserved_ram_buf_reset_uint32_t(panic_state);
	} else if (panic_data_is_fresh()) {
		ccprintf("New panic detected, freezing panic state\n");
		frozen = true;
		return;
	} else {
		ccprintf("No new panic, unfreezing panic state\n");
	}
	frozen = false;
}

static inline bool panic_state_freeze(bool freeze)
{
	bool cur_freeze = frozen;
	frozen = freeze;
	return cur_freeze;
}

void panic_state_update(enum panic_state_tag_t tag, uint32_t value)
{
	if (frozen)
		return;
	preserved_ram_buf_write_uint32_t(panic_state, (uint32_t)tag, value);
}

static void panic_state_tick(void)
{
	PANIC_STATE_UPDATE(TICK, clock());
}
DECLARE_HOOK(HOOK_TICK, panic_state_tick, HOOK_PRIO_DEFAULT);

#ifdef PANIC_STATE_DEBUG

void panic_state_update_measure(uint32_t value)
{
	panic_state_update(PANIC_STATE_TAG_NULL, value);
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
		ccprintf("\tElapsed: %d us, Avg: %d op/us\n", elapsed,        \
			 count / elapsed);                                    \
	} while (0);

static int command_panic_state(int argc, const char **argv)
{
	if (argc == 1) {
		int cur_frozen = panic_state_freeze(true);
		frozen = true;
		ccprintf("vvvvvvvvvvvvv PANIC STATE vvvvvvvvvvvvv\n");
		for (int i = 0; i < CONFIG_PANIC_STATE_LEN; i++) {
			uint32_t value =
				preserved_ram_buf_read_uint32_t(panic_state, i);
			ccprintf("%d: %x\n", i, value);
			cflush();
			watchdog_reload();
		}
		ccprintf("\n^^^^^^^^^^^^^ PANIC STATE  ^^^^^^^^^^^^^\n");
		ccprintf("Valid: %d\n",
			 preserved_ram_buf_verify_uint32_t(panic_state));
		ccprintf("Frozen: %d\n", cur_frozen);
		ccprintf("Checksum: %d\n", panic_state->noinit->checksum);
		ccprintf("Buffer Size: %d\n", panic_state->noinit->buffer_size);
		ccprintf("Version: %d\n", panic_state->noinit->version);
		ccprintf("Calc Checksum: %d\n",
			 preserved_ram_buf_calc_checksum_uint32_t(panic_state));
		panic_state_freeze(cur_frozen);

	} else if (argc >= 2) {
		if (!strcasecmp(argv[1], "reset")) {
			panic_state_freeze(true);
			preserved_ram_buf_reset_uint32_t(panic_state);
			panic_state_freeze(false);
		} else if (!strcasecmp(argv[1], "freeze")) {
			panic_state_freeze(true);
		} else if (!strcasecmp(argv[1], "unfreeze")) {
			panic_state_freeze(false);
		} else if (!strcasecmp(argv[1], "corrupt")) {
			panic_state->noinit->checksum = -1;
		} else if (!strcasecmp(argv[1], "measure")) {
			char *e;
			int measure_count = panic_state->noinit->buffer_size;
			if (argc == 3) {
				measure_count = strtoi(argv[2], &e, 10);
			}
			bool disable_interrupts = true;
			MEASURE(panic_state_update_measure, 0x12345678,
				measure_count, disable_interrupts);

		} else {
			return EC_ERROR_PARAM1;
		}
	} else {
		return EC_ERROR_PARAM_COUNT;
	}

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(panicstate, command_panic_state,
			"[reset | freeze | unfreeze | corrupt | measure]",
			"Panic State");

#endif /* PANIC_STATE_DEBUG */
