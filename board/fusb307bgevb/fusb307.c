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

static int fusb307_tcpm_init(int port)
{
	int rv;
	/*int rxdect;*/

	/* Tcpci init */
	rv = tcpci_tcpm_init(port);
	/* Set role ctrl */
	rv = tcpc_write(port, TCPC_REG_ROLE_CTRL, TCPC_REG_ROLE_CTRL_SET(1, 2, 2, 2));
	
	/*tcpc_read(port, TCPC_REG_ROLE_CTRL, &rxdect);
	CPRINTF("!!!!DRP Reg role_ctrl: 0x%02X\t", rxdect);*/
	pd_set_dual_role(port, PD_DRP_TOGGLE_ON);

	return rv;
}

static int cnt;
uint32_t source_caps[CONFIG_USB_PD_PORT_MAX_COUNT][PDO_MAX_OBJECTS];
static void lcd_print_deferred(void)
{
	int p;
	char c[20]; /* On LCD each row at most has 20 characters */
	uint32_t  ma, mv;

	for (p = 0; p < cnt; p++) {
		pd_extract_pdo_power(source_caps[0][p], &ma, &mv);
		CPRINTF("[%d] %dmV %dmA\t", p, mv, ma);

		snprintf(c, 20, "[%d] %dmV %dmA", p, mv, ma);
		if(p < 4) {
			lcd_setCursor(0, p);
			lcd_printString(c);
		}
	}

	/* Set selecter at end of 1st ros */
	lcd_setCursor(19, 0);
	lcd_printString("V");
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
		CPRINTS("Get Source Cap");
		cnt = PD_HEADER_CNT(*head);
		pd_process_source_cap(port, cnt, payload);
		for (i = 0; i < cnt; i++) {
			source_caps[port][i] = *payload++;
		}

		/*hook_call_deferred(&lcd_print_deferred_data, 100 * MSEC);*/
		CPRINTF("\n");	
	}

	return rv;
}

int fusb307_tcpm_set_polarity(int port, enum tcpc_cc_polarity polarity)
{
	int rv;
	enum tcpc_cc_voltage_status cc1, cc2;

	rv = tcpci_tcpm_set_polarity(port, polarity);

	tcpm_get_cc(port, &cc1, &cc2);
	if (cc1) {
		if (pd_get_power_role(port) == PD_ROLE_SINK) {
			int role = TCPC_REG_ROLE_CTRL_SET(0, tcpci_get_cached_rp(port),
							  TYPEC_CC_RD, TYPEC_CC_OPEN);
			tcpc_write(port, TCPC_REG_ROLE_CTRL, role);
			CPRINTS("!!!GET CC!!! Set cc1");
		}
	} else if (cc2) {
		if (pd_get_power_role(port) == PD_ROLE_SINK) {
			int role = TCPC_REG_ROLE_CTRL_SET(0, tcpci_get_cached_rp(port),
							  TYPEC_CC_OPEN, TYPEC_CC_RD);
			tcpc_write(port, TCPC_REG_ROLE_CTRL, role);
			CPRINTS("!!!GET CC!!! Set cc2");
		}
	} else {
		if (pd_get_power_role(port) == PD_ROLE_SINK) {
			tcpci_tcpm_set_cc(port, 0x2);
			CPRINTS("!!!GET CC!!! Set Rd/Rd");
		}
	}

	return rv;
}

int fusb307_tcpm_get_cc(int port, enum tcpc_cc_voltage_status *cc1,
	enum tcpc_cc_voltage_status *cc2)
{
	int rv;

	rv = tcpci_tcpm_get_cc(port, cc1, cc2);

	/* if (*cc2) {
		int role = TCPC_REG_ROLE_CTRL_SET(0, tcpci_get_cached_rp(port),
						  TYPEC_CC_OPEN, TYPEC_CC_RD);
		tcpc_write(port, TCPC_REG_ROLE_CTRL, role);
		CPRINTS("!!!GET CC!!! Set cc2");
	} else if (*cc1) {
		int role = TCPC_REG_ROLE_CTRL_SET(0, tcpci_get_cached_rp(port),
						  TYPEC_CC_RD, TYPEC_CC_OPEN);
		tcpc_write(port, TCPC_REG_ROLE_CTRL, role);
		CPRINTS("!!!GET CC!!! Set cc1");
	} else {
		tcpci_tcpm_set_cc(port, 0x2);
		CPRINTS("!!!GET CC!!! Set Rd/Rd");
	} */

	return rv;
}

const struct tcpm_drv fusb307_tcpm_drv = {
	.init			= &fusb307_tcpm_init,
	.release		= &tcpci_tcpm_release,
	.get_cc			= &tcpci_tcpm_get_cc,
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	.check_vbus_level	= &tcpci_tcpm_check_vbus_level,
#endif
	.select_rp_value	= &tcpci_tcpm_select_rp_value,
	.set_cc			= &tcpci_tcpm_set_cc,
	.set_polarity		= &fusb307_tcpm_set_polarity,
	.set_vconn		= &tcpci_tcpm_set_vconn,
	.set_msg_header		= &tcpci_tcpm_set_msg_header,
	.set_rx_enable		= &tcpci_tcpm_set_rx_enable,
	.get_message_raw	= &fusb307_tcpm_get_message_raw,
	.transmit		= &tcpci_tcpm_transmit,
	.tcpc_alert		= &tcpci_tcpc_alert,
	.tcpc_enable_auto_discharge_disconnect =
				  &tcpci_tcpc_enable_auto_discharge_disconnect,
	.get_chip_info		= &tcpci_get_chip_info,
#if defined(CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE)
	.drp_toggle		= &tcpci_tcpc_drp_toggle,
#endif
};

