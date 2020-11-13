/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Type-C port manager for Cypress EZ-PD CCG6DF, CCG6SF */

#include "ccgxxf.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"

static int ccgxxf_tcpm_init(int port)
{
	return 0;
}

static int ccgxxf_tcpm_release(int port)
{
	return 0;
}

static int ccgxxf_tcpm_get_cc(int port, enum tcpc_cc_voltage_status *cc1,
	enum tcpc_cc_voltage_status *cc2)
{
	return 0;
}

static bool __maybe_unused ccgxxf_tcpm_check_vbus_level(int port,
						enum vbus_level level)
{
	return 0;
}

static int ccgxxf_tcpm_select_rp_value(int port, int rp)
{
	return 0;
}

static int ccgxxf_tcpm_set_cc(int port, int pull)
{
	return 0;
}

static int ccgxxf_tcpm_set_polarity(int port, enum tcpc_cc_polarity polarity)
{
	return 0;
}

static int ccgxxf_tcpm_set_vconn(int port, int enable)
{
	return 0;
}

static int ccgxxf_tcpm_set_msg_header(int port, int power_role, int data_role)
{
	return 0;
}

static int ccgxxf_tcpm_set_rx_enable(int port, int enable)
{
	return 0;
}

static int ccgxxf_tcpm_get_message_raw(int port, uint32_t *payload, int *head)
{
	return 0;
}

static int ccgxxf_tcpm_transmit(int port, enum tcpm_transmit_type type,
			uint16_t header, const uint32_t *data)
{
	return 0;
}

void ccgxxf_tcpc_alert(int port)
{
}

static int __maybe_unused ccgxxf_tcpm_enter_low_power_mode(int port)
{
	return 0;
}

const struct tcpm_drv ccgxxf_tcpm_drv = {
	.init			= &ccgxxf_tcpm_init,
	.release		= &ccgxxf_tcpm_release,
	.get_cc			= &ccgxxf_tcpm_get_cc,
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	.check_vbus_level	= &ccgxxf_tcpm_check_vbus_level,
#endif
	.select_rp_value	= &ccgxxf_tcpm_select_rp_value,
	.set_cc			= &ccgxxf_tcpm_set_cc,
	.set_polarity		= &ccgxxf_tcpm_set_polarity,
	.set_vconn		= &ccgxxf_tcpm_set_vconn,
	.set_msg_header		= &ccgxxf_tcpm_set_msg_header,
	.set_rx_enable		= &ccgxxf_tcpm_set_rx_enable,
	.get_message_raw	= &ccgxxf_tcpm_get_message_raw,
	.transmit		= &ccgxxf_tcpm_transmit,
	.tcpc_alert		= &ccgxxf_tcpc_alert,
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
	.enter_low_power_mode	= &ccgxxf_tcpm_enter_low_power_mode,
#endif
};

static int ccgxxf_init(int port)
{
	return 0;
}

static int ccgxxf_is_sourcing_vbus(int port)
{
	return 0;
}

static int ccgxxf_vbus_sink_enable(int port, int enable)
{
	return 0;
}

static int ccgxxf_vbus_source_enable(int port, int enable)
{
	return 0;
}

static int ccgxxf_set_vbus_source_current_limit(int port,
					enum tcpc_rp_value rp)
{
	return 0;
}

static int ccgxxf_discharge_vbus(int port, int enable)
{
	return 0;
}

static int ccgxxf_enter_low_power_mode(int port)
{
	return 0;
}

static int __maybe_unused ccgxxf_dump(int port)
{
	return 0;
}

static int __maybe_unused ccgxxf_is_vbus_present(int port)
{
	return 0;
}

static int ccgxxf_set_polarity(int port, int polarity)
{
	return 0;
}

static int ccgxxf_set_sbu(int port, int enable)
{
	return 0;
}

static int ccgxxf_set_vconn(int port, int enable)
{
	return 0;
}

const struct ppc_drv ccgxxf_ppc_drv = {
	.init = &ccgxxf_init,
	.is_sourcing_vbus = &ccgxxf_is_sourcing_vbus,
	.vbus_sink_enable = &ccgxxf_vbus_sink_enable,
	.vbus_source_enable = &ccgxxf_vbus_source_enable,
	.set_vbus_source_current_limit = &ccgxxf_set_vbus_source_current_limit,
	.discharge_vbus = &ccgxxf_discharge_vbus,
	.enter_low_power_mode = &ccgxxf_enter_low_power_mode,
#ifdef CONFIG_CMD_PPC_DUMP
	.reg_dump = &ccgxxf_dump,
#endif
#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
	.is_vbus_present = &ccgxxf_is_vbus_present,
#endif
#ifdef CONFIG_USBC_PPC_POLARITY
	.set_polarity = &ccgxxf_set_polarity,
#endif
#ifdef CONFIG_USBC_PPC_SBU
	.set_sbu = &ccgxxf_set_sbu,
#endif /* defined(CONFIG_USBC_PPC_SBU) */
#ifdef CONFIG_USBC_PPC_VCONN
	.set_vconn = &ccgxxf_set_vconn,
#endif
};
