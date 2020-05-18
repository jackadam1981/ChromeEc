/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TCPM for MCU also running TCPC */

#include "common.h"
#include "config.h"
#include "console.h"
#include "it83xx_pd.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "tcpci.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "hooks.h"

#if defined(CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE) || \
	defined(CONFIG_USB_PD_VBUS_DETECT_TCPC) || \
	defined(CONFIG_USB_PD_TCPC_LOW_POWER) || \
	defined(CONFIG_USB_PD_DISCHARGE_TCPC)
#error "Unsupported config options of Stm32gx PD driver"
#endif

/* Wait time for vconn power switch to turn off. */
#ifndef PD_STM32GX_VCONN_TURN_OFF_DELAY_US
#define PD_STM32GX_VCONN_TURN_OFF_DELAY_US 500
#endif


/*
 * This function disables integrated pd module and enables 5.1K resistor for
 * dead battery. A EC reset or calling _init() is able to re-active pd module.
 */
void stm32gx_disable_pd_module(int port)
{

}

static enum tcpc_cc_voltage_status stm32gx_get_cc(
	enum usbpd_port port,
	enum usbpd_cc_pin cc_pin)
{

	return cc_state;
}

static int stm32gx_tcpm_get_message_raw(int port, uint32_t *buf, int *head)
{

	return EC_SUCCESS;
}

static enum tcpc_transmit_complete stm32gx_tx_data(
	enum usbpd_port port,
	enum tcpm_transmit_type type,
	uint16_t header,
	const uint32_t *buf)
{
	return TCPC_TX_COMPLETE_SUCCESS;
}

static enum tcpc_transmit_complete stm32gx_send_hw_reset(enum usbpd_port port,
				enum tcpm_transmit_type reset_type)
{
	return TCPC_TX_COMPLETE_SUCCESS;
}

static void stm32gx_send_bist_mode2_pattern(enum usbpd_port port)
{

}

static void stm32gx_enable_vconn(enum usbpd_port port, int enabled)
{

}

static void stm32gx_enable_cc(enum usbpd_port port, int enable)
{

}

static void stm32gx_set_power_role(enum usbpd_port port, int power_role)
{

}

static void stm32gx_set_data_role(enum usbpd_port port, int pd_role)
{

}

static void stm32gx_init(enum usbpd_port port, int role)
{

}

static void stm32gx_select_polarity(enum usbpd_port port,
					enum usbpd_cc_pin cc_pin)
{

}

static int stm32gx_set_cc(enum usbpd_port port, int pull)
{
	return EC_SUCCESS;
}

static int stm32gx_tcpm_init(int port)
{
	/* Initialize physical layer */
	stm32gx_init(port, PD_ROLE_DEFAULT(port));

	return EC_SUCCESS;
}

static int stm32gx_tcpm_release(int port)
{
	return EC_ERROR_UNIMPLEMENTED;
}

static int stm32gx_tcpm_get_cc(int port, enum tcpc_cc_voltage_status *cc1,
	enum tcpc_cc_voltage_status *cc2)
{
	return EC_SUCCESS;
}

static int stm32gx_tcpm_select_rp_value(int port, int rp_sel)
{
	return EC_SUCCESS;
}

static int stm32gx_tcpm_set_cc(int port, int pull)
{
	return stm32gx_set_cc(port, pull);
}

static int stm32gx_tcpm_set_polarity(int port, enum tcpc_cc_polarity polarity)
{
	return EC_SUCCESS;
}

static int stm32gx_tcpm_set_vconn(int port, int enable)
{

	return EC_SUCCESS;
}

static int stm32gx_tcpm_set_msg_header(int port, int power_role, int data_role)
{
	return EC_SUCCESS;
}

static int stm32gx_tcpm_set_rx_enable(int port, int enable)
{
	return EC_SUCCESS;
}

static int stm32gx_tcpm_transmit(int port,
			enum tcpm_transmit_type type,
			uint16_t header,
			const uint32_t *data)
{
	return EC_SUCCESS;
}

static int stm32gx_tcpm_get_chip_info(int port, int live,
			struct ec_response_pd_chip_info_v1 **chip_info)
{

	return EC_SUCCESS;
}

static void stm32gx_tcpm_sw_reset(void)
{

}

DECLARE_HOOK(HOOK_USB_PD_DISCONNECT, stm32gx_tcpm_sw_reset, HOOK_PRIO_DEFAULT);

const struct tcpm_drv stm32gx_tcpm_drv = {
	.init			= &stm32gx_tcpm_init,
	.release		= &stm32gx_tcpm_release,
	.get_cc			= &stm32gx_tcpm_get_cc,
	.select_rp_value	= &stm32gx_tcpm_select_rp_value,
	.set_cc			= &stm32gx_tcpm_set_cc,
	.set_polarity		= &stm32gx_tcpm_set_polarity,
	.set_vconn		= &stm32gx_tcpm_set_vconn,
	.set_msg_header		= &stm32gx_tcpm_set_msg_header,
	.set_rx_enable		= &stm32gx_tcpm_set_rx_enable,
	.get_message_raw	= &stm32gx_tcpm_get_message_raw,
	.transmit		= &stm32gx_tcpm_transmit,
	.get_chip_info		= &stm32gx_tcpm_get_chip_info,
};
