/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(tcpci_faulty_snk_emul, CONFIG_TCPCI_EMUL_LOG_LEVEL);

#include <sys/byteorder.h>
#include <zephyr.h>

#include "common.h"
#include "emul/tcpc/emul_tcpci.h"
#include "emul/tcpc/emul_tcpci_partner_common.h"
#include "emul/tcpc/emul_tcpci_partner_faulty_snk.h"
#include "emul/tcpc/emul_tcpci_partner_snk.h"
#include "usb_pd.h"

/**
 * @brief Reduce number of times to repeat action. If count reaches zero, action
 *        is removed from queue.
 *
 * @param data Pointer to USB-C malfunctioning sink device emulator data
 */
static void tcpci_faulty_snk_emul_reduce_action_count(
	struct tcpci_faulty_snk_emul_data *data)
{
	struct tcpci_faulty_snk_action *action;

	action = k_fifo_peek_head(&data->action_list);

	if (action->count == TCPCI_FAULTY_SNK_INFINITE_ACTION) {
		return;
	}

	action->count--;
	if (action->count != 0) {
		return;
	}

	/* Remove action from queue */
	k_fifo_get(&data->action_list, K_FOREVER);
}

/** Check description in emul_tcpci_partner_faulty_snk.h */
enum tcpci_partner_handler_res tcpci_faulty_snk_emul_handle_sop_msg(
	struct tcpci_partner_extension *ext,
	struct tcpci_partner_data *common_data,
	const struct tcpci_emul_msg *msg)
{
	struct tcpci_faulty_snk_emul_data *data =
		CONTAINER_OF(ext, struct tcpci_faulty_snk_emul_data, ext);
	struct tcpci_faulty_snk_action *action;
	uint16_t header;

	action = k_fifo_peek_head(&data->action_list);
	header = sys_get_le16(msg->buf);

	if (action == NULL) {
		/* No faulty action, so send GoodCRC */
		tcpci_emul_partner_msg_status(common_data->tcpci_emul,
					      TCPCI_EMUL_TX_SUCCESS);

		return TCPCI_PARTNER_COMMON_MSG_NOT_HANDLED;
	}

	if (PD_HEADER_CNT(header)) {
		/* Handle data message */
		switch (PD_HEADER_TYPE(header)) {
		case PD_DATA_SOURCE_CAP:
			if (action->action_mask &
			    TCPCI_FAULTY_SNK_FAIL_SRC_CAP) {
				/* Fail is not sending GoodCRC from partner */
				tcpci_emul_partner_msg_status(
						common_data->tcpci_emul,
						TCPCI_EMUL_TX_FAILED);
				tcpci_faulty_snk_emul_reduce_action_count(data);
				return TCPCI_PARTNER_COMMON_MSG_HANDLED;
			}
			if (action->action_mask &
			    TCPCI_FAULTY_SNK_DISCARD_SRC_CAP) {
				/* Discard because partner is sending message */
				tcpci_emul_partner_msg_status(
						common_data->tcpci_emul,
						TCPCI_EMUL_TX_DISCARDED);
				tcpci_partner_send_control_msg(
						&faulty_snk_emul->common_data,
						PD_CTRL_ACCEPT, 0);
				tcpci_faulty_snk_emul_reduce_action_count(data);
				return TCPCI_PARTNER_COMMON_MSG_HANDLED;
			}
			if (action->action_mask &
			    TCPCI_FAULTY_SNK_IGNORE_SRC_CAP) {
				/* Send only GoodCRC */
				tcpci_emul_partner_msg_status(
						common_data->tcpci_emul,
						TCPCI_EMUL_TX_SUCCESS);
				tcpci_faulty_snk_emul_reduce_action_count(data);
				return TCPCI_PARTNER_COMMON_MSG_HANDLED;
			}
		}
	}

	/* Send GoodCRC for all unhandled messages, since we disabled it in common handler */
	tcpci_emul_partner_msg_status(common_data->tcpci_emul,
				      TCPCI_EMUL_TX_SUCCESS);

	return TCPCI_PARTNER_COMMON_MSG_NOT_HANDLED;
}

/** Check description in emul_tcpci_partner_faulty_snk.h */
void tcpci_faulty_snk_emul_init_data(struct tcpci_partner_extension *ext,
			      struct tcpci_partner_data *common_data)
{
	struct tcpci_faulty_snk_emul_data *data =
		CONTAINER_OF(ext, struct tcpci_faulty_snk_emul_data, ext);

	k_fifo_init(&data->action_list);
}

struct tcpci_partner_extension_ops tcpci_faulty_snk_emul_ops = {
	.sop_msg_handler = tcpci_faulty_snk_emul_handle_sop_msg,
	.hard_reset = NULL,
	.soft_reset = NULL,
	.disconnect = NULL,
	.connect = NULL,
	.init = tcpci_faulty_snk_emul_init_data,
}
