/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"

#include "gpio_list.h"

/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[] = {
	/* Not a real device */
	{"ECTemp", LM3_ADC_SEQ0, -225, ADC_READ_MAX, 420,
	 LM3_AIN_NONE, 0x0e /* TS0 | IE0 | END0 */, 0, 0},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"i2c_port", 0, 100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

void board_config_post_gpio_init(void)
{
}

