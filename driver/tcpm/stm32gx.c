/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TCPM for MCU also running TCPC */

#include "chip/stm32/ucpd-stm32gx.h"
#include "common.h"
#include "config.h"
#include "console.h"
#include "registers.h"
#include "stm32gx.h"
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
	defined(CONFIG_USB_PD_TCPC_LOW_POWER)
#error "Unsupported config options of Stm32gx PD driver"
#endif

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

/* Wait time for vconn power switch to turn off. */
#ifndef PD_STM32GX_VCONN_TURN_OFF_DELAY_US
#define PD_STM32GX_VCONN_TURN_OFF_DELAY_US 500
#endif

static int cached_rp[CONFIG_USB_PD_PORT_MAX_COUNT];


static int stm32gx_tcpm_get_message_raw(int port, uint32_t *buf, int *head)
{
	return stm32gx_ucpd_get_message_raw(port, buf, head);
}

static int stm32gx_tcpm_init(int port)
{
	return stm32gx_ucpd_init(port);
}

static int stm32gx_tcpm_release(int port)
{
	return stm32gx_ucpd_release(port);
}

static int stm32gx_tcpm_get_cc(int port, enum tcpc_cc_voltage_status *cc1,
	enum tcpc_cc_voltage_status *cc2)
{
//	int role_control = stm32gx_ucpd_get_role_control(port);

	/* errors will return CC as open */
	*cc1 = TYPEC_CC_VOLT_OPEN;
	*cc2 = TYPEC_CC_VOLT_OPEN;

	/* Get cc_state value for each CC line */
	stm32gx_ucpd_get_cc(port, cc1, cc2);

	/* Map between cc_state and tcpc_cc_voltage_status */
	/* if (*cc1) */
	/* 	*cc1 += ((role_control & 0x3) == TYPEC_CC_RD) ? 4 : 0; */
	/* if (*cc2) */
	/* 	*cc2 += (((role_control >> 2) & 0x3) == TYPEC_CC_RD) ? 4 : 0; */

	return EC_SUCCESS;
}

static int stm32gx_tcpm_select_rp_value(int port, int rp_sel)
{
	cached_rp[port] = rp_sel;

	return EC_SUCCESS;
}

static int stm32gx_tcpm_set_cc(int port, int pull)
{

	return stm32gx_ucpd_set_cc(port, pull, cached_rp[port]);;
}

static int stm32gx_tcpm_set_polarity(int port, enum tcpc_cc_polarity polarity)
{
	return stm32gx_ucpd_set_polarity(port, polarity);
}

static int stm32gx_tcpm_set_vconn(int port, int enable)
{
	stm32gx_ucpd_vconn_disc_rp(port, enable);

	return EC_SUCCESS;
}

static int stm32gx_tcpm_set_msg_header(int port, int power_role, int data_role)
{
	return stm32gx_ucpd_set_msg_header(port, power_role, data_role);
}

static int stm32gx_tcpm_set_rx_enable(int port, int enable)
{

	return stm32gx_ucpd_set_rx_enable(port, enable);
}

static int stm32gx_tcpm_transmit(int port,
			enum tcpm_transmit_type type,
			uint16_t header,
			const uint32_t *data)
{
	int rv;

	if (header == 0x194f)
		CPRINTS("tcpm: sending svid.identity message");

	rv = stm32gx_ucpd_transmit(port, type, header, data);

	return rv;
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
