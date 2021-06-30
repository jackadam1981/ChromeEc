/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "util.h"

void gpio_enable_clocks(void)
{
	/*
	 * Enable all GPIOs clocks
	 *
	 * TODO(crosbug.com/p/23770): only enable the banks we need to,
	 * and support disabling some of them in low-power idle.
	 */
	STM32_RCC_AHB2ENR |= STM32_RCC_AHB2ENR_GPIOMASK;

	/* Delay 1 AHB clock cycle after the clock is enabled */
	clock_wait_bus_cycles(BUS_AHB, 1);
}

#include "gpio-f0-l.c"
