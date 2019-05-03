/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel ICLY-RVP, ITE EC board-specific configuration */
#include "adc.h"
#include "button.h"
#include "extpower.h"
#include "i2c.h"
#include "intc.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "switch.h"
#include "system.h"
#include "tablet_mode.h"
#include "uart.h"

#include "gpio_list.h"

#define CPRINTS(format, args...) cprints(CC_COMMAND, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ## args)

/* TCPC gpios */
const struct tcpc_gpio_config_t tcpc_gpios[CONFIG_USB_PD_PORT_COUNT] = {
	[TYPE_C_PORT_0] = {
		.vbus = {
			.pin = GPIO_USB_C0_VBUS_INT,
			.pin_pol = 1,
		},
		.src = {
			.pin = GPIO_USB_C0_SRC_EN,
			.pin_pol = 1,
		},
		.snk = {
			.pin = GPIO_USB_C0_SNK_EN_L,
			.pin_pol = 0,
		},
	},
	[TYPE_C_PORT_1] = {
		.vbus = {
			.pin = GPIO_USB_C1_VBUS_INT,
			.pin_pol = 1,
		},
		.src = {
			.pin = GPIO_USB_C1_SRC_EN,
			.pin_pol = 1,
		},
		.snk = {
			.pin = GPIO_USB_C1_SNK_EN_L,
			.pin_pol = 0,
		},
	},
};
BUILD_ASSERT(ARRAY_SIZE(tcpc_gpios) == CONFIG_USB_PD_PORT_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"chan-A", IT83XX_I2C_CH_A, 100, GPIO_I2C_A_SCL, GPIO_I2C_A_SDA},
	/*
	 * Port-80 Display, Charger, Battery, IO-expanders, EEPROM,
	 * IMVP9, AUX-rail, power-monitor.
	 */
	{"batt_chg", IT83XX_I2C_CH_B, 100, GPIO_I2C_B_SCL, GPIO_I2C_B_SDA},
	/* Retimers, PDs */
	{"retimer", IT83XX_I2C_CH_E, 100, GPIO_I2C_E_SCL, GPIO_I2C_E_SDA},
#if 0
	{"chan-C", IT83XX_I2C_CH_C, 100, GPIO_I2C_C_SCL, GPIO_I2C_C_SDA},
	/* SMLink, PDs */
	{"smlink", IT83XX_I2C_CH_F, 100, GPIO_I2C_F_SCL, GPIO_I2C_F_SDA},
#endif
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/*
 * Returns board information (board id[7:0] and Fab id[15:8]) on success
 * -1 on error.
 */
int board_get_version(void)
{
	int fab_id;
	int board_id;
	int bom_id;

	if (ioexpander_read_intelrvp_version(&fab_id, &board_id))
		return -1;
	/*
	 * Port0: bit 1:0 - FAB ID(1:0) + 1
	 * Port1: bit 7:5 - BOM ID(7:5)
	 *        bit 4:0 - BOARD ID(4:0)
	 */
	bom_id = (board_id & 0xE0) >> 5;
	fab_id = (fab_id & 0x03) + 1;
	board_id = board_id & 0x1F;

	CPRINTS("BID:0x%x, FID:0x%x, BOM:0x%x", board_id, fab_id, bom_id);

	return board_id | (fab_id << 8);
}
