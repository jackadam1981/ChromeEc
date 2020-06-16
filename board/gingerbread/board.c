/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Gingerbread board-specific configuration */

#include "common.h"
#include "driver/ppc/sn5s330.h"
#include "driver/tcpm/ps8xxx.h"
#include "driver/tcpm/stm32gx.h"
#include "driver/tcpm/tcpci.h"
#include "driver/usb_mux/tusb1064.h"
#include "gpio.h"
#include "hooks.h"
#include "mp4245.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "usb_pe_sm.h"
#include "usb_prl_sm.h"
#include "usb_tc_sm.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

static void ppc_interrupt(enum gpio_signal signal)
{
	switch (signal) {
	case GPIO_HOST_USBC_PPC_INT_ODL:
		sn5s330_interrupt(0);
		break;

	default:
		break;
	}
}

static void mp4245_interrupt(enum gpio_signal signal)
{
	mp4245_alert_handler();
}

void hpd_interrupt(enum gpio_signal signal)
{
	baseboard_manage_hpd_event(signal);
}

#include "gpio_list.h" /* Must come after other header files. */

const struct power_seq board_power_seq[BOARD_NUM_POWER_GPIOS] = {
	{GPIO_EN_AC_JACK,               1, 20},
	{GPIO_EN_PP5000_A,              1, 31},
	{GPIO_EN_PP3300_A,              1, 35},
	{GPIO_STATUS_LED1,              0, 100},
	{GPIO_EN_BB,                    1, 30},
	{GPIO_EN_PP1100_A,              1, 30},
	{GPIO_EN_PP1000_A,              1, 20},
	{GPIO_EN_PP1050_A,              1, 30},
	{GPIO_EN_PP1200_A,              1, 20},
	{GPIO_EN_PP5000_HSPORT,         1, 31},
	{GPIO_EN_DP_SINK,               1, 80},
	{GPIO_MST_LP_CTL_L,             1, 80},
	{GPIO_MST_RST_L,                1, 41},
	{GPIO_EC_HUB1_RESET_L,          1, 41},
	{GPIO_EC_HUB2_RESET_L,          1, 33},
	{GPIO_USBC_DP_PD_RST_L,         1, 100},
	{GPIO_USBC_UF_RESET_L,          1, 33},
	{GPIO_DEMUX_DUAL_DP_PD_N,       1, 100},
	{GPIO_DEMUX_DUAL_DP_RESET_N,    1, 100},
	{GPIO_DEMUX_DP_HDMI_PD_N,       1, 10},
	{GPIO_DEMUX_DUAL_DP_MODE,       1, 10},
	{GPIO_DEMUX_DP_HDMI_MODE,       1, 1},
	{GPIO_STATUS_LED2,              0, 100},
};

void board_hpd_update(const struct usb_mux *me, int hpd_lvl, int hpd_irq)
{

}

/* TCPCs */
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.bus_type = EC_BUS_TYPE_EMBEDDED,
		.drv = &stm32gx_tcpm_drv,
	},
};

const struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	[USB_PD_PORT_HOST] = {
		.usb_port = USB_PD_PORT_HOST,
		.i2c_addr_flags = TUSB1064_I2C_ADDR0_FLAG,
		.driver = &tusb1064_usb_mux_driver,
		.hpd_update = &board_hpd_update,
	},
};

/* USB-C PPC Configuration */
struct ppc_config_t ppc_chips[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	[USB_PD_PORT_HOST] = {
		.i2c_port = 2,
		.i2c_addr_flags = SN5S330_ADDR0_FLAGS,
		.drv = &sn5s330_drv
	},
};
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

/* Power Delivery and charging functions */
void board_tcpc_init(void)
{
	/* Only reset TCPC if not sysjump */
	/* if (!system_jumped_to_this_image()) */
	/* 	board_reset_pd_mcu(); */

	/* Enable PPC interrupts. */
	gpio_enable_interrupt(GPIO_HOST_USBC_PPC_INT_ODL);
	/* Enable TCPC interrupts. */

	/* Enable HPD interrupt */
	gpio_enable_interrupt(GPIO_DDI_MST_IN_HPD);

}
DECLARE_HOOK(HOOK_INIT, board_tcpc_init, HOOK_PRIO_INIT_I2C + 1);

static void board_select_drp_mode(void)
{

	pd_set_dual_role(0, PD_DRP_TOGGLE_ON);
	CPRINTS("ucpd: drp_state = %d", pd_get_dual_role(0));
}
DECLARE_DEFERRED(board_select_drp_mode);

static void board_manage_led(void)
{
	static int counter;

	gpio_set_level(GPIO_STATUS_LED1, counter & 1);
	gpio_set_level(GPIO_STATUS_LED2, counter & 1);
	counter++;
}
DECLARE_HOOK(HOOK_SECOND,board_manage_led, HOOK_PRIO_DEFAULT);

static void board_init(void)
{
	hook_call_deferred(&board_select_drp_mode_data, 25 * MSEC);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

int ppc_get_alert_status(int port)
{
	if (port == USB_PD_PORT_HOST)
		return gpio_get_level(GPIO_HOST_USBC_PPC_INT_ODL) == 0;

	return EC_ERROR_UNIMPLEMENTED;
}

void board_overcurrent_event(int port, int is_overcurrented)
{
	/* TODO: b/ - check correct operation for honeybuns */
}

void board_debug_gpio(int trigger, int enable)
{
	switch (trigger) {
	case TRIGGER_1:
		gpio_set_level(GPIO_TRIGGER_1, enable);
		break;
	case TRIGGER_2:
		gpio_set_level(GPIO_TRIGGER_2, enable);
		break;
	default:
		CPRINTS("bad debug gpio selection");
		break;
	}
}
