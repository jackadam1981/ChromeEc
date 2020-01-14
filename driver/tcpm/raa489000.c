/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Renesas RAA489000 TCPC driver
 */

#include "common.h"
#include "console.h"
#include "driver/charger/isl923x.h"
#include "i2c.h"
#include "raa489000.h"
#include "tcpci.h"
#include "tcpm.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

int raa489000_init(int port)
{
	int rv;
	int regval;
	int i2c_port;

	/* Perform unlock sequence */
	rv = tcpc_write16(port, 0xAA, 0xDAA0);
	if (rv)
		CPRINTS("c%d: failed unlock step1", port);
	rv = tcpc_write16(port, 0xAA, 0xACE0);
	if (rv)
		CPRINTS("c%d: failed unlock step2", port);
	rv = tcpc_write16(port, 0xAA, 0x0D0B);
	if (rv)
		CPRINTS("c%d: failed unlock step3", port);

	/* Note: registers may not be ready until TCPCI init succeeds */
	rv = tcpci_tcpm_init(port);
	if (rv)
		return rv;

	/*
	 * Set some vendor defined registers to enable the CC comparators and
	 * remove the dead battery resistors.
	 */
	rv = tcpc_write16(port, RAA489000_TYPEC_SETTING1,
		     RAA489000_SETTING1_RDOE | RAA489000_SETTING1_CC2_CMP3_EN |
		     RAA489000_SETTING1_CC2_CMP2_EN |
		     RAA489000_SETTING1_CC2_CMP1_EN |
		     RAA489000_SETTING1_CC1_CMP3_EN |
		     RAA489000_SETTING1_CC1_CMP2_EN |
		     RAA489000_SETTING1_CC1_CMP1_EN |
		     RAA489000_SETTING1_CC_DB_EN);
	if (rv)
		CPRINTS("failed to enable CC comparators");

	/* Enable the ADC */
	i2c_port = tcpc_config[port].i2c_info.port;
	rv = i2c_read16(i2c_port, ISL923X_ADDR_FLAGS, ISL9238_REG_CONTROL3,
			&regval);
	regval |= RAA489000_ENABLE_ADC;
	rv |= i2c_write16(i2c_port, ISL923X_ADDR_FLAGS, ISL9238_REG_CONTROL3,
			  regval);
	if (rv)
		CPRINTS("failed to enable ADCs");

	/* Enable Vbus detection */
	rv = tcpc_write(port, TCPC_REG_COMMAND,
			TCPC_REG_COMMAND_ENABLE_VBUS_DETECT);
	if (rv)
		CPRINTS("failed to enable vbus detect cmd");

	return rv;
}

int raa489000_tcpm_set_cc(int port, int pull)
{
	int rv;

	rv = tcpci_tcpm_set_cc(port, pull);
	if (rv)
		return rv;

	/* TCPM should set RDOE to 1 after setting Rp */
	if (pull == TYPEC_CC_RP)
		rv = tcpc_update16(port, RAA489000_TYPEC_SETTING1,
				   RAA489000_SETTING1_RDOE, MASK_SET);

	return rv;
}

int raa489000_tcpm_set_rx_enable(int port, int enable)
{
	int regval;
	int rv;

	rv = tcpci_tcpm_set_rx_enable(port, enable);
	if (rv)
		return rv;

	/* Set Rx enable for receiver comparator */
	tcpc_read16(port, 0xE0, &regval);
	regval |= BIT(9) | BIT(8);
	return tcpc_write16(port, 0xE0, regval);
}

/* RAA489000 is a TCPCI compatible port controller */
const struct tcpm_drv raa489000_tcpm_drv = {
	.init                   = &raa489000_init,
	.release                = &tcpci_tcpm_release,
	.get_cc                 = &tcpci_tcpm_get_cc,
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	.get_vbus_level         = &tcpci_tcpm_get_vbus_level,
#endif
	.select_rp_value        = &tcpci_tcpm_select_rp_value,
	.set_cc                 = &raa489000_tcpm_set_cc,
	.set_polarity           = &tcpci_tcpm_set_polarity,
	.set_vconn              = &tcpci_tcpm_set_vconn,
	.set_msg_header         = &tcpci_tcpm_set_msg_header,
	.set_rx_enable          = &raa489000_tcpm_set_rx_enable,
	.get_message_raw        = &tcpci_tcpm_get_message_raw,
	.transmit               = &tcpci_tcpm_transmit,
	.tcpc_alert             = &tcpci_tcpc_alert,
#ifdef CONFIG_USB_PD_DISCHARGE_TCPC
	.tcpc_discharge_vbus    = &tcpci_tcpc_discharge_vbus,
#endif
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
	.drp_toggle             = &tcpci_tcpc_drp_toggle,
#endif
	.get_chip_info          = &tcpci_get_chip_info,
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
	.enter_low_power_mode   = &tcpci_enter_low_power_mode,
#endif
};
