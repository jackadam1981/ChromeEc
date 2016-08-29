/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "registers.h"
#include "task.h"
#include "tcpci.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "util.h"

void alert_event(enum gpio_signal signal)
{
	/* Exchange status with PD MCU. */
	host_command_pd_send_status(PD_CHARGE_NO_CHANGE);
}

#include "gpio_list.h"

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
	{"tcpc", I2C_PORT_TCPC, 100 /* kHz */, GPIO_I2C_A_SCL, GPIO_I2C_A_SDA}
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_COUNT] = {
	{I2C_PORT_TCPC, TCPC1_I2C_ADDR, &tcpci_tcpm_drv},
#if CONFIG_USB_PD_PORT_COUNT >= 2
	{I2C_PORT_TCPC, TCPC2_I2C_ADDR, &tcpci_tcpm_drv},
#endif
};

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;

	if (!gpio_get_level(GPIO_PD_MCU_INT)) {
		status = PD_STATUS_TCPC_ALERT_0;
#if CONFIG_USB_PD_PORT_COUNT >= 2
		status |= PD_STATUS_TCPC_ALERT_1;
#endif
	}

	return status;
}
