/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * MT6360 TCPC Driver
 */

#include "console.h"
#include "hooks.h"
#include "mt6360.h"
#include "task.h"
#include "tcpci.h"
#include "tcpm.h"
#include "timer.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static int mt6360_polarity;

/* i2c_write function which won't wake TCPC from low power mode. */
static int mt6360_i2c_write8(int port, int reg, int val)
{
	return i2c_write8(tcpc_config[port].i2c_info.port,
			  tcpc_config[port].i2c_info.addr_flags, reg, val);
}

static int mt6360_i2c_read8(int port, int reg, int *data)
{
	return i2c_read8(tcpc_config[port].i2c_info.port,
			 tcpc_config[port].i2c_info.addr_flags, reg, data);
}

static int mt6360_init_phy_ctrl(int port)
{
	int rv;

	/* Disable TX Discard and auto-retry method */
	rv = tcpc_write(port, MT6360_REG_PHY_CTRL1,
			  MT6360_REG_PHY_CTRL1_SET(0, 7, 0, 0));
	/* PHY CDR threshold */
	rv |= tcpc_write(port, MT6360_REG_PHY_CTRL2, 0x3A);
	/* Transition window count */
	rv |= tcpc_write(port, MT6360_REG_PHY_CTRL3, 0x82);
	/* BMC Decoder idle time, 164ns per steps */
	rv |= tcpc_write(port, MT6360_REG_PHY_CTRL7, 0x36);
	rv |= tcpc_write(port, MT6360_REG_PHY_CTRL11, 0x60);
	/* Retry period setting, 416ns per step */
	rv |= tcpc_write(port, MT6360_REG_PHY_CTRL12, 0x3C);
	rv |= tcpc_write(port, MT6360_REG_RX_CTRL1, 0xE8);

	return rv;
}

static int mt6360_init(int port)
{
	int rv, val;

	rv = tcpc_read(port, MT6360_REG_MODE_CTRL2, &val);

	/* Only do soft-reset in shipping mode. (b:122017882) */
	if (!(val & MT6360_SHIPPING_OFF)) {

		/* Software reset. */
		rv = tcpc_write(port, MT6360_REG_SWRESET, 1);
		if (rv)
			return rv;

		/* Need 1 ms for software reset. */
		msleep(1);
	}

	/* The earliest point that we can do generic init. */
	rv = tcpci_tcpm_init(port);

	if (rv)
		return rv;

	/*
	 * AUTO IDLE off, shipping off, select CK_300K from BICIO_320K,
	 * PD3.0 ext-msg on.
	 */
	rv = tcpc_write(port, MT6360_REG_MODE_CTRL2,
			MT6360_REG_MODE_CTRL2_SET(1, 0, 2));
	/* CC Detect Debounce 5 */
	rv |= tcpc_write(port, MT6360_REG_DEBOUNCE_CTRL1, 10);
	/* DRP Duty */
	rv |= tcpc_write(port, MT6360_REG_DRP_CTRL1, 4);
	rv |= tcpc_write16(port, MT6360_REG_DRP_CTRL2, 400);
	/* Vconn OC on */
	rv |= tcpc_write(port, MT6360_REG_VCONN_CTRL1, 0x41);
	/* PHY control */
	rv |= mt6360_init_phy_ctrl(port);

	return rv;
}

static int mt6360_get_cc(int port, enum tcpc_cc_voltage_status *cc1,
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
	 * MT6370 TCPC follows USB PD 1.0 protocol. When DRP not auto-toggling,
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

	return rv;
}

static int mt6360_set_cc(int port, int pull)
{
	return tcpci_tcpm_set_cc(port, pull);
}

#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
static int mt6360_enter_low_power_mode(int port)
{
	int rv;

	/* VBUS_DET_EN for detecting charger plug. */
	rv = tcpc_write(port, MT6360_REG_MODE_CTRL3,
			MT6360_LPWR_EN | MT6360_LPWR_LDO_EN |
			MT6360_VBUS_DET_EN | MT6360_PD_BG_EN |
			MT6360_PD_IREF_EN);

	if (rv)
		return rv;

	return tcpci_enter_low_power_mode(port);
}
#endif

static int mt6360_set_polarity(int port, enum tcpc_cc_polarity polarity)
{
	enum tcpc_cc_voltage_status cc1, cc2;

	mt6360_polarity = polarity;
	mt6360_get_cc(port, &cc1, &cc2);
	return tcpci_tcpm_set_polarity(port, polarity);
}

int mt6360_vconn_discharge(int port)
{
	int val;
	/*
	 * Write to mt6360 in low-power mode may return fail, but it is
	 * actually written. So we just ignore its return value.
	 */
	mt6360_i2c_read8(port, MT6360_REG_VCONN_CTRL3, &val);
	val &= ~MT6360_MASK_VCONN_LOAD_SEL;
	val |= (MT6360_VCONN_LOAD_LVL0 & MT6360_MASK_VCONN_LOAD_SEL);
	mt6360_i2c_write8(port, MT6360_REG_VCONN_CTRL3, val);
	/* Set MT6360_REG_DISCHARGE_EN bit and also the rest default value. */
	mt6360_i2c_write8(port, MT6360_REG_MODE_CTRL3,
			  MT6360_VCONN_DISCHARGE_EN |
				MT6360_REG_BMC_CTRL_DEFAULT);

	return EC_SUCCESS;
}

/* MT6370 is a TCPCI compatible port controller */
const struct tcpm_drv mt6360_tcpm_drv = {
	.init			= &mt6360_init,
	.release		= &tcpci_tcpm_release,
	.get_cc			= &mt6360_get_cc,
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	.get_vbus_level		= &tcpci_tcpm_get_vbus_level,
#endif
	.select_rp_value	= &tcpci_tcpm_select_rp_value,
	.set_cc			= &mt6360_set_cc,
	.set_polarity		= &mt6360_set_polarity,
	.set_vconn		= &tcpci_tcpm_set_vconn,
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
	.enter_low_power_mode	= &mt6360_enter_low_power_mode,
#endif
};
