/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Kukui SCP configuration */

#include "common.h"
#include "console.h"
#include "cpu.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "link_defs.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Build GPIO tables */
void eint_event(enum gpio_signal signal);

#include "gpio_list.h"


void eint_event(enum gpio_signal signal)
{
	ccprintf("EINT event: %d\n", signal);
}

/* Initialize board.  */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_EINT5_TP);
	gpio_enable_interrupt(GPIO_EINT6_TP);
	gpio_enable_interrupt(GPIO_EINT7_TP);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

static int command_dcachetest(int argc, char **argv)
{
	uint32_t *cached = (void *)0x10000000;
	uint32_t *direct = (void *)0x30000000;
	int i, j;
	const int it = 1000;
	const int len = 0x100;
	timestamp_t start;
	uint32_t val;

	start = get_time();
	val = 0;
	for (i = 0; i < it; i++) {
		for (j = 0; j < len; j++)
			val += cached[j];
	}
	ccprintf("cached: %d us (val: %x)\n", time_since32(start), val);
	cflush();

	start = get_time();
	val = 0;
	for (i = 0; i < it; i++) {
		cpu_invalidate_dcache();
		for (j = 0; j < len; j++)
			val += cached[j];
	}
	ccprintf("cached+inval: %d us (val: %x)\n", time_since32(start), val);
	cflush();

	start = get_time();
	val = 0;
	for (i = 0; i < it; i++) {
		for (j = 0; j < len; j++)
			val += direct[j];
	}
	ccprintf("direct: %d us (val: %x)\n", time_since32(start), val);
	cflush();

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(dcachetest, command_dcachetest,
			     NULL,
			     "Do D-cache performance test");

static inline uint64_t perf_test(void)
{
	volatile int x = 0;
	int i, it;
	uint64_t total;

	for (it = 0; it < 10; it++) {
		total = it;
		for (i = 0; i < 100000; i++) {
			total += i;
			x = !x;
		}
	}

	return total;
}

__SECTION(dram) static int command_icachetest(int argc, char **argv)
{
	uint64_t total;
	timestamp_t start;

	start = get_time();
	total = perf_test();

	ccprintf("run from DRAM: %d us (total: %lx)\n",
		time_since32(start), total);
	cflush();

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(icachetest, command_icachetest,
			     NULL,
			     "Do I-cache performance test");

static int command_perftest(int argc, char **argv)
{
	uint64_t total;
	timestamp_t start;

	start = get_time();
	total = perf_test();

	ccprintf("run from SRAM: %d us (total: %lx)\n",
		time_since32(start), total);
	cflush();

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(perftest, command_perftest,
			     NULL,
			     "Do I-cache performance test");
