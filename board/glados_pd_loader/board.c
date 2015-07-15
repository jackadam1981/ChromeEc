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
#include "jtag.h"
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

#include "gpio_list.h"

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"slave", I2C_PORT_SLAVE, 1000, GPIO_SLAVE_I2C_SCL, GPIO_SLAVE_I2C_SDA}
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

void ignore_bus_fault(int ignored)
{ }


extern void host_command_check_and_process(int evt);
extern void hardware_init(void);
extern void i2c_init(void);

int main(void)
{
	/* Enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;

	/* Configure the pin multiplexers and GPIOs */
	jtag_pre_init();
	gpio_pre_init();

	/* Initialize system module */
	system_pre_init();
	system_common_pre_init();

	/*
	 * Initialize flash and apply write protect if necessary.  Requires
	 * the reset flags calculated by system initialization.
	 */
	flash_pre_init();

	/* Initialize all other peripherals not using the common code */
	hardware_init();

	/* Run HOOK_INITs: i2c_init */
	i2c_init();

	gpio_config_module(MODULE_UART, 1);
	debug_printf("\n\n--- UART initialized after reboot ---\n");
	debug_printf("[Reset cause: 0x%04x]\n", system_get_reset_flags());
	debug_printf("[Image: %s, %s]\n",
		 system_get_image_copy_string(), system_get_build_info());

	while (1) {
		int evt = task_wait_event(1000*MSEC);

		/* Reload the watchdog */
		STM32_IWDG_KR = STM32_IWDG_KR_RELOAD;

		/* Check and process host commands */
		host_command_check_and_process(evt);

		/* Read ADCs */
		debug_printf("ADC C0 %d %d, C1 %d %d\n",
				adc_read_channel(ADC_C0_CC1_PD),
				adc_read_channel(ADC_C0_CC2_PD),
				adc_read_channel(ADC_C1_CC1_PD),
				adc_read_channel(ADC_C1_CC2_PD));
	}

	debug_printf("EXIT!\n");
	/* we should never reach that point */
	system_reset(0);
	return 0;
}

