/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Honeybuns family-specific configuration */
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "usb_pd.h"
#include "system.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

/******************************************************************************/

static void board_power_sequence(void)
{
	int i;

	for(i = 0; i < board_power_seq_count; i++) {
		gpio_set_level(board_power_seq[i].signal,
			       board_power_seq[i].level);
		msleep(board_power_seq[i].delay_ms);
	}
}

/******************************************************************************/
/* I2C port map configuration */
const struct i2c_port_t i2c_ports[] = {
	{"i2c1",  I2C_PORT_I2C1,  400, GPIO_EC_I2C1_SCL, GPIO_EC_I2C1_SDA},
	{"i2c2",  I2C_PORT_I2C2,  400, GPIO_EC_I2C2_SCL, GPIO_EC_I2C2_SDA},
	{"i2c3",  I2C_PORT_I2C3,  400, GPIO_EC_I2C3_SCL, GPIO_EC_I2C3_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#ifndef SECTION_IS_RW
static void baseboard_set_usbc_sink_mode(void)
{
	uint32_t cr;

	/*
	 * Bare minimum code required to enable UCPD peripheral and apply Rd to
	 * the CC lines. This is only applied in RO.
	 */
	/* Ensure that clock to UCPD is enabled */
	STM32_RCC_APB1ENR2 |= STM32_RCC_APB1ENR2_UPCD1EN;
	/* enable the peripheral */
	STM32_UCPD_CFGR1(0) |= STM32_UCPD_CFGR1_UCPDEN;

	cr = STM32_UCPD_CR(0);
	/* Apply Rd to both CC lines */
	cr |= STM32_UCPD_CR_ANAMODE | STM32_UCPD_CR_CCENABLE_MASK;
	STM32_UCPD_CR(0) = cr;

	CPRINTS("usbc: CR = 0x%x", STM32_UCPD_CR(0));
}
#endif

static void baseboard_init(void)
{
	/* Turn on power rails */
	board_power_sequence();
	CPRINTS("board: Power rails enabled");

#ifdef SECTION_IS_RW
	system_clear_reset_flags(EC_RESET_FLAG_POWER_ON);
#else
	baseboard_set_usbc_sink_mode();
#endif
}
DECLARE_HOOK(HOOK_INIT, baseboard_init, HOOK_PRIO_INIT_I2C + 1);
