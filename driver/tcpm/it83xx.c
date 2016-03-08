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
	chip_pd_rx_data(port, NULL, head, payload);
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
		status = chip_pd_tx_data(port,
					USBPD_SOP_TYPE_SOP,
					PD_HEADER_TYPE(header),
					PD_HEADER_CNT(header),
					data);
		break;
	case TCPC_TX_BIST_MODE_2:
		chip_pd_send_bist_mode2_pattern(port);
		status = TCPC_TX_COMPLETE_SUCCESS;
		break;
	case TCPC_TX_HARD_RESET:
		status = chip_pd_send_hw_reset(port, USBPD_RESET_TYPE_HARD);
		break;
	default:
		status = TCPC_TX_COMPLETE_FAILED;
		break;
	}
	pd_transmit_complete(port, status);

	return EC_SUCCESS;
}
