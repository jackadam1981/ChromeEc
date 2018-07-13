/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MT6360 port manager */

#include "mt6370.h"
#include "console.h"
#include "ec_version.h"
#include "hooks.h"
#include "tcpci.h"
#include "tcpm.h"
#include "timer.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "util.h"
#include "task.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static int tcpc_vbus[CONFIG_USB_PD_PORT_COUNT];

static int tcpm_alert_mask_set(int port, uint16_t mask)
{
	/* write to the Alert Mask register */
	return tcpc_write16(port, TCPC_REG_ALERT_MASK, mask);
}

static int init_alert_mask(int port)
{
   	uint16_t mask;
	int rv;

	mask = TCPC_REG_ALERT_TX_SUCCESS | TCPC_REG_ALERT_TX_DISCARDED |
		TCPC_REG_ALERT_TX_FAILED | TCPC_REG_ALERT_RX_HARD_RST |
		TCPC_REG_ALERT_RX_STATUS | TCPC_REG_ALERT_CC_STATUS
#ifdef CONFIG_USB_PD_TCPM_VBUS
		| TCPC_REG_ALERT_POWER_STATUS
#endif
		;

	/* Set the alert mask in TCPC*/
	rv = tcpm_alert_mask_set(port, mask);

	return rv;
}

static int tcpm_set_power_status_mask(int port, uint8_t mask)
{
	/* write to the Alert Mask register */
	return tcpc_write(port, TCPC_REG_POWER_STATUS_MASK, mask);
}

static int init_power_status_mask(int port)
{
	uint8_t mask;

	mask = TCPC_REG_POWER_STATUS_VBUS_PRES;

	return tcpm_set_power_status_mask(port, mask);
}

int tcpm_get_power_status(int port, int *status)
{
	return tcpc_read(port, TCPC_REG_POWER_STATUS, status);
}

int tcpm_alert_status(int port, int *alert)
{
	/* Read TCPC Alert register */
	return tcpc_read16(port, TCPC_REG_ALERT, alert);
}

#ifdef CONFIG_USB_PD_TCPM_VBUS
int tcpm_get_vbus_level(int port)
{
	return tcpc_vbus[port];
}
#endif

void mt6370_tcpc_alert(int port)
{
	int status;
	int power_status;

	/* Read the Alert register from the TCPC */
	tcpm_alert_status(port, &status);

	/*
	 * Clear alert status for everything except RX_STATUS, which shouldn't
	 * be cleared until we have successfully retrieved message.
	 */
	if (status & ~TCPC_REG_ALERT_RX_STATUS) {
		tcpc_write16(port, TCPC_REG_ALERT, (TCPC_REG_ALERT_RX_STATUS
				| ~status));
	}

	if (status & TCPC_REG_ALERT_CC_STATUS) {
		/* CC status changed, wake task */
		task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_CC, 0);
	}
	if (status & TCPC_REG_ALERT_POWER_STATUS) {
		/* Read Power Status register */
		tcpm_get_power_status(port, &power_status);
		/* Update VBUS status */
		tcpc_vbus[port] = power_status &
			TCPC_REG_POWER_STATUS_VBUS_PRES ? 1 : 0;
	#if defined(CONFIG_USB_PD_TCPM_VBUS) && defined(CONFIG_USB_CHARGER)
		/* Update charge manager with new VBUS state */
		usb_charger_vbus_change(port, tcpc_vbus[port]);
	#endif /* CONFIG_USB_PD_TCPM_VBUS && CONFIG_USB_CHARGER */
		task_wake(PD_PORT_TO_TASK_ID(port));
	}
	if (status & TCPC_REG_ALERT_RX_STATUS) {
		/* message received */
		task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_RX, 0);
	}
	if (status & TCPC_REG_ALERT_RX_HARD_RST) {
		/* hard reset received */
		pd_execute_hard_reset(port);
		task_wake(PD_PORT_TO_TASK_ID(port));
	}
	if (status & TCPC_REG_ALERT_TX_COMPLETE) {
		/* transmit complete */
		pd_transmit_complete(port, status & TCPC_REG_ALERT_TX_SUCCESS
					? TCPC_TX_COMPLETE_SUCCESS
					: TCPC_TX_COMPLETE_FAILED);
	}
}

int mt6370_init(int port)
{
	int rv;
	int power_status;

	rv = tcpc_write(port, MT6370_REG_SWRESET, 1);
	if (rv != EC_SUCCESS) return rv;

	msleep(1);
	rv = tcpc_read(port, TCPC_REG_POWER_STATUS, &power_status);

	if(rv != EC_SUCCESS) {
		return rv;
	}

	/* AUTO IDLE off, shipping off, 300K from 320K, PD3.0 ext-msg on */
	rv = tcpc_write(port, MT6370_REG_IDLE_CTRL,
			MT6370_REG_IDLE_SET(0, 1, 0, 0));
	/* CC Detect Debounce -- 5 */
	rv |= tcpc_write(port, 0xA1, 5);
	/* DRP Duty */
	rv |= tcpc_write(port, MT6370_REG_DRP_TOGGLE_CYCLE, 4);
	rv |= tcpc_write16(port, MT6370_REG_DRP_DUTY_CTRL, 400);
	/* Vconn OC on */
	rv |= tcpc_write(port, MT6370_REG_VCONN_CLIMITEN, 1);
	/* PHY control */
	rv |= tcpc_write(port, MT6370_REG_PHY_CTRL1,
			MT6370_REG_PHY_CTRL1_SET(1, 7, 0, 1));
 	rv |= tcpc_write(port, MT6370_REG_PHY_CTRL3, 0x82);
	/* write 1 clear */
	rv |= tcpc_write16(port, TCPC_REG_ALERT, 0xffff);

	rv |= init_power_status_mask(port);

	if (rv== EC_SUCCESS) {
		tcpc_vbus[port] = (power_status &
					TCPC_REG_POWER_STATUS_VBUS_PRES)
					? 1 : 0;
	}

	return init_alert_mask(port);
}

static int mt6370_release(int port)
{
	return EC_SUCCESS;
}

/* MT6370 is a TCPCI compatible port controller */
const struct tcpm_drv mt6370_tcpm_drv = {
	.init			= &mt6370_init,
	.release		= &mt6370_release,
	.get_cc			= &tcpci_tcpm_get_cc,
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	.get_vbus_level		= &tcpm_get_vbus_level,
#endif
	.select_rp_value	= &tcpci_tcpm_select_rp_value,
	.set_cc			= &tcpci_tcpm_set_cc,
	.set_polarity		= &tcpci_tcpm_set_polarity,
	.set_vconn		= &tcpci_tcpm_set_vconn,
	.set_msg_header		= &tcpci_tcpm_set_msg_header,
	.set_rx_enable		= &tcpci_tcpm_set_rx_enable,
	.get_message		= &tcpci_tcpm_get_message,
	.transmit		= &tcpci_tcpm_transmit,
	.tcpc_alert		= &mt6370_tcpc_alert,
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
};

