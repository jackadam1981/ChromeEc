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
#include "mpu.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "link_defs.h"

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

/* Put print_test in .data section to test XN (Execution Never) bit. */
__SECTION_KEEP(data) const void print_test(void)
{
	ccprintf("print_test success\n");
}

static int command_mpu_test(int argc, char **argv)
{
	print_test();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(mpu_test, command_mpu_test,
	"",
	"Test data RAM XN (Execution Never) bit.");

static int command_mpu(int argc, char **argv)
{
	if (argc < 1 || argc > 2)
		return EC_ERROR_PARAM_COUNT;

	if (argc != 2) {
		ccprintf("MPU ");
		if (MPU_CTRL & MPU_CTRL_ENABLE)
			ccprintf("enabled\n");
		else
			ccprintf("disabled\n");
		return EC_SUCCESS;
	}

	if (strtoi(argv[1], NULL, 10))
		mpu_enable();
	else
		mpu_disable();

	return EC_SUCCESS;

}
DECLARE_CONSOLE_COMMAND(mpu, command_mpu,
	"[1|0]",
	"Lock/Unlock MPU");
