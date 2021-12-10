/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(tcpci_snk_emul, CONFIG_TCPCI_EMUL_LOG_LEVEL);

#include <zephyr.h>

#include "common.h"
#include "emul/tcpc/emul_tcpci_partner_common.h"
#include "emul/tcpc/emul_tcpci_partner_snk.h"
#include "emul/tcpc/emul_tcpci.h"
#include "usb_pd.h"

static int tcpci_snk_emul_num_of_pdos(struct tcpci_snk_emul_data *data)
{
	for (int pdos = 0; pdos < PDO_MAX_OBJECTS; pdos++) {
		if (data->pdo[pdos] == 0) {
			return pdos;
		}
	}

	return PDO_MAX_OBJECTS;
}

/**
 * @brief Send capability message constructed from USB-C sink emulator PDOs
 *
 * @param data Pointer to USB-C sink emulator
 * @param delay Optional delay
 *
 * @return 0 on success
 * @return -ENOMEM when there is no free memory for message
 * @return -EINVAL on TCPCI emulator add RX message error
 */
static int tcpci_snk_emul_send_capability_msg(struct tcpci_snk_emul_data *data,
					      uint64_t delay)
{
	struct tcpci_partner_msg *msg;
	int pdos;
	int byte;
	int addr;

	/* Find number of PDOs */
	pdos = tcpci_snk_emul_num_of_pdos(data);

	/* Allocate space for header and 4 bytes for each PDO */
	msg = tcpci_partner_alloc_msg(2 + pdos * 4);
	if (msg == NULL) {
		return -ENOMEM;
	}

	tcpci_partner_set_header(&data->common_data, msg, PD_DATA_SINK_CAP,
				 pdos);

	for (int i = 0; i < pdos; i++) {
		/* Address of given PDO in message buffer */
		addr = 2 + i * 4;
		for (byte = 0; byte < 4; byte++) {
			msg->msg.buf[addr + byte] =
				(data->pdo[i] >> (8 * byte)) & 0xff;
		}
	}

	/* Fill tcpci message structure */
	msg->msg.type = TCPCI_MSG_SOP;

	return tcpci_partner_send_msg(&data->common_data, msg, delay);
}

static void tcpci_snk_emul_handle_source_cap(struct tcpci_snk_emul_data *data,
					     const struct tcpci_emul_msg *msg)
{
	struct tcpci_partner_msg *req_msg;
	uint32_t pdo_type;
	uint32_t pdo;
	uint32_t rdo;
	int skip_first_pdo;
	int pdos;
	int addr;
	int op, max;
	int found = -1;
	int pdo_num;

	/* If higher capability bit is set, skip matching to first (5V) PDO */
	if (data->pdo[0] & BIT(28)) {
		skip_first_pdo = 1;
	} else {
		skip_first_pdo = 0;
	}

	/* Find number of PDOs */
	pdos = tcpci_snk_emul_num_of_pdos(data);

	for (addr = 2; addr < msg->cnt; addr += 4) {
		pdo = 0;
		for (int i = 0; i < 4; i++) {
			pdo |= msg->buf[addr + i] << (8 * i);
		}
		pdo_type = pdo & PDO_TYPE_MASK;

		for (int i = skip_first_pdo; i < pdos; i++) {
			if ((data->pdo[i] & PDO_TYPE_MASK) != pdo_type) {
				continue;
			}

			switch (pdo_type) {
			case PDO_TYPE_FIXED:
				if (PDO_FIXED_VOLTAGE(data->pdo[i]) !=
				    PDO_FIXED_VOLTAGE(pdo)) {
					/* Voltage doesn't match */
					continue;
				}
				if (PDO_FIXED_CURRENT(data->pdo[i]) >
				    PDO_FIXED_CURRENT(pdo)) {
					/* Too low current */
					continue;
				}
				break;
			case PDO_TYPE_BATTERY:
				if ((PDO_BATT_MIN_VOLTAGE(data->pdo[i]) <
				     PDO_BATT_MIN_VOLTAGE(pdo)) ||
				    (PDO_BATT_MAX_VOLTAGE(data->pdo[i]) >
				     PDO_BATT_MAX_VOLTAGE(pdo))) {
					/* Voltage not in range */
					continue;
				}
				if (PDO_BATT_MAX_POWER(data->pdo[i]) >
				    PDO_BATT_MAX_POWER(pdo)) {
					/* Too low power */
					continue;
				}
				break;
			case PDO_TYPE_VARIABLE:
				if ((PDO_VAR_MIN_VOLTAGE(data->pdo[i]) <
				     PDO_VAR_MIN_VOLTAGE(pdo)) ||
				    (PDO_VAR_MAX_VOLTAGE(data->pdo[i]) >
				     PDO_VAR_MAX_VOLTAGE(pdo))) {
					/* Voltage not in range */
					continue;
				}
				if (PDO_VAR_MAX_CURRENT(data->pdo[i]) >
				    PDO_VAR_MAX_CURRENT(pdo)) {
					/* Too low current */
					continue;
				}
				break;
			default:
				continue;
			}

			/* Found correct PDO */
			found = i;
			break;
		}

		if (found != -1) {
			break;
		}
	}

	if (found == -1) {
		/* Correct PDO wasn't found, let's use 5V */
		pdo = 0;
		for (int i = 0; i < 4; i++) {
			pdo |= msg->buf[2 + i] << (8 * i);
		}

		op = MIN(PDO_FIXED_CURRENT(pdo),
			 PDO_FIXED_CURRENT(data->pdo[0]));
		max = MAX(PDO_FIXED_CURRENT(pdo),
			  PDO_FIXED_CURRENT(data->pdo[0]));

		rdo = RDO_FIXED(1, op, max, RDO_CAP_MISMATCH);
	} else {
		pdo_num = (addr - 2) / 4 + 1;

		switch (pdo_type) {
		case PDO_TYPE_FIXED:
			rdo = RDO_FIXED(pdo_num, PDO_FIXED_CURRENT(data->pdo[found]),
					PDO_FIXED_CURRENT(data->pdo[found]), 0);
			break;
		case PDO_TYPE_BATTERY:
			rdo = RDO_BATT(pdo_num, PDO_BATT_MAX_POWER(data->pdo[found]),
				       PDO_BATT_MAX_POWER(data->pdo[found]), 0);
			break;
		case PDO_TYPE_VARIABLE:
			rdo = RDO_FIXED(pdo_num, PDO_VAR_MAX_CURRENT(data->pdo[found]),
					PDO_VAR_MAX_CURRENT(data->pdo[found]), 0);
		default:
			rdo = 0;
			break;
		}
	}

	/* Allocate space for header and 4 bytes for RDO */
	req_msg = tcpci_partner_alloc_msg(2 + 4);
	if (req_msg == NULL) {
		return;
	}

	tcpci_partner_set_header(&data->common_data, req_msg, PD_DATA_REQUEST, 1);

	for (int i = 0; i < 4; i++) {
		req_msg->msg.buf[2 + i] = (rdo >> (8 * i)) & 0xff;
	}

	/* Fill tcpci message structure */
	req_msg->msg.type = TCPCI_MSG_SOP;

	tcpci_partner_send_msg(&data->common_data, req_msg, 0);
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
static void tcpci_snk_emul_transmit_op(const struct emul *emul,
				       const struct tcpci_emul_partner_ops *ops,
				       const struct tcpci_emul_msg *tx_msg,
				       enum tcpci_msg_type type,
				       int retry)
{
	struct tcpci_snk_emul_data *data =
		CONTAINER_OF(ops, struct tcpci_snk_emul_data, ops);
	uint16_t header;

	/* Acknowledge that message was sent successfully */
	tcpci_emul_partner_msg_status(emul, TCPCI_EMUL_TX_SUCCESS);

	/* Handle only SOP messages */
	if (type != TCPCI_MSG_SOP) {
		return;
	}

	LOG_HEXDUMP_INF(tx_msg->buf, tx_msg->cnt, "USB-C sink received message");

	header = (tx_msg->buf[1] << 8) | tx_msg->buf[0];

	if (PD_HEADER_CNT(header)) {
		/* Handle data message */
		switch (PD_HEADER_TYPE(header)) {
		case PD_DATA_SOURCE_CAP:
			tcpci_snk_emul_handle_source_cap(data, tx_msg);
			break;
		case PD_DATA_VENDOR_DEF:
			/* VDM (vendor defined message) - ignore */
			break;
		default:
			tcpci_partner_send_control_msg(&data->common_data,
						       PD_CTRL_REJECT, 0);
			break;
		}
	} else {
		/* Handle control message */
		switch (PD_HEADER_TYPE(header)) {
		case PD_CTRL_GET_SOURCE_CAP:
			tcpci_partner_send_control_msg(&data->common_data,
						       PD_CTRL_REJECT, 0);
			break;
		case PD_CTRL_GET_SINK_CAP:
			tcpci_snk_emul_send_capability_msg(data, 0);
			break;
		case PD_CTRL_DR_SWAP:
			tcpci_partner_send_control_msg(&data->common_data,
						       PD_CTRL_REJECT, 0);
			break;
		case PD_CTRL_SOFT_RESET:
			data->common_data.msg_id = 0;
			tcpci_partner_send_control_msg(&data->common_data,
						       PD_CTRL_ACCEPT, 0);
			break;
		case PD_CTRL_ACCEPT:
			break;
		case PD_CTRL_REJECT:
			break;
		case PD_CTRL_PING:
			break;
		case PD_CTRL_PS_RDY:
			break;
		default:
			tcpci_partner_send_control_msg(&data->common_data,
						       PD_CTRL_REJECT, 0);
			break;
		}
	}
}

/**
 * @brief Function called when TCPM consumes message. Free message that is no
 *        longer needed.
 *
 * @param emul Pointer to TCPCI emulator
 * @param ops Pointer to partner operations structure
 * @param rx_msg Message that was consumed by TCPM
 */
static void tcpci_snk_emul_rx_consumed_op(
		const struct emul *emul,
		const struct tcpci_emul_partner_ops *ops,
		const struct tcpci_emul_msg *rx_msg)
{
	struct tcpci_partner_msg *msg = CONTAINER_OF(rx_msg,
						     struct tcpci_partner_msg,
						     msg);

	tcpci_partner_free_msg(msg);
}

/** Check description in emul_tcpci_snk.h */
int tcpci_snk_emul_connect_to_tcpci(struct tcpci_snk_emul_data *data,
				    const struct emul *tcpci_emul)
{
	int ec;

	tcpci_emul_set_partner_ops(tcpci_emul, &data->ops);
	ec = tcpci_emul_connect_partner(tcpci_emul, PD_ROLE_SINK,
					TYPEC_CC_VOLT_RD,
					TYPEC_CC_VOLT_OPEN, POLARITY_CC1);
	if (!ec) {
		data->common_data.tcpci_emul = tcpci_emul;
	}

	return ec;
}

/** Check description in emul_tcpci_snk.h */
void tcpci_snk_emul_init(struct tcpci_snk_emul_data *data)
{
	tcpci_partner_init(&data->common_data);

	data->common_data.data_role = PD_ROLE_DFP;
	data->common_data.power_role = PD_ROLE_SINK;
	data->common_data.rev = PD_REV20;

	data->ops.transmit = tcpci_snk_emul_transmit_op;
	data->ops.rx_consumed = tcpci_snk_emul_rx_consumed_op;
	data->ops.control_change = NULL;

	/* By default there is only PDO 5v@500mA */
	data->pdo[0] = PDO_FIXED(5000, 500, 0);
	for (int i = 1; i < PDO_MAX_OBJECTS; i++) {
		data->pdo[i] = 0;
	}
}

