/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "assert.h"
#include "console.h"
#include "link_defs.h"
#include "panic_log.h"
#include "panic.h"
#include "preserved_ram_buf.h"
#include "time.h"
#include "util.h"
#include "watchdog.h"

#define PANIC_LOG_DEBUG

DEFINE_PRESERVED_RAM_RING_BUF(char);
DECLARE_PRESERVED_RAM_RING_BUF(panic_log, char, CONFIG_PANIC_LOG_LEN, 1);

static int frozen = true;

static bool panic_data_is_fresh(void)
{
    struct panic_data *pdata = panic_get_data();
    return !!pdata && !(pdata->flags & PANIC_DATA_FLAG_OLD_HOSTCMD);
}

void panic_log_init(void)
{
    if (!preserved_ram_ring_buf_verify_char(panic_log))
    {
        ccprintf("Panic log is invalid, resetting panic log\n");
        preserved_ram_ring_buf_reset_char(panic_log);
    }
    else if (panic_data_is_fresh()) {
        ccprintf("New panic detected, freezing panic log\n");
        frozen = true;
        return;
    }
    else {
        ccprintf("No new panic, unfreezing panic log\n");
    }
    frozen = false;
}

void panic_log_write_char(const char c)
{
    if (frozen) {
        return;
    }
    preserved_ram_ring_buf_write_char(panic_log, c);
}

#ifdef PANIC_LOG_DEBUG

DEFINE_PRESERVED_RAM_RING_BUF(uint16_t);
DECLARE_PRESERVED_RAM_RING_BUF(panic_log_16, uint16_t, 16, 1);

void panic_log_write_uint16(const uint16_t word)
{
    if (frozen) {
        return;
    }
    preserved_ram_ring_buf_write_uint16_t(panic_log_16, word);

}
DEFINE_PRESERVED_RAM_RING_BUF(uint32_t);
DECLARE_PRESERVED_RAM_RING_BUF(panic_log_32, uint32_t, 16, 1);

void panic_log_write_uint32(const uint32_t word)
{
    if (frozen) {
        return;
    }
    preserved_ram_ring_buf_write_uint32_t(panic_log_32, word);
}

DEFINE_PRESERVED_RAM_RING_BUF(uint64_t);
DECLARE_PRESERVED_RAM_RING_BUF(panic_log_64, uint64_t, 16, 1);

void panic_log_write_uint64(const uint64_t word)
{
    if (frozen)
        return;
    preserved_ram_ring_buf_write_uint64_t(panic_log_64, word);
}

#define MEASURE(func, arg, count, disable_irq) \
    do { \
        int key; \
        ccprintf("Func: %s Count: %d Interrupts: %d\n", #func, count, !disable_irq); \
        if (disable_irq) { \
            key = irq_lock(); \
        } \
        timestamp_t start = get_time(); \
        for(int i=0; i<count; i++) { \
            func(arg); \
        } \
        uint32_t elapsed = time_since32(start); \
        if (disable_irq) { \
            irq_unlock(key); \
        } \
        ccprintf("\tElapsed: %d us, Avg: %d op/us\n", elapsed, elapsed/count); \
    } while(0);

static int command_panic_log(int argc, const char **argv)
{
	if (argc == 1) {

        int cur_frozen = frozen;
        frozen = true;
        ccprintf("vvvvvvvvvvvvv PANIC LOG vvvvvvvvvvvvv\n");
        for (int i=0; i < preserved_ram_ring_buf_len_char(panic_log); i++) {
            char c = preserved_ram_ring_buf_read_char(panic_log, i);
            if (c == 0) 
                continue;
            ccprintf("%c", c);
            cflush();
            watchdog_reload();
        }
        ccprintf("\n^^^^^^^^^^^^^ PANIC LOG  ^^^^^^^^^^^^^\n");
        ccprintf("Valid: %d\n", preserved_ram_ring_buf_verify_char(panic_log));
        ccprintf("Frozen: %d\n", cur_frozen);
        ccprintf("Head: %d\n", panic_log->noinit->head);
        ccprintf("Checksum: %d\n", panic_log->noinit->checksum);
        ccprintf("Length: %d\n", preserved_ram_ring_buf_len_char(panic_log));
        ccprintf("Buffer Size: %d\n", panic_log->noinit->buffer_size);
        ccprintf("Version: %d\n", panic_log->noinit->version);
        ccprintf("Calc Checksum: %d\n", preserved_ram_ring_buf_calc_checksum_char(panic_log));
        frozen = cur_frozen;

    } else if (argc >= 2) {
        if (!strcasecmp(argv[1], "reset")) {
            frozen = true;
            preserved_ram_ring_buf_reset_char(panic_log);
            frozen = false;
        } else if (!strcasecmp(argv[1], "freeze")) {
            frozen = true;
        } else if (!strcasecmp(argv[1], "unfreeze")) {
            frozen = false;
        } else if (!strcasecmp(argv[1], "corrupt")) {
            panic_log->noinit->checksum = -1;
        } else if (!strcasecmp(argv[1], "measure")) {
            char *e;
            int measure_count = CONFIG_PANIC_LOG_LEN;
            if (argc == 3) {
                measure_count = strtoi(argv[2], &e, 10);
            }
            bool disable_interrupts = true;
            MEASURE(panic_log_write_char, 'x', measure_count, disable_interrupts);
            MEASURE(panic_log_write_char, 'x', measure_count, disable_interrupts);
            MEASURE(panic_log_write_uint16, 0x1234, measure_count, disable_interrupts);
            MEASURE(panic_log_write_uint32, 0x12345678, measure_count, disable_interrupts);
            MEASURE(panic_log_write_uint64, 0x1234567812345678, measure_count, disable_interrupts);

        } else {
            return EC_ERROR_PARAM1;
        }
    } else {
        return EC_ERROR_PARAM_COUNT;
    }

	/* Everything crashes, so shouldn't get back here */
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(paniclog, command_panic_log,
			"[reset | freeze | unfreeze | corrupt | measure]",
			"Panic log"
);
#endif
