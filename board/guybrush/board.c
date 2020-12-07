/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Guybrush board-specific configuration */

#include "button.h"
#include "common.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "util.h"

#include "gpio_list.h" /* Must come after other header files. */

static void board_init(void)
{
	/* TODO */
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

void sbu_fault_interrupt(enum ioex_signal signal)
{
	/* TODO */
}

void tcpc_alert_event(enum gpio_signal signal)
{
	/* TODO */
}

void ppc_interrupt(enum gpio_signal signal)
{
	/* TODO */
}

void bc12_interrupt(enum gpio_signal signal)
{
	/* TODO */
}

void bmi160_interrupt(enum gpio_signal signal)
{
	/* TODO */
}
