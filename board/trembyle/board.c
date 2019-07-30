/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Trembyle board configuration */

#include "button.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/bc12/pi3usb9201.h"
#include "extpower.h"
#include "gpio.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "switch.h"
#include "task.h"
#include "usb_charge.h"

static void ppc_interrupt(enum gpio_signal signal)
{
	/* TODO */
}

static void bc12_interrupt(enum gpio_signal signal)
{
	switch (signal) {
	case GPIO_USB_C0_BC12_INT_ODL:
		task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_BC12, 0);
		break;

	case GPIO_USB_C1_BC12_INT_ODL:
		task_set_event(TASK_ID_USB_CHG_P1, USB_CHG_EVENT_BC12, 0);
		break;

	default:
		break;
	}
}

#include "gpio_list.h"

/******************************************************************************/
/* USB-C TPCP Configuration */
const struct pi3usb2901_config_t pi3usb2901_bc12_chips[] = {
	[USB_PD_PORT_TCPC_0] = {
		.i2c_port = I2C_PORT_PPC0,
		.i2c_addr_flags = PI3USB9201_I2C_ADDR_3_FLAGS,
	},

	[USB_PD_PORT_TCPC_1] = {
		.i2c_port = I2C_PORT_TCPC1,
		.i2c_addr_flags = PI3USB9201_I2C_ADDR_3_FLAGS,
	},
};

void board_update_sensor_config_from_sku(void)
{
	/* Enable Gyro interrupts */
	gpio_enable_interrupt(GPIO_6AXIS_INT_L);
}

int board_is_sourcing_vbus(int port)
{
	/* TODO */
	return 0;
}

void board_reset_pd_mcu(void)
{
	/* TODO */
}

uint32_t system_get_sku_id(void)
{
	/* TODO */
	return 0;
}

uint16_t tcpc_get_alert_status(void)
{
	/* TODO */
	return 0;
}

void tcpc_alert_event(enum gpio_signal signal)
{
	/* TODO */
}

int charger_get_vbus_voltage(int port)
{
	/* TODO */
	return 0;
}

int board_set_active_charge_port(int port)
{
	/* TODO */
	return 0;
}
