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
 * @brief Handle PR_SWAP message
 *
 * @return enum tcpci_partner_handler_res
 */
static enum tcpci_partner_handler_res
tcpci_drp_emul_pr_swap_handler(struct tcpci_drp_emul_data *data,
				  struct tcpci_partner_data *common_data)
{
	tcpci_partner_send_control_msg(common_data, PD_CTRL_ACCEPT, 0);
	data->current_req = PD_CTRL_PR_SWAP;
	return TCPCI_PARTNER_COMMON_MSG_HANDLED;
}

static enum tcpci_partner_handler_res
tcpi_drp_emul_ps_rdy_pr_swap_handler(struct tcpci_drp_emul_data *data,
				     struct tcpci_src_emul_data *src_data,
				     struct tcpci_snk_emul_data *snk_data,
				     struct tcpci_partner_data *common_data,
				     const struct tcpci_emul_partner_ops *ops)
{
	uint16_t pwr_status;

	/* Reset counters */
	common_data->msg_id = 0;
	common_data->recv_msg_id = -1;

	/* Perform power role swap */
	if (!data->sink) {
		/* Disable VBUS if emulator was source
		 */
		tcpci_emul_get_reg(common_data->tcpci_emul,
				   TCPC_REG_POWER_STATUS, &pwr_status);
		pwr_status &= ~TCPC_REG_POWER_STATUS_VBUS_PRES;
		tcpci_emul_set_reg(common_data->tcpci_emul,
				   TCPC_REG_POWER_STATUS, pwr_status);
		/* Reconnect as sink */
		data->sink = true;
		common_data->power_role = PD_ROLE_SINK;
	} else {
		/* Reconnect as source */
		data->sink = false;
		common_data->power_role = PD_ROLE_SOURCE;
	}
	tcpci_partner_send_control_msg(common_data, PD_CTRL_PS_RDY, 0);
	/* Reconnect to TCPCI emulator */
	tcpci_drp_emul_connect_to_tcpci(data, src_data, snk_data, common_data,
					ops, common_data->tcpci_emul);

	data->current_req = PD_CTRL_INVALID;

	return TCPCI_PARTNER_COMMON_MSG_HANDLED;
}

static void tcpci_drp_emul_set_vconn(struct tcpci_partner_data *common_data,
				     int enable)
{
	uint16_t vconn_status;

	tcpci_emul_get_reg(common_data->tcpci_emul, TCPC_REG_POWER_CTRL,
			   &vconn_status);

	vconn_status &= ~TCPC_REG_POWER_CTRL_VCONN(1);
	vconn_status |= TCPC_REG_POWER_CTRL_VCONN(enable);

	tcpci_emul_set_reg(common_data->tcpci_emul, TCPC_REG_POWER_CTRL,
			   vconn_status);
}

/**
 * @brief Handle VCONN_SWAP message
 *
 * @return enum tcpci_partner_handler_res
 */
static enum tcpci_partner_handler_res
tcpci_drp_emul_vconn_swap_handler(struct tcpci_drp_emul_data *data,
				  struct tcpci_partner_data *common_data)
{
	data->current_req = PD_CTRL_VCONN_SWAP;

	tcpci_partner_send_control_msg(common_data, PD_CTRL_ACCEPT, 0);

	if (common_data->vconn_role == PD_ROLE_VCONN_OFF)
		tcpci_drp_emul_set_vconn(common_data, 1);

	/* PS ready after 15 ms */
	tcpci_partner_send_control_msg(common_data, PD_CTRL_PS_RDY, 15);
	return TCPCI_PARTNER_COMMON_MSG_HANDLED;
}

static enum tcpci_partner_handler_res tcpi_drp_emul_ps_rdy_vconn_swap_handler(
	struct tcpci_drp_emul_data *data, struct tcpci_src_emul_data *src_data,
	struct tcpci_snk_emul_data *snk_data,
	struct tcpci_partner_data *common_data,
	const struct tcpci_emul_partner_ops *ops)
{
	data->current_req = PD_CTRL_INVALID;

	if (common_data->vconn_role == PD_ROLE_VCONN_SRC)
		tcpci_drp_emul_set_vconn(common_data, 0);

	/* Update VCONN Role */
	common_data->vconn_role =
		(common_data->vconn_role == PD_ROLE_VCONN_SRC) ?
			PD_ROLE_VCONN_OFF :
			PD_ROLE_VCONN_SRC;

	return TCPCI_PARTNER_COMMON_MSG_HANDLED;
}

static enum tcpci_partner_handler_res
tcpi_drp_emul_ps_rdy_handler(struct tcpci_drp_emul_data *data,
			     struct tcpci_src_emul_data *src_data,
			     struct tcpci_snk_emul_data *snk_data,
			     struct tcpci_partner_data *common_data,
			     const struct tcpci_emul_partner_ops *ops)
{
	switch (data->current_req) {
	case PD_CTRL_PR_SWAP:
		return tcpi_drp_emul_ps_rdy_pr_swap_handler(
			data, src_data, snk_data, common_data, ops);

	case PD_CTRL_VCONN_SWAP:
		return tcpi_drp_emul_ps_rdy_vconn_swap_handler(
			data, src_data, snk_data, common_data, ops);

	case PD_CTRL_INVALID:
		return TCPCI_PARTNER_COMMON_MSG_NOT_HANDLED;

	default:
		LOG_ERR("Unhandled current_req=%u in PS_RDY",
			data->current_req);
		return TCPCI_PARTNER_COMMON_MSG_NOT_HANDLED;
	}
}

/** Check description in emul_tcpci_partner_drp.h */
enum tcpci_partner_handler_res
tcpci_drp_emul_handle_sop_msg(struct tcpci_drp_emul_data *data,
			      struct tcpci_src_emul_data *src_data,
			      struct tcpci_snk_emul_data *snk_data,
			      struct tcpci_partner_data *common_data,
			      const struct tcpci_emul_partner_ops *ops,
			      const struct tcpci_emul_msg *msg)
{
	uint16_t header;

	header = sys_get_le16(msg->buf);

	if (PD_HEADER_CNT(header)) {
		/* Handle data message */
		switch (PD_HEADER_TYPE(header)) {
		case PD_DATA_REQUEST:
			if (data->sink) {
				/* As sink we shouldn't accept request */
				tcpci_partner_send_control_msg(common_data,
							       PD_CTRL_REJECT,
							       0);
				return TCPCI_PARTNER_COMMON_MSG_HANDLED;
			}
			/* As source, let source handler to handle this */
			return TCPCI_PARTNER_COMMON_MSG_NOT_HANDLED;
		case PD_DATA_SOURCE_CAP:
			if (!data->sink) {
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
			return tcpci_drp_emul_pr_swap_handler(data,
							      common_data);

		case PD_CTRL_VCONN_SWAP:
			return tcpci_drp_emul_vconn_swap_handler(data,
								 common_data);

		case PD_CTRL_PS_RDY:
			return tcpi_drp_emul_ps_rdy_handler(
				data, src_data, snk_data, common_data, ops);
		}
	}

	return TCPCI_PARTNER_COMMON_MSG_NOT_HANDLED;
}

/** Check description in emul_tcpci_partner_drp.h */
void tcpci_drp_emul_hard_reset(void *emul)
{
	struct tcpci_drp_emul *drp_emul = emul;

	if (drp_emul->data.sink) {
		tcpci_snk_emul_hard_reset(&drp_emul->snk_data);
	} else {
		tcpci_src_emul_hard_reset(&drp_emul->src_data);
	}
}

/**
 * @brief Function called when TCPM wants to transmit message. Accept received
 *        message and generate response.
 *
 * @param emul Pointer to TCPCI emulator
 * @param ops Pointer to partner operations structure
 * @param tx_msg Pointer to TX message buffer
 * @param type Type of message
 * @param retry Count of retries
 */
static void tcpci_drp_emul_transmit_op(const struct emul *emul,
				       const struct tcpci_emul_partner_ops *ops,
				       const struct tcpci_emul_msg *tx_msg,
				       enum tcpci_msg_type type,
				       int retry)
{
	struct tcpci_drp_emul *drp_emul =
		CONTAINER_OF(ops, struct tcpci_drp_emul, ops);
	enum tcpci_partner_handler_res processed;
	uint16_t header;
	int ret;

	ret = k_mutex_lock(&drp_emul->common_data.transmit_mutex, K_FOREVER);
	if (ret) {
		LOG_ERR("Failed to get DRP mutex");
		/* Inform TCPM that message send failed */
		tcpci_partner_common_msg_handler(&drp_emul->common_data,
						 tx_msg, type,
						 TCPCI_EMUL_TX_FAILED);
		return;
	}

	header = sys_get_le16(tx_msg->buf);

	/* Call common handler */
	processed = tcpci_partner_common_msg_handler(&drp_emul->common_data,
						     tx_msg, type,
						     TCPCI_EMUL_TX_SUCCESS);
	switch (processed) {
	case TCPCI_PARTNER_COMMON_MSG_HARD_RESET:
		k_mutex_unlock(&drp_emul->common_data.transmit_mutex);
		return;
	case TCPCI_PARTNER_COMMON_MSG_HANDLED:
		if (!drp_emul->data.sink && PD_HEADER_CNT(header) == 0 &&
		    PD_HEADER_TYPE(header) == PD_CTRL_SOFT_RESET) {
			/* As source, advertise capabilities after soft reset */
			tcpci_src_emul_send_capability_msg_with_timer(
							&drp_emul->src_data,
							&drp_emul->common_data,
							0);
		}
		/* Message handled nothing to do */
		k_mutex_unlock(&drp_emul->common_data.transmit_mutex);
		return;
	case TCPCI_PARTNER_COMMON_MSG_NOT_HANDLED:
	default:
		/* Continue */
		break;
	}

	/* Handle only SOP messages */
	if (type != TCPCI_MSG_SOP) {
		k_mutex_unlock(&drp_emul->common_data.transmit_mutex);
		return;
	}

	/* Call drp specific handler */
	processed = tcpci_drp_emul_handle_sop_msg(&drp_emul->data,
						  &drp_emul->src_data,
						  &drp_emul->snk_data,
						  &drp_emul->common_data,
						  ops, tx_msg);
	if (processed == TCPCI_PARTNER_COMMON_MSG_HANDLED) {
		k_mutex_unlock(&drp_emul->common_data.transmit_mutex);
		return;
	}

	/* Call source specific handler */
	processed = tcpci_src_emul_handle_sop_msg(&drp_emul->src_data,
						  &drp_emul->common_data,
						  tx_msg);
	if (processed == TCPCI_PARTNER_COMMON_MSG_HANDLED) {
		k_mutex_unlock(&drp_emul->common_data.transmit_mutex);
		return;
	}

	/* Call sink specific handler */
	processed = tcpci_snk_emul_handle_sop_msg(&drp_emul->snk_data,
						  &drp_emul->common_data,
						  tx_msg);
	if (processed == TCPCI_PARTNER_COMMON_MSG_HANDLED) {
		k_mutex_unlock(&drp_emul->common_data.transmit_mutex);
		return;
	}

	/* Send reject for not handled messages (PD rev 2.0) */
	tcpci_partner_send_control_msg(&drp_emul->common_data,
				       PD_CTRL_REJECT, 0);
	k_mutex_unlock(&drp_emul->common_data.transmit_mutex);
}

/**
 * @brief Function called when TCPM consumes message. Free message that is no
 *        longer needed.
 *
 * @param emul Pointer to TCPCI emulator
 * @param ops Pointer to partner operations structure
 * @param rx_msg Message that was consumed by TCPM
 */
static void tcpci_drp_emul_rx_consumed_op(
		const struct emul *emul,
		const struct tcpci_emul_partner_ops *ops,
		const struct tcpci_emul_msg *rx_msg)
{
	struct tcpci_partner_msg *msg = CONTAINER_OF(rx_msg,
						     struct tcpci_partner_msg,
						     msg);

	tcpci_partner_free_msg(msg);
}

/**
 * @brief Function called when emulator is disconnected from TCPCI
 *
 * @param emul Pointer to TCPCI emulator
 * @param ops Pointer to partner operations structure
 */
static void tcpci_drp_emul_disconnect_op(
		const struct emul *emul,
		const struct tcpci_emul_partner_ops *ops)
{
	struct tcpci_drp_emul *drp_emul =
		CONTAINER_OF(ops, struct tcpci_drp_emul, ops);

	tcpci_partner_common_disconnect(&drp_emul->common_data);
	tcpci_src_emul_disconnect(&drp_emul->src_data);
}

/** Check description in emul_tcpci_partner_drp.h */
int tcpci_drp_emul_connect_to_tcpci(struct tcpci_drp_emul_data *data,
				    struct tcpci_src_emul_data *src_data,
				    struct tcpci_snk_emul_data *snk_data,
				    struct tcpci_partner_data *common_data,
				    const struct tcpci_emul_partner_ops *ops,
				    const struct emul *tcpci_emul)
{
	if (data->sink) {
		return tcpci_snk_emul_connect_to_tcpci(snk_data, common_data,
						       ops, tcpci_emul);
	}

	return tcpci_src_emul_connect_to_tcpci(src_data, common_data,
					       ops, tcpci_emul);
}

/** Check description in emul_tcpci_partner_drp.h */
void tcpci_drp_emul_init(struct tcpci_drp_emul *emul)
{
	tcpci_partner_init(&emul->common_data, tcpci_drp_emul_hard_reset, emul);

	/* By default init as sink */
	emul->common_data.data_role = PD_ROLE_DFP;
	emul->common_data.power_role = PD_ROLE_SINK;
	emul->common_data.vconn_role = PD_ROLE_VCONN_OFF;
	emul->common_data.rev = PD_REV20;

	emul->ops.transmit = tcpci_drp_emul_transmit_op;
	emul->ops.rx_consumed = tcpci_drp_emul_rx_consumed_op;
	emul->ops.control_change = NULL;
	emul->ops.disconnect = tcpci_drp_emul_disconnect_op;

	emul->data.sink = true;
	emul->data.current_req = PD_CTRL_INVALID;
	tcpci_src_emul_init_data(&emul->src_data, &emul->common_data);
	tcpci_snk_emul_init_data(&emul->snk_data);

	/* Add dual role bit to sink and source PDOs */
	emul->src_data.pdo[0] |= PDO_FIXED_DUAL_ROLE;
	emul->snk_data.pdo[0] |= PDO_FIXED_DUAL_ROLE;
}
