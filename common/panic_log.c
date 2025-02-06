/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "assert.h"
#include "console.h"
#include "link_defs.h"
#include "panic.h"
#include "panic_log.h"
#include "preserved_ring_buf.h"
#include "time.h"
#include "util.h"
#include "watchdog.h"

/* Update if the schema of the panic log changes */
#define PANIC_LOG_VERSION 1

DECLARE_PRESERVED_RING_BUF(uint8_t, panic_log, CONFIG_PANIC_LOG_LEN,
			   PANIC_LOG_VERSION);

/* Panic log defaults to frozen */
static bool frozen = true;

test_export_static bool panic_log_is_frozen(void)
{
	return frozen;
}

/* Returns the freeze state before applying the requested freeze state */
static bool panic_log_freeze(bool freeze)
{
	bool orig_frozen = frozen;
	frozen = freeze;
	return orig_frozen;
}

/* Default to frozen after reset */
static void panic_log_reset(void)
{
	frozen = true;
	preserved_ring_buf_reset(panic_log);
}

/* Check if panic data is fresh */
static bool panic_data_is_fresh(void)
{
	struct panic_data *pdata = panic_get_data();
	return !!pdata && !(pdata->flags & PANIC_DATA_FLAG_OLD_HOSTCMD);
}

void panic_log_init(void)
{
	if (!preserved_ring_buf_verify(panic_log)) {
		panic_printf("Panic log is invalid, resetting panic log\n");
		panic_log_reset();
	} else if (panic_data_is_fresh()) {
		panic_printf("New panic detected, freezing panic log\n");
		panic_log_freeze(true);
		return;
	} else {
		panic_printf("No new panic, unfreezing panic log\n");
	}
	panic_log_freeze(false);
}

void panic_log_write_char(const char c)
{
	if (frozen)
		return;
	preserved_ring_buf_write(panic_log, c);
}

void panic_log_write_str(const char *str, const size_t size)
{
	if (frozen)
		return;
	for (int i = 0; i < size; i++)
		preserved_ring_buf_write(panic_log, str[i]);
}

/* Returns the current state of panic log, before applying any requested changes
 */
static enum ec_status
host_command_get_panic_log_info(struct host_cmd_handler_args *args)
{
	struct ec_response_panic_log_info *r = args->response;
	const struct ec_params_panic_log_info *p = args->params;

	if (p->freeze && p->unfreeze)
		return EC_RES_INVALID_PARAM;

	/* Freeze before while reading */
	bool orig_frozen = panic_log_freeze(true);
	r->frozen = orig_frozen;
	r->version = preserved_ring_buf_version(panic_log);
	r->capacity = preserved_ring_buf_capacity(panic_log);
	r->valid = preserved_ring_buf_verify(panic_log);
	r->length = preserved_ring_buf_len(panic_log);
	args->response_size = sizeof(*r);

	if (p->reset)
		panic_log_reset();

	if (p->freeze)
		panic_log_freeze(true);
	else if (p->unfreeze)
		panic_log_freeze(false);
	else
		/* Restore original frozen state if no change requested */
		panic_log_freeze(orig_frozen);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PANIC_LOG_INFO, host_command_get_panic_log_info,
		     EC_VER_MASK(0));

static enum ec_status
host_command_read_panic_log(struct host_cmd_handler_args *args)
{
	const struct ec_params_panic_log_read *p = args->params;
	char *response = args->response;
	uint32_t offset = p->offset;

	if (offset >= preserved_ring_buf_len(panic_log))
		return EC_RES_INVALID_PARAM;

	while (offset < preserved_ring_buf_len(panic_log) &&
	       args->response_size < args->response_max - 1) {
		response[args->response_size++] =
			preserved_ring_buf_read(panic_log, offset++);
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PANIC_LOG_READ, host_command_read_panic_log,
		     EC_VER_MASK(0));

/**** PANIC LOG DEBUG UTILS ****/
#define PANIC_LOG_DEBUG
#ifdef PANIC_LOG_DEBUG

DECLARE_PRESERVED_RING_BUF(uint8_t, panic_log_8_debug, 16, 1);
void panic_log_write_uint8_measure(const uint8_t word)
{
	if (frozen)
		return;
	preserved_ring_buf_write(panic_log_8_debug, word);
}

DECLARE_PRESERVED_RING_BUF(uint16_t, panic_log_16_debug, 16, 1);
void panic_log_write_uint16_measure(const uint16_t word)
{
	if (frozen)
		return;
	preserved_ring_buf_write(panic_log_16_debug, word);
}

DECLARE_PRESERVED_RING_BUF(uint32_t, panic_log_32_debug, 16, 1);
void panic_log_write_uint32_measure(const uint32_t word)
{
	if (frozen)
		return;
	preserved_ring_buf_write(panic_log_32_debug, word);
}

void panic_log_write_str8_measure(const uint64_t word)
{
	char *str = (char *)&word;
	panic_log_write_str(str, 8);
}

void panic_log_write_str4_measure(const uint32_t word)
{
	char *str = (char *)&word;
	panic_log_write_str(str, 4);
}

/* Measure the microseconds per call of a given function.
 * Function must take a single argument */
#define MEASURE(func, arg, count, disable_irq)                            \
	do {                                                              \
		int key = 0;                                              \
		ccprintf("Func: %s Count: %d Interrupts On: %d\n", #func, \
			 count, !disable_irq);                            \
		/* Extra call to prevent compiler over optimization */    \
		func(arg);                                                \
		if (disable_irq) {                                        \
			key = irq_lock();                                 \
		}                                                         \
		timestamp_t start = get_time();                           \
		for (int i = 0; i < count; i++) {                         \
			func(arg);                                        \
		}                                                         \
		uint32_t elapsed = time_since32(start);                   \
		if (disable_irq) {                                        \
			irq_unlock(key);                                  \
		}                                                         \
		/* Extra call to prevent compiler over optimization */    \
		func(arg);                                                \
		ccprintf("\tElapsed: %d us, Avg: %d us/call\n", elapsed,  \
			 elapsed / count);                                \
	} while (0);

static int command_panic_log(int argc, const char **argv)
{
	if ((argc == 1) || (argc == 2 && !strcasecmp(argv[1], "info"))) {
		bool orig_frozen = panic_log_freeze(true);
		ccprintf("Valid: %d\n", preserved_ring_buf_verify(panic_log));
		ccprintf("Frozen: %d\n", orig_frozen);
		ccprintf("Length: %d\n", preserved_ring_buf_len(panic_log));
		ccprintf("Capacity: %d\n", panic_log->preserved->capacity);
		ccprintf("Version: %d\n", panic_log->preserved->version);
		ccprintf("Checksum: %d\n", panic_log->preserved->checksum);
		panic_log_freeze(orig_frozen);
	} else if (argc == 2 && !strcasecmp(argv[1], "dump")) {
		bool orig_frozen = panic_log_freeze(true);
		ccprintf("=== Panic Log Start ===\n");
		for (int i = 0; i < preserved_ring_buf_len(panic_log); i++) {
			char c = preserved_ring_buf_read(panic_log, i);
			if (c == 0)
				continue;
			ccprintf("%c", c);
			cflush();
			watchdog_reload();
		}
		panic_log_freeze(orig_frozen);
		ccprintf("=== Panic Log End ===\n");
	} else if (argc == 2 && !strcasecmp(argv[1], "reset")) {
		bool orig_frozen = panic_log_freeze(true);
		panic_log_reset();
		panic_log_freeze(orig_frozen);
		ccprintf("Panic log reset, currently %s\n",
			 orig_frozen ? "frozen" : "unfrozen");
	} else if (argc == 2 && !strcasecmp(argv[1], "freeze")) {
		bool orig_frozen = panic_log_freeze(true);
		if (orig_frozen) {
			ccprintf("Panic log already frozen\n");
		} else {
			ccprintf("Panic log frozen\n");
		}
	} else if (argc == 2 && !strcasecmp(argv[1], "unfreeze")) {
		bool orig_frozen = panic_log_freeze(false);
		if (!orig_frozen) {
			ccprintf("Panic log already unfrozen\n");
		} else {
			ccprintf("Panic log unfrozen\n");
		}
	} else if (argc == 2 && !strcasecmp(argv[1], "corrupt")) {
		panic_log->preserved->checksum = -1;
		ccprintf("Panic log corrupted\n");
	} else if (argc >= 2 && !strcasecmp(argv[1], "measure")) {
		bool orig_frozen = panic_log_freeze(false);
		char *e;
		int measure_count = panic_log->constants.capacity;
		bool disable_interrupts = true;
		if (argc >= 3) {
			measure_count = strtoi(argv[2], &e, 10);
		}
		MEASURE(panic_log_write_uint8_measure, 0x12, measure_count,
			disable_interrupts);
		MEASURE(panic_log_write_uint16_measure, 0x1234, measure_count,
			disable_interrupts);
		MEASURE(panic_log_write_uint32_measure, 0x12345678,
			measure_count, disable_interrupts);
		MEASURE(panic_log_write_str8_measure, 0x1234567812345678,
			measure_count, disable_interrupts);
		MEASURE(panic_log_write_str4_measure, 0x12345678, measure_count,
			disable_interrupts);
		panic_log_freeze(orig_frozen);
	} else {
		return EC_ERROR_PARAM1;
	}
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(
	paniclog, command_panic_log,
	"[info | dump | reset | freeze | unfreeze | corrupt | measure]",
	"Panic log");
#endif
