/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TCPM for MCU also running TCPC */

#include "common.h"
#include "config.h"
#include "console.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"
#include "usb_pd_phy_chip.h"
#include "usb_pd_tcpm.h"

static const char * const typec_cc_volt[8] = {
	"TYPEC_CC_VOLT_OPEN",
	"TYPEC_CC_VOLT_RA",
	"TYPEC_CC_VOLT_RD",
	NULL,
	NULL,
	"TYPEC_CC_VOLT_SNK_DEF",
	"TYPEC_CC_VOLT_SNK_1_5",
	"TYPEC_CC_VOLT_SNK_3_0",
};

int tcpm_init(int port)
{
	board_pd_init(port, PD_ROLE_DEFAULT);
	/* Initialize physical layer */
	chip_pd_init(port, PD_ROLE_DEFAULT);

	return EC_SUCCESS;
}

int tcpm_get_cc(int port, int *cc1, int *cc2)
{
	*cc2 = chip_pd_get_cc(port, USBPD_CC_PIN_2);
	*cc1 = chip_pd_get_cc(port, USBPD_CC_PIN_1);

	return EC_SUCCESS;
}

int tcpm_set_cc(int port, int pull)
{
	chip_pd_set_cc(port, pull);

	return EC_SUCCESS;
}

int tcpm_set_polarity(int port, int polarity)
{
	chip_pd_select_polarity(port, polarity);

	return EC_SUCCESS;
}

int tcpm_set_vconn(int port, int enable)
{
#ifdef CONFIG_USBC_VCONN
	chip_pd_enable_vconn(port, enable);
	/* vconn switch */
	board_pd_vconn_ctrl(port,
		USBPD_GET_PULL_CC_SELECTION(port) ?
				USBPD_CC_PIN_2 :
				USBPD_CC_PIN_1, enable);
#endif

	return EC_SUCCESS;
}

int tcpm_set_msg_header(int port, int power_role, int data_role)
{
	chip_pd_set_power_role(port, power_role);
	chip_pd_set_data_role(port, data_role);

	return EC_SUCCESS;
}

int tcpm_set_rx_enable(int port, int enable)
{
	int i;

	if (enable) {
		IT83XX_USBPD_IMR(port) &= ~USBPD_REG_MASK_MSG_RX_DONE;
		USBPD_ENABLE_BMC_PHY(port);
	} else {
		IT83XX_USBPD_IMR(port) |= USBPD_REG_MASK_MSG_RX_DONE;
		USBPD_DISABLE_BMC_PHY(port);
	}

	/* If any PD port is connected, then disable deep sleep */
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; ++i)
		if (IT83XX_USBPD_GCR(port) | USBPD_REG_MASK_BMC_PHY)
			break;

	if (i == CONFIG_USB_PD_PORT_COUNT)
		enable_sleep(SLEEP_MASK_USB_PD);
	else
		disable_sleep(SLEEP_MASK_USB_PD);

	return EC_SUCCESS;
}

int tcpm_get_message(int port, uint32_t *payload, int *head)
{
	chip_pd_rx_data(port, head, payload);
	/* un-mask RX done interrupt */
	IT83XX_USBPD_IMR(port) &= ~USBPD_REG_MASK_MSG_RX_DONE;

	return EC_SUCCESS;
}

int tcpm_transmit(int port, enum tcpm_transmit_type type, uint16_t header,
		  const uint32_t *data)
{
	int status = TCPC_TX_COMPLETE_FAILED;

	switch (type) {
	case TCPC_TX_SOP:
	case TCPC_TX_SOP_PRIME:
	case TCPC_TX_SOP_PRIME_PRIME:
		status = chip_pd_tx_data(port,
					type,
					PD_HEADER_TYPE(header),
					PD_HEADER_CNT(header),
					data);
		break;
	case TCPC_TX_BIST_MODE_2:
		chip_pd_send_bist_mode2_pattern(port);
		status = TCPC_TX_COMPLETE_SUCCESS;
		break;
	case TCPC_TX_HARD_RESET:
	case TCPC_TX_CABLE_RESET:
		status = chip_pd_send_hw_reset(port, type);
		break;
	default:
		status = TCPC_TX_COMPLETE_FAILED;
		break;
	}
	pd_transmit_complete(port, status);

	return EC_SUCCESS;
}

static int command_tcpm_get_cc(int argc, char **argv)
{
	int cc1, cc2, i;

	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++) {
		tcpm_get_cc(i, &cc1, &cc2);
		ccprintf("P%x CC1:%s CC2:%s\n",
			i, typec_cc_volt[cc1], typec_cc_volt[cc2]);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(tccc, command_tcpm_get_cc,
			NULL,
			"To get typec cc voltage status",
			NULL);
