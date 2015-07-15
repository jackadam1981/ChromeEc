/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* glados_pd board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "clock.h"
#include "common.h"
#include "console.h"
#include "debug_printf.h"
#include "flash.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "usb_pd.h"
#include "util.h"

static uint32_t ec_int_status;

void pd_send_ec_int(void)
{
	gpio_set_level(GPIO_EC_INT, !ec_int_status);

}

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;
}

#include "gpio_list.h"

/* ADC channels */
const struct adc_t adc_channels[] = {
	/* USB PD CC lines sensing. Converted to mV (3300mV/4096). */
	[ADC_C1_CC1_PD] = {"C1_CC1_PD", 3300, 4096, 0, STM32_AIN(0)},
	[ADC_C0_CC1_PD] = {"C0_CC1_PD", 3300, 4096, 0, STM32_AIN(2)},
	[ADC_C0_CC2_PD] = {"C0_CC2_PD", 3300, 4096, 0, STM32_AIN(4)},
	[ADC_C1_CC2_PD] = {"C1_CC2_PD", 3300, 4096, 0, STM32_AIN(5)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"slave", I2C_PORT_SLAVE, 1000, GPIO_SLAVE_I2C_SCL, GPIO_SLAVE_I2C_SDA}
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

void ignore_bus_fault(int ignored)
{ }

extern void host_command_task(void);
extern void hardware_init(void);

extern void adc_init(void);
extern void i2c_init(void);
int main(void)
{
	board_config_pre_init();
	gpio_pre_init();

	system_pre_init();
	system_common_pre_init();

	flash_pre_init();

	hardware_init();

	/* run HOOK_INITs: adc_init, i2c_init */
	adc_init();
	i2c_init();

	gpio_config_module(MODULE_UART, 1);

	debug_printf("Hey, this started!\n");
	host_command_task();
	return 0;
}

