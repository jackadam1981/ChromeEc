/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* STM32F072-discovery board based USB PD evaluation configuration */

#include "common.h"
#include "anx7406.h"
#include "anx7447.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "registers.h"
#include "task.h"
#include "tcpm/tcpci.h"
#include "usb_descriptor.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "util.h"

void button_event(enum gpio_signal signal);

void alert_event(enum gpio_signal signal)
{
#ifdef SECTION_IS_RO
	/* Exchange status with PD MCU. */
	host_command_pd_send_status(PD_CHARGE_NO_CHANGE);
#endif
}

#include "gpio_list.h"

/*
 * GPIO initialization is defined for both RO and RW, but TCPC and PD code will
 * only be compiled into RO.
 */
#ifdef SECTION_IS_RO
const void *const usb_strings[] = {
	[USB_STR_DESC]         = usb_string_desc,
	[USB_STR_VENDOR]       = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]      = USB_STRING_DESC("PDeval-stm32f072"),
	[USB_STR_VERSION]      = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("Shell"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON);
	gpio_enable_interrupt(GPIO_PD_MCU_INT);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

void board_reset_pd_mcu(void)
{
}

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{
		.name = "tcpc",
		.port = I2C_PORT_TCPC,
		.kbps = 400 /* kHz */,
		.scl  = GPIO_I2C0_SCL,
		.sda  = GPIO_I2C0_SDA
	}
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
#ifdef CONFIG_USB_PD_TCPM_ANX7447
	{
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_TCPC,
			.addr_flags = AN7447_TCPC2_I2C_ADDR_FLAGS,
		},
		.drv = &anx7447_tcpm_drv,
	},
#elif defined(CONFIG_USB_PD_TCPM_ANX7406)
	{
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_TCPC,
			.addr_flags = ANX7406_TCPC0_I2C_ADDR_FLAGS,
		},
		.drv = &anx7406_tcpm_drv,
	},
#endif
};

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;

	if (!gpio_get_level(GPIO_PD_MCU_INT)) {
		status = PD_STATUS_TCPC_ALERT_0;
	}

	return status;
}

#endif /* SECTION_IS_RO */
