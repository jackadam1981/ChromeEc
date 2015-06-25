/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* cube board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ## args)

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;
}

static void usb_vbus_detect(enum gpio_signal signal)
{
}

static void pcie3_wake(enum gpio_signal signal)
{
}

static void pcie0_wake(enum gpio_signal signal)
{
}

#include "gpio_list.h"

static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USB_VBUS_DETECT);
	gpio_enable_interrupt(GPIO_PCIE3_3P3_WAKE_L);
	gpio_enable_interrupt(GPIO_PCIE0_3P3_WAKE_L);

	CPRINTF("hello!");
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

const struct adc_t adc_channels[] = {
	[ADC_C0_CC1_PD] = {"CC1", 3300, 4096, 0, STM32_AIN(2)},
	[ADC_C0_CC2_PD] = {"CC2", 3300, 4096, 0, STM32_AIN(4)},
	/*
	 * sense resistor: 10 mOhm
	 * scale:          50 x
	 */
	[ADC_CUR_SENSE] = {"CUR", 6600, 4096, 0, STM32_AIN(9)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

const struct i2c_port_t i2c_ports[] = {
	{"pd", I2C_PORT_SLAVE, 1000, GPIO_SLAVE_I2C_SCL, GPIO_SLAVE_I2C_SDA}
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);


