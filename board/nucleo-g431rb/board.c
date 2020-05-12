/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* STM32G431 Nucleo-64 board-specific configuration */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "util.h"

#include "gpio_list.h" /* Must come after other header files. */

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

#define I2C_TEST_PORT0 0

static int board_power_sequence(void)
{
	/* turn on main board power */
	gpio_set_level(GPIO_EN_AC_JACK, 1);
	/* Turn on 5V rail */
	gpio_set_level(GPIO_EN_PP5000_A, 1);
	/* Turn on all 3.3V rails */
	gpio_set_level(GPIO_EN_PP3300_BB, 1);
	/* Turn 1.2 V rail */
	gpio_set_level(GPIO_EN_PP1200_A, 1);

	return EC_SUCCESS;
}

static void board_init(void)
{
	/* Turn on power rails */
	board_power_sequence();
	CPRINTS("board: Power rails enabled");
	/* Delay 5 msec before taking TCPC out of reset */
	msleep(5);
	/* Take TCPC out of reset */
	gpio_set_level(GPIO_USBC_DP_PD_RST_L, 1);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

void test_i2c_access(void)
{
	int rv;
	int i2c_reg;

	/* Read register address 0 of PS8805 */
	rv = i2c_read16(I2C_PORT_USBC, 0x36, 0, &i2c_reg);
	if (rv)
		CPRINTS("ps8805 read failed! Error = %d", rv);
	else
		CPRINTS("ps8805 reg 0 = 0x%x", i2c_reg);
}

static void led_second(void)
{
#if I2C_TEST_PORT0
	static int count;
	static int attempt;

	if ((count++ & 2) && (attempt < 3)) {
		test_i2c_access();
		attempt++;
	}
#endif
}
DECLARE_HOOK(HOOK_SECOND, led_second, HOOK_PRIO_DEFAULT);

