/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Kukui eMMC emulator board configuration */

#include "clock.h"
#include "common.h"
#include "dma.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "i2c.h"
#include "printf.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#include "gpio_list.h"

#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/******************************************************************************/
/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"charger",   I2C_PORT_CHARGER,   400, GPIO_I2C2_SCL, GPIO_I2C2_SDA},
	{"tcpc0",     I2C_PORT_TCPC0,     400, GPIO_I2C2_SCL, GPIO_I2C2_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/******************************************************************************
 * Initialize board.
 */
static void board_init(void)
{
}
/* This needs to happen before PWM is initialized. */
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_INIT_PWM - 1);

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB1ENR |= 1 << 0;
}
