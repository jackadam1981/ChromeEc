/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Type-C port manager for Cypress EZ-PD CCG6DF, CCG6SF */

#include "common.h"
#include "console.h"
#include "ps8xxx.h"
#include "tcpm/tcpci.h"
#include "tcpm/tcpm.h"
#include "timer.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "ccgxxf.h"
#include "usbc_ppc.h"
#include "usb_pd_tcpc.h"

#if !defined(CONFIG_USB_PD_TCPM_PPC_CCGXXF)
#error "Unsupported CCGXXF TCPC."
#endif

/*  CCGXXF FW is designed to adapt standard TCPM driver procedures.*/


const struct tcpm_drv ccgxxf_tcpm_drv = {
	.init			= &tcpci_tcpm_init,
	.release		= &tcpci_tcpm_release,
	.get_cc			= &tcpci_tcpm_get_cc,
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	.check_vbus_level	= &tcpci_tcpm_check_vbus_level,
#endif
	.select_rp_value	= &tcpci_tcpm_select_rp_value,
	.set_cc			= &tcpci_tcpm_set_cc,
	.set_polarity		= &tcpci_tcpm_set_polarity,
	.set_vconn		= &tcpci_tcpm_set_vconn,
	.set_msg_header		= &tcpci_tcpm_set_msg_header,
	.set_rx_enable		= &tcpci_tcpm_set_rx_enable,
	.get_message_raw	= &tcpci_tcpm_get_message_raw,
	.transmit		= &tcpci_tcpm_transmit,
	.tcpc_alert		= &tcpci_tcpc_alert,
#ifdef CONFIG_USB_PD_DECODE_SOP
	.sop_prime_enable	= &tcpci_tcpm_sop_prime_enable,
#endif
#ifdef CONFIG_USB_PD_DISCHARGE_TCPC
	.tcpc_discharge_vbus	= &tcpci_tcpc_discharge_vbus,
#endif
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
	.drp_toggle		= &tcpci_tcpc_drp_toggle,
#endif
	.get_chip_info		= &tcpci_get_chip_info,
#ifdef CONFIG_USBC_PPC
	.set_snk_ctrl		= &tcpci_tcpm_set_snk_ctrl,
	.set_src_ctrl		= &tcpci_tcpm_set_src_ctrl,
#endif
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
	.enter_low_power_mode	= &tcpci_enter_low_power_mode,
#endif
	.set_bist_test_mode	= &tcpci_set_bist_test_mode,
	.tcpc_enable_auto_discharge_disconnect =
		&tcpci_tcpc_enable_auto_discharge_disconnect,
	.set_bist_test_mode	= &tcpci_set_bist_test_mode,
#ifdef CONFIG_CMD_TCPC_DUMP
	.dump_registers		= &tcpc_dump_std_registers,
#endif
};

const struct ppc_drv ccgxxf_ppc_drv = {
	.vbus_sink_enable = &tcpci_tcpm_set_snk_ctrl, /*ccgxxf_vbus_sink_enable,*/
	.vbus_source_enable = &tcpci_tcpm_set_src_ctrl, /* ccgxxf_vbus_source_enable, */
};
