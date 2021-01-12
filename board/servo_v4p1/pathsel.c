/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gl3590.h"
#include "gpio.h"
#include "ioexpanders.h"
#include "pathsel.h"
#include "util.h"

void init_pathsel(void)
{
	/* Connect TypeA port to DUT hub */
	usb3_a0_to_dut();
	/* Connect data lines */
	usb3_a0_mux_en_l(0);

	/* Enable power */
	usb3_a0_pwr_en(1);

	/* Connect TypeA port to DUT hub */
	usb3_a1_to_dut();
	/* Connect data lines */
	gpio_set_level(GPIO_USB3_A1_MUX_EN_L, 0);

	/* Enable power */
	usb3_a1_pwr_en(1);
}

void usb3_a0_to_dut(void)
{
	usb3_a0_mux_sel(1);
	gpio_set_level(GPIO_FASTBOOT_DUTHUB_MUX_SEL, 1);
}

void usb3_a1_to_dut(void)
{
	usb3_a1_mux_sel(1);
	gpio_set_level(GPIO_FASTBOOT_DUTHUB_MUX_SEL, 1);
}

void usb3_a0_to_host(void)
{
	usb3_a0_mux_sel(0);
}

void usb3_a1_to_host(void)
{
	usb3_a1_mux_sel(0);
}

void dut_to_host(void)
{
	gpio_set_level(GPIO_FASTBOOT_DUTHUB_MUX_SEL, 0);
	gpio_set_level(GPIO_FASTBOOT_DUTHUB_MUX_EN_L, 0);
	uservo_fastboot_mux_sel(MUX_SEL_FASTBOOT);
}


void uservo_to_host(void)
{
	uservo_fastboot_mux_sel(MUX_SEL_USERVO);
}

int usb3_a0_pwr_en(int en)
{
	if (board_id_det() <= BOARD_ID_REV1)
		return ec_usb3_a1_pwr_en(en);
	else
		return gl3590_enable_ports(0, GL3590_DFP2, en);
}

int usb3_a1_pwr_en(int en)
{
	if (board_id_det() <= BOARD_ID_REV1)
		return ec_usb3_a1_pwr_en(en);
	else
		return gl3590_enable_ports(0, GL3590_DFP1, en);
}

int uservo_pwr_en(int en)
{
	if (board_id_det() <= BOARD_ID_REV1)
		return ec_uservo_power_en(en);
	else
		return gl3590_enable_ports(0, GL3590_DFP4, en);
}

static int cmd_usb_port_en(int argc, char *argv[])
{
	int en;

	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;

	if (strcasecmp(argv[2], "on") == 0)
		en = 1;
	else if (strcasecmp(argv[2], "off") == 0)
		en = 0;
	else
		return EC_ERROR_PARAM2;

	if (strcasecmp(argv[1], "top") == 0)
		return usb3_a0_pwr_en(en);
	else if (strcasecmp(argv[1], "bottom") == 0)
		return usb3_a1_pwr_en(en);
	else if (strcasecmp(argv[1], "uservo") == 0)
		return uservo_pwr_en(en);
	else
		return EC_ERROR_PARAM1;
}
DECLARE_CONSOLE_COMMAND(usb_port_pwr, cmd_usb_port_en,
			" <top | bottom | uservo> <on | off>",
			"Manage Servo v4p1 USB ports power.");
