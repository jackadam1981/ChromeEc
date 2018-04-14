/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel ICL-RVP-ITE board-specific configuration */

#include "button.h"
#include "chipset.h"
#include "console.h"
#include "espi.h"
#include "ec2i_chip.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "intc.h"
#include "ioexpander_it8300.h"
#include "ioexpander_pca9555.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "spi.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

#include "gpio_list.h"

#define I2C_PORT_PCA555_BOARD_ID_GPIO	IT83XX_I2C_CH_A
#define I2C_ADDR_PCA555_BOARD_ID_GPIO	0x44

#define CPRINTS(format, args...) cprints(CC_COMMAND, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ## args)

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_PCH_SLP_S0_L,
		POWER_SIGNAL_ACTIVE_HIGH | POWER_SIGNAL_DISABLE_AT_BOOT,
		"SLP_S0_DEASSERTED"},
#ifdef CONFIG_HOSTCMD_ESPI_VW_SLP_SIGNALS
	{VW_SLP_S3_L,	      POWER_SIGNAL_ACTIVE_HIGH, "SLP_S3_DEASSERTED"},
	{VW_SLP_S4_L,	      POWER_SIGNAL_ACTIVE_HIGH, "SLP_S4_DEASSERTED"},
	{VW_SLP_S5_L,	      POWER_SIGNAL_ACTIVE_HIGH, "SLP_S5_DEASSERTED"},
#else
	{GPIO_PCH_SLP_S3_L,   POWER_SIGNAL_ACTIVE_HIGH, "SLP_S3_DEASSERTED"},
	{GPIO_PCH_SLP_S4_L,   POWER_SIGNAL_ACTIVE_HIGH, "SLP_S4_DEASSERTED"},
	{GPIO_PCH_SLP_S5_L,   POWER_SIGNAL_ACTIVE_HIGH, "SLP_S5_DEASSERTED"},
#endif
	{GPIO_PCH_SLP_SUS_L,  POWER_SIGNAL_ACTIVE_HIGH, "SLP_SUS_DEASSERTED"},
	{GPIO_RSMRST_L_PGOOD, POWER_SIGNAL_ACTIVE_HIGH, "RSMRST_L_PGOOD"},
	{GPIO_DSW_DPWROK,     POWER_SIGNAL_ACTIVE_HIGH, "DSW_DPWROK"},
	{GPIO_ALL_SYS_PWRGD,  POWER_SIGNAL_ACTIVE_HIGH, "ALL_SYS_PWRGD"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"charger", IT83XX_I2C_CH_A, 100, GPIO_I2C_A_SCL, GPIO_I2C_A_SDA},
	{"masterB", IT83XX_I2C_CH_B, 400, GPIO_I2C_B_SCL, GPIO_I2C_B_SDA},
	{"masterC", IT83XX_I2C_CH_C, 100, GPIO_I2C_C_SCL, GPIO_I2C_C_SDA},
	{"ext_io",  IT83XX_I2C_CH_E, 400, GPIO_I2C_E_SCL, GPIO_I2C_E_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* Wake-up pins for hibernate */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_AC_PRESENT,
	GPIO_LID_OPEN,
	GPIO_POWER_BUTTON_L,
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

int board_get_version(void)
{
	int data_port0;
	int data_port1;

	if (pca9555_read(I2C_PORT_PCA555_BOARD_ID_GPIO,
		I2C_ADDR_PCA555_BOARD_ID_GPIO,
		PCA9555_CMD_INPUT_PORT_0, &data_port0))
		return -1;

	if (pca9555_read(I2C_PORT_PCA555_BOARD_ID_GPIO,
		I2C_ADDR_PCA555_BOARD_ID_GPIO,
		PCA9555_CMD_INPUT_PORT_1, &data_port1))
		return -1;

	/*
	 * Port1: BOM ID[7:5], BOARD ID[4:0]
	 * Port0: FAB ID([1:0] + 1)
	 *
	 * Returns board information (board id[7:0] and
	 * Fab id[15:8]) on success and < -1 on error.
	 */
	CPRINTS("BOM: 0x%x, BID: 0x%x FID: 0x%x, return_data = 0x%x",
		(data_port1 & 0xE0) >> 5, data_port1 & 0x1F,
		(data_port0 & 0x03) + 1,
		(data_port1 & 0x1F) | (((data_port0 & 0x03) + 1) << 8));

	return (data_port1 & 0x1F) | (((data_port0 & 0x03) + 1) << 8);
}

/* Keyboard scan setting */
struct keyboard_scan_config keyscan_config = {
	.output_settle_us = 35,
	.debounce_down_us = 5 * MSEC,
	.debounce_up_us = 40 * MSEC,
	.scan_period_us = 3 * MSEC,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = 100 * MSEC,
	.actual_key_mask = {
		0x14, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xfe, 0x55, 0xfa, 0xca  /* full set */
	},
};
