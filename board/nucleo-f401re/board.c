/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* nucleo-f401re development board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"

#include "gpio.h"
#include "registers.h"
#include "task.h"
#include "util.h"

void user_button_evt(enum gpio_signal signal)
{
	ccprintf("Button %d, %d!\n", signal, gpio_get_level(signal));
}

#include "gpio_list.h"

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON_L);

	/* No power control yet */
	/* Go to S3 state */
	hook_notify(HOOK_CHIPSET_STARTUP);

	/* Go to S0 state */
	hook_notify(HOOK_CHIPSET_RESUME);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* ADC channels */
const struct adc_t adc_channels[] = {
	/* Arduino connectors analog pins */
	[ADC1_0] = {"ADC1_0",  3000, 4096, 0, STM32_AIN(0)},
	[ADC1_1] = {"ADC1_1",  3000, 4096, 0, STM32_AIN(1)},
	[ADC1_4] = {"ADC1_4",  3000, 4096, 0, STM32_AIN(4)},
	[ADC1_8] = {"ADC1_8",  3000, 4096, 0, STM32_AIN(8)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 100,
	 GPIO_MASTER_I2C_SCL, GPIO_MASTER_I2C_SDA},
};

const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);
