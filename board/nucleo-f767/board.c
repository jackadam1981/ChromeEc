/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* nucleo-f767zi development board configuration */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "util.h"

void user_button_evt(enum gpio_signal signal)
{
	ccprintf("Button %d, %d!\n", signal, gpio_get_level(signal));
	gpio_set_level(GPIO_LED_GREEN, !gpio_get_level(GPIO_LED_GREEN));
}

#include "gpio_list.h"

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON_L);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
