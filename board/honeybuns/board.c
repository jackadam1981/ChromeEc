/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Honeybuns board-specific configuration */

#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "util.h"

#include "gpio_list.h" /* Must come after other header files. */

static int count;
static void board_init(void)
{
	/* TODO */
	count = 0;
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

static void led_second(void)
{
	gpio_set_level(GPIO_LED1, count++ & 1);
}
DECLARE_HOOK(HOOK_SECOND, led_second, HOOK_PRIO_DEFAULT);
