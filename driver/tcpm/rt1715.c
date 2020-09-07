/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * RT1715 TCPC Driver
 */

#include "console.h"
#include "hooks.h"
#include "rt1715.h"
#include "task.h"
#include "tcpci.h"
#include "tcpm.h"
#include "timer.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static int rt1715_polarity;

static int rt1715_init(int port)
{
	int rv, val;

	rv = tcpc_read(port, RT1715_REG_IDLE_CTRL, &val);

	/* Only do soft-reset in shipping mode. (b:122017882) */
	if (!(val & RT1715_REG_SHIPPING_OFF)) {

		/* Software reset. */
		rv = tcpc_write(port, RT1715_REG_SWRESET, 1);
		if (rv)
			return rv;

		/* Need 5 ms for software reset. */
		msleep(5);
	}

	/* The earliest point that we can do generic init. */
	rv = tcpci_tcpm_init(port);

	if (rv)
		return rv;

	/*
	 * AUTO IDLE off, shipping off, select CK_300K from BICIO_320K,
	 * PD3.0 ext-msg on.
	 */
	rv = tcpc_write(port, RT1715_REG_IDLE_CTRL,
			RT1715_REG_IDLE_SET(0, 1, 0, 0));
					
	/* I2C reset : (val + 1) * 12.5ms */
	rv |= tcpc_write(port, RT1715_REG_I2CRST_CTRL,
            RT1715_REG_I2CRST_SET(1, 0x0F));	
						
	/* tTCPCfilter : (26.7 * val) us */
	rv |= tcpc_write(port, RT1715_REG_TTCPC_FILTER, 0x0F);
	/* DRP Duty : (51.2 + 6.4 * val) ms */
	rv |= tcpc_write(port, RT1715_REG_DRP_TOGGLE_CYCLE, 0x04);
	/* dcSRC.DRP : 40% */
	rv |= tcpc_write16(port, RT1715_REG_DRP_DUTY_CTRL, 400);
	
	/* Vconn OC on */
	rv |= tcpc_write(port, RT1715_REG_VCONN_CLIMITEN, 1);
	/* PHY control */
	rv |= tcpc_write(port, RT1715_REG_PHY_CTRL1,
			 RT1715_REG_PHY_CTRL1_SET(0, 7, 0, 1));
			 
	return rv;
}

static inline int rt1715_init_cc_params(int port, int cc_res)
{
	int rv, en, sel;

	if (cc_res == TYPEC_CC_VOLT_RP_DEF) { /* RXCC threshold : 0.55V */
		en = 0;
		sel = RT1715_OCCTRL_600MA | RT1715_MASK_BMCIO_RXDZSEL;
	} else { /* RD threshold : 0.35V & RP threshold : 0.75V */
		en = 1;
		sel = RT1715_OCCTRL_600MA | RT1715_MASK_BMCIO_RXDZSEL;
	}
	rv = tcpc_write(port, RT1715_REG_BMCIO_RXDZEN, en);
	if (!rv)
		rv = tcpc_write(port, RT1715_REG_BMCIO_RXDZSEL, sel);
	return rv;
}

static int rt1715_get_cc(int port, enum tcpc_cc_voltage_status *cc1,
	enum tcpc_cc_voltage_status *cc2)
{
	int status;
	int rv;
	int role, is_snk;

	rv = tcpc_read(port, TCPC_REG_CC_STATUS, &status);

	/* If tcpc read fails, return error and CC as open */
	if (rv) {
		*cc1 = TYPEC_CC_VOLT_OPEN;
		*cc2 = TYPEC_CC_VOLT_OPEN;
		return rv;
	}

	*cc1 = TCPC_REG_CC_STATUS_CC1(status);
	*cc2 = TCPC_REG_CC_STATUS_CC2(status);

	/*
	 * If status is not open, then OR in termination to convert to
	 * enum tcpc_cc_voltage_status.
	 *
	 * RT1715 TCPC follows USB PD 1.0 protocol. When DRP not auto-toggling,
	 * it will not update the DRP_RESULT bits in TCPC_REG_CC_STATUS,
	 * instead, we should check CC1/CC2 bits in TCPC_REG_ROLE_CTRL.
	 */
	rv = tcpc_read(port, TCPC_REG_ROLE_CTRL, &role);

	if (TCPC_REG_ROLE_CTRL_DRP(role))
		is_snk = TCPC_REG_CC_STATUS_TERM(status);
	else
		/* CC1/CC2 states are the same, checking one-side is enough. */
		is_snk = TCPC_REG_CC_STATUS_CC1(role) == TYPEC_CC_RD;

	if (is_snk) {
		if (*cc1 != TYPEC_CC_VOLT_OPEN)
			*cc1 |= 0x04;
		if (*cc2 != TYPEC_CC_VOLT_OPEN)
			*cc2 |= 0x04;
	}

	rv = rt1715_init_cc_params(port, (int)rt1715_polarity ? *cc1 : *cc2);
	return rv;
}

static int rt1715_set_cc(int port, int pull)
{
	if (pull == TYPEC_CC_RD)
		rt1715_init_cc_params(port, TYPEC_CC_VOLT_RP_DEF);
	return tcpci_tcpm_set_cc(port, pull);
}

#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
static int rt1715_enter_low_power_mode(int port)
{
	int rv;

	/* VBUS_DET_EN for detecting charger plug. */
	rv = tcpc_write(port, RT1715_REG_BMC_CTRL,
			RT1715_REG_BMCIO_LPEN | RT1715_REG_VBUS_DET_EN);

	if (rv)
		return rv;

	return tcpci_enter_low_power_mode(port);
}
#endif

static int rt1715_set_polarity(int port, enum tcpc_cc_polarity polarity)
{
	enum tcpc_cc_voltage_status cc1, cc2;

	rt1715_polarity = polarity;
	rt1715_get_cc(port, &cc1, &cc2);
	return tcpci_tcpm_set_polarity(port, polarity);
}

static int rt1715_set_vconn(int port, int enable)
{
	int rv;
	
	rv = tcpci_tcpm_set_vconn(port,enable);
	
	if (rv)
		return rv;
	
	return tcpc_write(port, RT1715_REG_IDLE_CTRL,
			RT1715_REG_IDLE_SET(0, 1, 0, 0));
}

/* RT1715 is a TCPCI compatible port controller */
const struct tcpm_drv rt1715_tcpm_drv = {
	.init			= &rt1715_init,
	.release		= &tcpci_tcpm_release,
	.get_cc			= &rt1715_get_cc,
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	.check_vbus_level	= &tcpci_tcpm_check_vbus_level,
#endif
	.select_rp_value	= &tcpci_tcpm_select_rp_value,
	.set_cc			= &rt1715_set_cc,
	.set_polarity		= &rt1715_set_polarity,
	.set_vconn		= &rt1715_set_vconn,
	.set_msg_header		= &tcpci_tcpm_set_msg_header,
	.set_rx_enable		= &tcpci_tcpm_set_rx_enable,
	.get_message_raw	= &tcpci_tcpm_get_message_raw,
	.transmit		= &tcpci_tcpm_transmit,
	.tcpc_alert		= &tcpci_tcpc_alert,
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
	.enter_low_power_mode	= &rt1715_enter_low_power_mode,
#endif
};
