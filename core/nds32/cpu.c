/* Copyright 2013 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Set up the N8 core
 */

#include "cpu.h"
#include "registers.h"
#include "console.h"
#include "system.h"
#include "timer.h"
#include "util.h"

void cpu_init(void)
{
	/* DLM initialization is done in init.S */
	/* Global interrupt enable */
	asm volatile("setgie.e");
}

static int execute_nop_4096_cnt(int argc, const char **argv)
{
	int time_delta;
	uint64_t time_start, time_stop;

	if (argc > 1) {
		if ((argv[1][0] == '.') && (strlen(argv[1]) == 2)) {
			switch (argv[1][1]) {
			case 'n':
				/* disable all interrupts */
				interrupt_disable();

				time_start = get_time().val;

				/* Loop to execute nop instruction for 4096 counts */
				__asm__ volatile(".rept 0x1000\n\t"
					     "nop\n\t"
					     ".endr\n\t");

				time_stop = get_time().val;
				time_delta = time_stop - time_start;

				/* enable all interrupts */
				interrupt_enable();

				ccprintf("time_delta %d(us)\n", time_delta);
				cflush();
				return EC_SUCCESS;
			default:
				return EC_ERROR_PARAM1;
			}
		}
	}

	return EC_ERROR_PARAM1;
}

DECLARE_CONSOLE_COMMAND(
	ex, execute_nop_4096_cnt, "[.n]",
	"Loop to execute nop instruction for 4096 counts");
