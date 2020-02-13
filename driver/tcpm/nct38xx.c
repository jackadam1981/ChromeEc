/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Type-C port manager for Nuvoton NCT38XX. */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "ioexpander_nct38xx.h"
#include "nct38xx.h"
#include "task.h"
#include "tcpci.h"

#if !defined(CONFIG_USB_PD_TCPM_TCPCI)
#error "NCT38XX is using part of standard TCPCI control"
#error "Please upgrade your board configuration"
#endif

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

static unsigned char txBuf[33];
static unsigned char rxBuf[33];

static int nct38xx_tcpm_init(int port)
{
	int rv = 0;
	int reg;

	rv = tcpci_tcpm_init(port);
		if (rv)
			return rv;

	/*
	 * Write to the CONTROL_OUT_EN register to enable:
	 * [6] - CONNDIREN : Connector direction indication output enable
	 * [2] - SNKEN     : VBUS sink enable output enable
	 * [0] - SRCEN     : VBUS source voltage enable output enable
	 */
	reg = NCT38XX_REG_CTRL_OUT_EN_SRCEN |
			NCT38XX_REG_CTRL_OUT_EN_SNKEN |
			NCT38XX_REG_CTRL_OUT_EN_CONNDIREN;

	rv = tcpc_write(port, NCT38XX_REG_CTRL_OUT_EN, reg);
	if (rv)
		return rv;

	/* Disable OVP */
	rv = tcpc_update8(port,
			  TCPC_REG_FAULT_CTRL,
			  TCPC_REG_FAULT_CTRL_VBUS_OVP_FAULT_DIS,
			  MASK_SET);
	if (rv)
		return rv;

	/* Enable VBus monitor and Disable FRS */
	rv = tcpc_update8(port,
			  TCPC_REG_POWER_CTRL,
			  (TCPC_REG_POWER_CTRL_VBUS_VOL_MONITOR_DIS |
			   TCPC_REG_POWER_CTRL_FRS_ENABLE),
			  MASK_CLR);
	if (rv)
		return rv;

	/* Set FRS direction for SNK detect, if FRS is enabled */
	if (IS_ENABLED(CONFIG_USB_TYPEC_PD_FAST_ROLE_SWAP)) {
		reg = TCPC_REG_DEV_CAP_2_SNK_FR_SWAP;
		rv = tcpc_write(port, TCPC_REG_DEV_CAP_2, reg);
		if (rv)
			return rv;

		reg = TCPC_REG_CONFIG_EXT_1_FR_SWAP_SNK_DIR;
		rv = tcpc_write(port, TCPC_REG_CONFIG_EXT_1, reg);
		if (rv)
			return rv;
	}

	/* Start VBus monitor */
	rv = tcpc_write(port, TCPC_REG_COMMAND,
			TCPC_REG_COMMAND_ENABLE_VBUS_DETECT);

	/*
	 * Enable the Vendor Define alert event only when the IO expander
	 * feature is defined
	 */
	if (IS_ENABLED(CONFIG_IO_EXPANDER_NCT38XX))
		rv |= tcpc_update16(port,
				    TCPC_REG_ALERT_MASK,
				    TCPC_REG_ALERT_VENDOR_DEF,
				    MASK_SET);

	return rv;
}

int tcpci_nct38xx_transmit(int port, enum tcpm_transmit_type type,
			uint16_t header, const uint32_t *data)
{
	int rv, num_obj_byte;

	num_obj_byte = PD_HEADER_CNT(header) * 4;

	txBuf[0] = num_obj_byte + 2;

	txBuf[1] = (unsigned char)(header & 0xFF);
	txBuf[2] = (unsigned char)((header >> 8) & 0xFF);

	if (num_obj_byte) {
		uint32_t *buf_ptr;

		buf_ptr = (uint32_t *)&txBuf[3];
		memcpy(buf_ptr, data, num_obj_byte);
	}

	/* total write size = size header (1 byte) + message size */
	rv = tcpc_write_block(port, TCPC_REG_TX_BYTE_CNT,
					(const uint8_t *)txBuf, txBuf[0] + 1);

	rv = tcpc_write(port, TCPC_REG_TRANSMIT,
			TCPC_REG_TRANSMIT_SET_WITH_RETRY(type));
	return rv;
}

static int tcpci_nct38xx_get_message_raw(int port, uint32_t *payload, int *head)
{
	int rv, cnt, num_obj_byte;

	rv = tcpc_read(port, TCPC_REG_RX_BYTE_CNT, &cnt);

	if (rv != EC_SUCCESS || cnt < 3) {
		rv = EC_ERROR_UNKNOWN;
		goto clear;
	}

	rv = tcpc_read_block(port, TCPC_REG_RX_BYTE_CNT, rxBuf, cnt + 1);
	if (rv != EC_SUCCESS)
		goto clear;

	*head = *(int *)&rxBuf[2];
	num_obj_byte = PD_HEADER_CNT(*head) * 4;

	if (num_obj_byte) {
		uint32_t *buf_ptr;

		buf_ptr = (uint32_t *)&rxBuf[4];
		memcpy(payload, buf_ptr, num_obj_byte);
	}

clear:
	/* Read complete, clear RX status alert bit */
	tcpc_write16(port, TCPC_REG_ALERT, TCPC_REG_ALERT_RX_STATUS);

	return rv;
}

static void nct38xx_tcpc_alert(int port)
{
	int alert, rv;

	/*
	 * If IO expander feature is defined, read the ALERT register first to
	 * keep the status of Vendor Define bit. Otherwise, the status of ALERT
	 * register will be cleared after tcpci_tcpc_alert() is executed.
	 */
	if (IS_ENABLED(CONFIG_IO_EXPANDER_NCT38XX))
		rv = tcpc_read16(port, TCPC_REG_ALERT, &alert);

	/* Process normal TCPC ALERT event and clear status */
	tcpci_tcpc_alert(port);

	/*
	 * If IO expander feature is defined, check the Vendor Define bit to
	 * handle the IOEX IO's interrupt event
	 */
	if (IS_ENABLED(CONFIG_IO_EXPANDER_NCT38XX))
		if (!rv && (alert & TCPC_REG_ALERT_VENDOR_DEF))
			nct38xx_ioex_event_handler(port);

}

#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
static void nct38xx_drp_toggle_connection(int port,
	enum tcpc_cc_voltage_status cc1,
	enum tcpc_cc_voltage_status cc2)
{
	int rv;
	int role;

	/* If the connection is OPEN, then no need to do more */
	if (cc_is_open(cc1, cc2))
		return;

	/* Get the ROLE CONTROL value */
	rv = tcpc_read(port, TCPC_REG_ROLE_CTRL, &role);
	if (rv) {
		CPRINTS("C%d: %s failed to read ROLE",
			port, __func__);
		return;
	}

	/*
	 * If DRP is set, then we have a prospective connection.
	 * This chip will go back to an OPEN connection if we
	 * don't leave DRP with the state it suggests is right.
	 * So write out the ROLE to clear DRP and everything will
	 * look the same as if the DRP mode and prospective
	 * connection never happened and we will be in the mode that
	 * the auto-toggle said we should be in.
	 *
	 * Now if the PD stack wants to TRY a different connection
	 * we will no longer just go OPEN.
	 */
	if (role & TCPC_REG_ROLE_CTRL_DRP_MASK) {
		rv = tcpc_write(port, TCPC_REG_ROLE_CTRL,
				TCPC_REG_ROLE_CTRL_SET(0,
						tcpci_get_cached_rp(port),
						cc1, cc2));
		if (rv) {
			CPRINTS("C%d: %s failed to write ROLE",
				port, __func__);
			return;
		}
	}
}
#endif

const struct tcpm_drv nct38xx_tcpm_drv = {
	.init			= &nct38xx_tcpm_init,
	.release		= &tcpci_tcpm_release,
	.get_cc			= &tcpci_tcpm_get_cc,
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	.get_vbus_level		= &tcpci_tcpm_get_vbus_level,
#endif
	.select_rp_value	= &tcpci_tcpm_select_rp_value,
	.set_cc			= &tcpci_tcpm_set_cc,
	.set_polarity		= &tcpci_tcpm_set_polarity,
	.set_vconn		= &tcpci_tcpm_set_vconn,
	.set_msg_header		= &tcpci_tcpm_set_msg_header,
	.set_rx_enable		= &tcpci_tcpm_set_rx_enable,
	.get_message_raw	= &tcpci_nct38xx_get_message_raw,
	.transmit		= &tcpci_nct38xx_transmit,
	.tcpc_alert		= &nct38xx_tcpc_alert,
#ifdef CONFIG_USB_PD_DISCHARGE_TCPC
	.tcpc_discharge_vbus	= &tcpci_tcpc_discharge_vbus,
#endif
	.tcpc_enable_auto_discharge_disconnect =
				  &tcpci_tcpc_enable_auto_discharge_disconnect,
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
	.drp_toggle		= &tcpci_tcpc_drp_toggle,
	.drp_toggle_connection	= &nct38xx_drp_toggle_connection,
#endif
#ifdef CONFIG_USBC_PPC
	.set_snk_ctrl		= &tcpci_tcpm_set_snk_ctrl,
	.set_src_ctrl		= &tcpci_tcpm_set_src_ctrl,
#endif
	.get_chip_info		= &tcpci_get_chip_info,
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
	.enter_low_power_mode	= &tcpci_enter_low_power_mode,
#endif
#ifdef CONFIG_USB_TYPEC_PD_FAST_ROLE_SWAP
	.set_frs_enable         = &tcpci_tcpc_fast_role_swap_enable,
#endif
};
