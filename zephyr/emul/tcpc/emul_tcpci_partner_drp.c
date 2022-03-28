/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(tcpci_drp_emul, CONFIG_TCPCI_EMUL_LOG_LEVEL);

#include <sys/byteorder.h>
#include <zephyr.h>

#include "common.h"
#include "emul/tcpc/emul_tcpci.h"
#include "emul/tcpc/emul_tcpci_partner_common.h"
#include "emul/tcpc/emul_tcpci_partner_drp.h"
#include "emul/tcpc/emul_tcpci_partner_snk.h"
#include "emul/tcpc/emul_tcpci_partner_src.h"
#include "tcpm/tcpci.h"
#include "usb_pd.h"

/**
 * @brief Handle SOP messages as TCPCI dual role device
 *
 * @param ext Pointer to USB-C DRP device emulator extension
 * @param common_data Pointer to USB-C device emulator common data
 * @param msg Pointer to received message
 *
 * @return TCPCI_PARTNER_COMMON_MSG_HANDLED Message was handled
 * @return TCPCI_PARTNER_COMMON_MSG_NOT_HANDLED Message wasn't handled
 */
static enum tcpci_partner_handler_res tcpci_drp_emul_handle_sop_msg(
	struct tcpci_partner_extension *ext,
	struct tcpci_partner_data *common_data,
	const struct tcpci_emul_msg *msg)
{
	struct tcpci_drp_emul_data *data =
		CONTAINER_OF(ext, struct tcpci_drp_emul_data, ext);
	uint16_t pwr_status;
	uint16_t header;

	header = sys_get_le16(msg->buf);

	if (PD_HEADER_CNT(header)) {
		/* Handle data message */
		switch (PD_HEADER_TYPE(header)) {
		case PD_DATA_REQUEST:
			if (common_data->power_role == PD_ROLE_SINK) {
				/* As sink we shouldn't accept request */
				tcpci_partner_send_control_msg(common_data,
							       PD_CTRL_REJECT,
							       0);
				return TCPCI_PARTNER_COMMON_MSG_HANDLED;
			}
			/* As source, let source handler to handle this */
			return TCPCI_PARTNER_COMMON_MSG_NOT_HANDLED;
		case PD_DATA_SOURCE_CAP:
			if (common_data->power_role == PD_ROLE_SOURCE) {
				/* As source we shouldn't respond */
				return TCPCI_PARTNER_COMMON_MSG_HANDLED;
			}
			/* As sink, let sink handler to handle this */
			return TCPCI_PARTNER_COMMON_MSG_NOT_HANDLED;
		}
	} else {
		/* Handle control message */
		switch (PD_HEADER_TYPE(header)) {
		case PD_CTRL_PR_SWAP:
			tcpci_partner_send_control_msg(common_data,
						       PD_CTRL_ACCEPT,
						       0);
			data->in_pwr_swap = true;
			return TCPCI_PARTNER_COMMON_MSG_HANDLED;
		case PD_CTRL_PS_RDY:
			if (!data->in_pwr_swap) {
				return TCPCI_PARTNER_COMMON_MSG_NOT_HANDLED;
			}
			data->in_pwr_swap = false;

			/* Reset counters */
			common_data->msg_id = 0;
			common_data->recv_msg_id = -1;

			/* Perform power role swap */
			if (common_data->power_role == PD_ROLE_SOURCE) {
				/* Disable VBUS if emulator was source */
				tcpci_emul_get_reg(common_data->tcpci_emul,
						   TCPC_REG_POWER_STATUS,
						   &pwr_status);
				pwr_status &= ~TCPC_REG_POWER_STATUS_VBUS_PRES;
				tcpci_emul_set_reg(common_data->tcpci_emul,
						   TCPC_REG_POWER_STATUS,
						   pwr_status);
				/* Reconnect as sink */
				common_data->power_role = PD_ROLE_SINK;
			} else {
				/* Reconnect as source */
				common_data->power_role = PD_ROLE_SOURCE;
			}
			tcpci_partner_send_control_msg(common_data,
						       PD_CTRL_PS_RDY, 0);
			/* Reconnect to TCPCI emulator */
			tcpci_partner_connect_to_tcpci(common_data,
						       common_data->tcpci_emul);

			return TCPCI_PARTNER_COMMON_MSG_HANDLED;
		}
	}

	return TCPCI_PARTNER_COMMON_MSG_NOT_HANDLED;
}

/**
 * @brief Initialise USB-C DRP device data structure
 *
 * @param ext Pointer to USB-C DRP device emulator extension
 * @param common_data Pointer to USB-C device emulator common data
 */
static void tcpci_drp_emul_init_data(struct tcpci_partner_extension *ext,
				     struct tcpci_partner_data *common_data)
{
	struct tcpci_drp_emul_data *data =
		CONTAINER_OF(ext, struct tcpci_drp_emul_data, ext);

	data->in_pwr_swap = false;
}

/** Check description in emul_tcpci_partner_drp.h */
void tcpci_drp_emul_set_dr_in_first_pdo(uint32_t *pdo)
{
	*pdo |= PDO_FIXED_DUAL_ROLE;
}

/** USB-C DRP device extension callbacks */
struct tcpci_partner_extension_ops tcpci_drp_emul_ops = {
	.sop_msg_handler = tcpci_drp_emul_handle_sop_msg,
	.hard_reset = NULL,
	.soft_reset = NULL,
	.disconnect = NULL,
	.connect = NULL,
	.init = tcpci_drp_emul_init_data,
};
