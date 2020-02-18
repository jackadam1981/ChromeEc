/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nucleo-H7A3ZI baseboard setup */

#include "common.h"
#include "gpio.h" // Will
#include "hooks.h"
#include "registers.h"

__overridable void button_event(enum gpio_signal signal)
{
}

/* Initialize board. */
static void baseboard_init(void)
{
	STM32_DBGMCU_CR |= BIT(0)|BIT(1)|BIT(2) | BIT(7)|BIT(8);
}
DECLARE_HOOK(HOOK_INIT, baseboard_init, HOOK_PRIO_FIRST);