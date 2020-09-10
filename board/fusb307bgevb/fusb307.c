/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Type-C port manager for Fairchild's FUSB307 */

#include "console.h"
#include "fusb307.h"
#include "task.h"
#include "hooks.h"
#include "tcpci.h"
#include "tcpm.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "usb_pd_tcpc.h"
#include "util.h"
#include "usb_common.h"
#include "lcd.h"
#include "printf.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

int fusb307_power_supply_reset(int port)
{
	/* return tcpc_write(port, TCPC_REG_COMMAND, 0x66); */
	return EC_SUCCESS;
}

const struct tcpm_drv fusb307_tcpm_drv = {
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
	.tcpc_enable_auto_discharge_disconnect =
				  &tcpci_tcpc_enable_auto_discharge_disconnect,
	.get_chip_info		= &tcpci_get_chip_info,
#if defined(CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE)
	.drp_toggle		= &tcpci_tcpc_drp_toggle,
#endif
};

