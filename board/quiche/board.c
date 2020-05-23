/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Quiche board-specific configuration */

#include "common.h"
#include "driver/tcpm/ps8xxx.h"
#include "driver/tcpm/stm32gx.h"
#include "driver/tcpm/tcpci.h"
#include "gpio.h"
#include "hooks.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "util.h"

#include "gpio_list.h" /* Must come after other header files. */

const struct power_seq board_power_seq[BOARD_NUM_POWER_GPIOS] = {
	{GPIO_EN_AC_JACK,               1, 20},
	{GPIO_EN_PP5000_A,              1, 31},
	{GPIO_EN_PP3300_BB,             1, 100},
	{GPIO_EN_BB,                    1, 30},
	{GPIO_EN_PP1100_A,              1, 30},
	{GPIO_EN_PP1050_A,              1, 30},
	{GPIO_EN_PP1200_A,              1, 20},
	{GPIO_EN_PP5000_C,              1, 20},
	{GPIO_EN_PP5000_HSPORT,         1, 31},
	{GPIO_EN_DP_SINK,               1, 80},
	{GPIO_MST_RST_L,                1, 20},
	{GPIO_MST_LP_CTL_L,             1, 41},
	{GPIO_EC_HUB2_RESET_L,          1, 41},
	{GPIO_EC_HUB3_RESET_L,          1, 33},
	{GPIO_DP_SINK_RESET,            1, 100},
	{GPIO_USBC_DP_PD_RST_L,         1, 100},
	{GPIO_USBC_UF_RESET_L,          1, 33},
	{GPIO_DEMUX_DUAL_DP_PD_N,       1, 100},
	{GPIO_DEMUX_DUAL_DP_RESET_N,    1, 100},
	{GPIO_DEMUX_DP_HDMI_PD_N,       1, 10},
	{GPIO_DEMUX_DUAL_DP_MODE,       1, 10},
	{GPIO_DEMUX_DP_HDMI_MODE,       1, 1},
};

/* TCPCs */
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.bus_type = EC_BUS_TYPE_EMBEDDED,
		.drv = &stm32gx_tcpm_drv,
	},
};

const struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	[0] = {
		.usb_port = 0,
		.driver = &virtual_usb_mux_driver,
		.hpd_update = &virtual_hpd_update,
	},
};

static void board_init(void)
{
	/* TODO */
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
