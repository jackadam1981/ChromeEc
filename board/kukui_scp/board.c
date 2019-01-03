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
#include "memmap.h"
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

static void dump_memmap(int (*scp_to_ap)(uintptr_t, uintptr_t *),
			int (*ap_to_scp)(uintptr_t, uintptr_t *))
{
	uintptr_t scp = 0x00001000;
	uintptr_t ap, scp2;
	int i;

	for (i = 0; i < 32; i++, scp += (i % 2) ? 0x0f001000 : 0x01001000) {
		int ret;

		cflush();

		ret = (*scp_to_ap)(scp, &ap);
		if (ret != EC_SUCCESS) {
			ccprintf("  %08x INVAL\n", scp);
			continue;
		}

		ret = (*ap_to_scp)(ap, &scp2);

		ccprintf("  %08x %08x => %08x %s\n", scp, ap, scp2,
			(ret == EC_SUCCESS && scp == scp2) ? "OK" : "BAD");
	}
}

static int command_memmaptest(int argc, char **argv)
{
	ccprintf("Direct mapping:\n");
	dump_memmap(&memmap_scp_to_ap, &memmap_ap_to_scp);

	ccprintf("Cached mapping:\n");
	dump_memmap(&memmap_scp_cache_to_ap, &memmap_ap_to_scp_cache);

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(memmaptest, command_memmaptest,
			     NULL,
			     "Do remmap test");
