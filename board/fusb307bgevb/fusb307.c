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


static int cnt;
static uint32_t src_caps[CONFIG_USB_PD_PORT_MAX_COUNT][PDO_MAX_OBJECTS];
static void lcd_print_deferred(void)
{
	int p;
	char c[20]; /* On LCD each row at most has 20 characters */
	uint32_t  ma, mv;

	for (p = 0; p < cnt; p++) {
		pd_extract_pdo_power(src_caps[0][p], &ma, &mv);
		CPRINTF("[%d] %dmV %dmA\t", p, mv, ma);

		snprintf(c, 20, "[%d] %dmV %dmA", p, mv, ma);
		if(p < 4) {
			lcd_setCursor(0, p);
			lcd_printString(c);
		}
	}
}
DECLARE_DEFERRED(lcd_print_deferred);

int fusb307_tcpm_get_message_raw(int port, uint32_t *payload, int *head)
{
	int rv;
	int type;
	int i;


	rv = tcpci_tcpm_get_message_raw(port, payload, head);

	type = PD_HEADER_TYPE(*head);
	if(type == PD_DATA_SOURCE_CAP) {
		cnt = PD_HEADER_CNT(*head);
		pd_process_source_cap(port, cnt, payload);
		for (i = 0; i < cnt; i++) {
			src_caps[port][i] = *payload++;
		}

		hook_call_deferred(&lcd_print_deferred_data, 100 * MSEC);
		CPRINTF("\n");	
	}

	return rv;
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
	.get_message_raw	= &fusb307_tcpm_get_message_raw,
	.transmit		= &tcpci_tcpm_transmit,
	.tcpc_alert		= &tcpci_tcpc_alert,
	.tcpc_enable_auto_discharge_disconnect =
				  &tcpci_tcpc_enable_auto_discharge_disconnect,
	.get_chip_info		= &tcpci_get_chip_info,
};
