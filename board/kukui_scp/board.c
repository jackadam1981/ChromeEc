/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Kukui SCP configuration */

#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "link_defs.h"

/* Build GPIO tables */
void eint_event(enum gpio_signal signal);

#include "gpio_list.h"

__SECTION(dram.rodata) const int dram_rodata_val[] = {55, 56, 57, 58, 59, 60};
__SECTION(dram.bss) unsigned char dram_bss_val;
__SECTION(dram.data)
unsigned char dram_data_val[] = {1, 2, 3, 4, 5, 6, 7, 8, 9,
				 10, 11, 12, 13, 14, 15, 16, 17};

void eint_event(enum gpio_signal signal)
{
	ccprintf("EINT event: %d\n", signal);
}

__SECTION(dram.text) __attribute__((noinline)) void dram_text_func(void) {
	int i;

	ccprintf("dram_bss_val = %d\n", (unsigned int)dram_bss_val);
	for (i = 0; i < ARRAY_SIZE(dram_data_val); ++i)
		ccprintf("dram_data_val[%d]= %d\n", i,
			(unsigned int)(dram_data_val[i]));

	for (i = 0; i < ARRAY_SIZE(dram_rodata_val); ++i)
		ccprintf("dram_rodata_val[%d]= %d\n", i,
			(unsigned int)(dram_rodata_val[i]));
}

/* Initialize board.  */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_EINT5_TP);
	gpio_enable_interrupt(GPIO_EINT6_TP);
	gpio_enable_interrupt(GPIO_EINT7_TP);

	dram_text_func();
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
