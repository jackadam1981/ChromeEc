/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_tcpc_emul

#include <logging/log.h>
LOG_MODULE_REGISTER(tcpc_emul, CONFIG_TCPC_EMUL_LOG_LEVEL);

#include <device.h>
#include <emul.h>
#include <drivers/i2c.h>
#include <drivers/i2c_emul.h>

#include "tcpm/tcpci.h"

#include "emul/emul_common_i2c.h"
#include "emul/emul_tcpc.h"

#define TCPC_DATA_FROM_I2C_EMUL(_emul)					     \
	CONTAINER_OF(CONTAINER_OF(_emul, struct i2c_common_emul_data, emul), \
		     struct tcpc_emul_data, common)

/** Run-time data used by the emulator */
struct tcpc_emul_data {
	/** Common I2C data */
	struct i2c_common_emul_data common;

	/** Current state of all emulated TCPC registers */
	uint8_t reg[TCPC_EMUL_REG_COUNT];

	/** Structures representing TX and RX buffers */
	struct tcpc_emul_msg *rx_msg;
	struct tcpc_emul_msg *tx_msg;

	/** Data that should be written to register (except TX_BUFFER) */
	uint16_t write_data;

	/** Return error when trying to write to RO register */
	bool error_on_ro_write;
	/** Return error when trying to write 1 to reserved bit */
	bool error_on_rsvd_write;
	/** Return error when trying to read more than one reg in one msg */
	bool error_on_sequential_read;
	/** Return error when trying to write more than one reg in one msg */
	bool error_on_sequential_write;

	/** User function called when alert line could change */
	tcpc_emul_alert_state_func alert_callback;
	/** Data passed to alert_callback */
	void *alert_callback_data;

	/** Callbacks for specific TCPC device emulator */
	struct tcpc_emul_dev_ops *dev_ops;
	/** Callbacks for TCPC partner */
	struct tcpc_emul_partner_ops *partner;
};

/**
 * @brief Returns number of bytes in specific register
 *
 * @param reg Register address
 *
 * @return Number of bytes
 */
static int tcpc_emul_reg_bytes(int reg)
{

	switch (reg) {
	case TCPC_REG_VENDOR_ID:
	case TCPC_REG_PRODUCT_ID:
	case TCPC_REG_BCD_DEV:
	case TCPC_REG_TC_REV:
	case TCPC_REG_PD_REV:
	case TCPC_REG_PD_INT_REV:
	case TCPC_REG_ALERT:
	case TCPC_REG_ALERT_MASK:
	case TCPC_REG_DEV_CAP_1:
	case TCPC_REG_DEV_CAP_2:
	case TCPC_REG_GENERIC_TIMER:
	case TCPC_REG_VBUS_VOLTAGE:
	case TCPC_REG_VBUS_SINK_DISCONNECT_THRESH:
	case TCPC_REG_VBUS_STOP_DISCHARGE_THRESH:
	case TCPC_REG_VBUS_VOLTAGE_ALARM_HI_CFG:
	case TCPC_REG_VBUS_VOLTAGE_ALARM_LO_CFG:
	case TCPC_REG_VBUS_NONDEFAULT_TARGET:
		return 2;
	}

	/* Assume that other registers are one byte */
	return 1;
}


/** Check description in emul_tcpc.h */
int tcpc_emul_set_reg(const struct emul *emul, int reg, uint16_t val)
{
	struct tcpc_emul_data *data = emul->data;
	int byte;

	if (reg < 0 || reg > TCPC_EMUL_REG_COUNT) {
		return -EINVAL;
	}

	for (byte = tcpc_emul_reg_bytes(reg); byte > 0; byte--) {
		data->reg[reg] = val & 0xff;
		val >>= 8;
		reg++;
	}

	return 0;
}

/** Check description in emul_tcpc.h */
uint16_t tcpc_emul_get_reg(const struct emul *emul, int reg)
{
	struct tcpc_emul_data *data = emul->data;
	uint16_t val = 0;
	int byte;

	if (reg < 0 || reg > TCPC_EMUL_REG_COUNT) {
		return 0;
	}

	byte = tcpc_emul_reg_bytes(reg);
	for (byte -= 1; byte >= 0; byte--) {
		val <<= 8;
		val |= data->reg[reg + byte];
	}

	return val;
}

/**
 * @brief Check if alert line should be active based on alert registers and
 *        masks
 *
 * @param emul Pointer to TCPC emulator
 *
 * @return State of alert line
 */
static int tcpc_emul_check_int(const struct emul *emul)
{
	struct tcpc_emul_data *data = emul->data;
	uint16_t alert_mask;
	uint16_t alert;

	alert = tcpc_emul_get_reg(emul, TCPC_REG_ALERT);
	alert_mask = tcpc_emul_get_reg(emul, TCPC_REG_ALERT_MASK);

	/*
	 * For nested interrupts alert group bit and alert register bit has to
	 * be unmasked
	 */
	if (alert & alert_mask & TCPC_REG_ALERT_ALERT_EXT &&
	    data->reg[TCPC_REG_ALERT_EXT] &
	    data->reg[TCPC_REG_ALERT_EXTENDED_MASK]) {
		return 1;
	}

	if (alert & alert_mask & TCPC_REG_ALERT_EXT_STATUS &&
	    data->reg[TCPC_REG_EXT_STATUS] &
	    data->reg[TCPC_REG_EXT_STATUS_MASK]) {
		return 1;
	}

	if (alert & alert_mask & TCPC_REG_ALERT_FAULT &&
	    data->reg[TCPC_REG_FAULT_STATUS] &
	    data->reg[TCPC_REG_FAULT_STATUS_MASK]) {
		return 1;
	}

	if (alert & alert_mask & TCPC_REG_POWER_STATUS &&
	    data->reg[TCPC_REG_POWER_STATUS] &
	    data->reg[TCPC_REG_POWER_STATUS_MASK]) {
		return 1;
	}

	/* Nested alerts are handled above */
	alert &= ~(TCPC_REG_ALERT_ALERT_EXT | TCPC_REG_ALERT_EXT_STATUS |
		   TCPC_REG_ALERT_FAULT | TCPC_REG_ALERT_POWER_STATUS);
	if (alert & alert_mask) {
		return 1;
	}

	return 0;
}

/**
 * @brief If alert callback is provided, call it with current alert line state
 *
 * @param emul Pointer to TCPC emulator
 *
 * @return State of alert line
 */
static void tcpc_emul_alert_changed(const struct emul *emul)
{
	struct tcpc_emul_data *data = emul->data;

	/* Nothing to do */
	if (data->alert_callback == NULL) {
		return;
	}

	data->alert_callback(emul, tcpc_emul_check_int(emul),
			     data->alert_callback_data);
}

/** Check description in emul_tcpc.h */
int tcpc_emul_add_rx_msg(const struct emul *emul, struct tcpc_emul_msg *rx_msg,
			 bool alert)
{
	struct tcpc_emul_data *data = emul->data;

	if (data->rx_msg == NULL) {
		if ((!(tcpc_emul_get_reg(emul, TCPC_REG_DEV_CAP_2) &
		       TCPC_REG_DEV_CAP_2_LONG_MSG) && rx_msg->cnt > 31) ||
		    rx_msg->cnt > 265) {
			LOG_ERR("Too long first message (%d)", rx_msg->cnt);
			return -1;
		}

		data->rx_msg = rx_msg;
	} else if (data->rx_msg->next == NULL) {
		if (rx_msg->cnt > 31) {
			LOG_ERR("Too long second message (%d)", rx_msg->cnt);
			return -1;
		}

		data->rx_msg->next = rx_msg;
		if (alert) {
			data->reg[TCPC_REG_ALERT + 1] |=
				TCPC_REG_ALERT_RX_BUF_OVF >> 8;
		}
	} else {
		LOG_ERR("Cannot setup third message");
		return -1;
	}

	if (alert) {
		if (rx_msg->cnt > 133) {
			data->reg[TCPC_REG_ALERT + 1] |=
				TCPC_REG_ALERT_RX_BEGINNING >> 8;
		}

		data->reg[TCPC_REG_ALERT] |= TCPC_REG_ALERT_RX_STATUS;

		tcpc_emul_alert_changed(emul);
	}

	rx_msg->next = NULL;
	rx_msg->idx = 0;

	return 0;
}

/** Check description in emul_tcpc.h */
struct tcpc_emul_msg *tcpc_emul_get_tx_msg(const struct emul *emul)
{
	struct tcpc_emul_data *data = emul->data;

	return data->tx_msg;
}

/** Check description in emul_tcpc.h */
void tcpc_emul_set_rev(const struct emul *emul, int rev)
{
	switch (rev) {
	case 1:
		tcpc_emul_set_reg(emul, TCPC_REG_PD_INT_REV,
				  (TCPC_REG_PD_INT_REV_REV_1_0 << 8) |
				   TCPC_REG_PD_INT_REV_VER_1_0);
		return;
	case 2:
		tcpc_emul_set_reg(emul, TCPC_REG_PD_INT_REV,
				  (TCPC_REG_PD_INT_REV_REV_2_0 << 8) |
				   TCPC_REG_PD_INT_REV_VER_1_1);
		return;
	}
}

/** Check description in emul_tcpc.h */
void tcpc_emul_set_err_on_ro_write(const struct emul *emul, bool set)
{
	struct tcpc_emul_data *data = emul->data;

	data->error_on_ro_write = set;
}

/** Check description in emul_tcpc.h */
void tcpc_emul_set_err_on_rsvd_write(const struct emul *emul, bool set)
{
	struct tcpc_emul_data *data = emul->data;

	data->error_on_rsvd_write = set;
}

/** Check description in emul_tcpc.h */
void tcpc_emul_set_err_on_sequential_read(const struct emul *emul, bool set)
{
	struct tcpc_emul_data *data = emul->data;

	data->error_on_sequential_read = set;
}

/** Check description in emul_tcpc.h */
void tcpc_emul_set_err_on_sequential_write(const struct emul *emul, bool set)
{
	struct tcpc_emul_data *data = emul->data;

	data->error_on_sequential_write = set;
}

/** Check description in emul_tcpc.h */
void tcpc_emul_set_alert_callback(const struct emul *emul,
				  tcpc_emul_alert_state_func func,
				  void *func_data)
{
	struct tcpc_emul_data *data = emul->data;

	data->alert_callback = func;
	data->alert_callback_data = func_data;
}

/** Check description in emul_tcpc.h */
void tcpc_emul_set_dev_ops(const struct emul *emul,
			   struct tcpc_emul_dev_ops *dev_ops)
{
	struct tcpc_emul_data *data = emul->data;

	data->dev_ops = dev_ops;
}

/** Check description in emul_tcpc.h */
void tcpc_emul_set_partner_ops(const struct emul *emul,
			       struct tcpc_emul_partner_ops *partner_ops)
{
	struct tcpc_emul_data *data = emul->data;

	data->partner = partner_ops;
}

/** Mask reserved bits in each register of TCPC */
static const uint8_t tcpc_emul_rsvd_mask[] = {
	[TCPC_REG_VENDOR_ID]				= 0x00,
	[TCPC_REG_VENDOR_ID + 1]			= 0x00,
	[TCPC_REG_PRODUCT_ID]				= 0x00,
	[TCPC_REG_PRODUCT_ID + 1]			= 0x00,
	[TCPC_REG_BCD_DEV]				= 0x00,
	[TCPC_REG_BCD_DEV + 1]				= 0xff,
	[TCPC_REG_TC_REV]				= 0x00,
	[TCPC_REG_TC_REV + 1]				= 0x00,
	[TCPC_REG_PD_REV]				= 0x00,
	[TCPC_REG_PD_REV + 1]				= 0x00,
	[TCPC_REG_PD_INT_REV]				= 0x00,
	[TCPC_REG_PD_INT_REV + 1]			= 0x00,
	[0x0c ... 0x0f]					= 0xff, /* Reserved */
	[TCPC_REG_ALERT]				= 0x00,
	[TCPC_REG_ALERT + 1]				= 0x00,
	[TCPC_REG_ALERT_MASK]				= 0x00,
	[TCPC_REG_ALERT_MASK + 1]			= 0x00,
	[TCPC_REG_POWER_STATUS_MASK]			= 0x00,
	[TCPC_REG_FAULT_STATUS_MASK]			= 0x00,
	[TCPC_REG_EXT_STATUS_MASK]			= 0xfe,
	[TCPC_REG_ALERT_EXTENDED_MASK]			= 0xf8,
	[TCPC_REG_CONFIG_STD_OUTPUT]			= 0x00,
	[TCPC_REG_TCPC_CTRL]				= 0x00,
	[TCPC_REG_ROLE_CTRL]				= 0x80,
	[TCPC_REG_FAULT_CTRL]				= 0x80,
	[TCPC_REG_POWER_CTRL]				= 0x00,
	[TCPC_REG_CC_STATUS]				= 0xc0,
	[TCPC_REG_POWER_STATUS]				= 0x00,
	[TCPC_REG_FAULT_STATUS]				= 0x00,
	[TCPC_REG_EXT_STATUS]				= 0xfe,
	[TCPC_REG_ALERT_EXT]				= 0xf8,
	[0x22]						= 0xff, /* Reserved */
	[TCPC_REG_COMMAND]				= 0x00,
	[TCPC_REG_DEV_CAP_1]				= 0x00,
	[TCPC_REG_DEV_CAP_1 + 1]			= 0x00,
	[TCPC_REG_DEV_CAP_2]				= 0x80,
	[TCPC_REG_DEV_CAP_2 + 1]			= 0x00,
	[TCPC_REG_STD_INPUT_CAP]			= 0xe0,
	[TCPC_REG_STD_OUTPUT_CAP]			= 0x00,
	[TCPC_REG_CONFIG_EXT_1]				= 0xfc,
	[0x2b]						= 0xff, /* Reserved */
	[TCPC_REG_GENERIC_TIMER]			= 0x00,
	[TCPC_REG_GENERIC_TIMER + 1]			= 0x00,
	[TCPC_REG_MSG_HDR_INFO]				= 0xe0,
	[TCPC_REG_RX_DETECT]				= 0x00,
	[TCPC_REG_RX_BUFFER ... 0x4f]			= 0x00,
	[TCPC_REG_TRANSMIT ... 0x69]			= 0x00,
	[TCPC_REG_VBUS_VOLTAGE]				= 0xf0,
	[TCPC_REG_VBUS_VOLTAGE + 1]			= 0x00,
	[TCPC_REG_VBUS_SINK_DISCONNECT_THRESH]		= 0x00,
	[TCPC_REG_VBUS_SINK_DISCONNECT_THRESH + 1]	= 0xfc,
	[TCPC_REG_VBUS_STOP_DISCHARGE_THRESH]		= 0x00,
	[TCPC_REG_VBUS_STOP_DISCHARGE_THRESH + 1]	= 0xfc,
	[TCPC_REG_VBUS_VOLTAGE_ALARM_HI_CFG]		= 0x00,
	[TCPC_REG_VBUS_VOLTAGE_ALARM_HI_CFG + 1]	= 0xfc,
	[TCPC_REG_VBUS_VOLTAGE_ALARM_LO_CFG]		= 0x00,
	[TCPC_REG_VBUS_VOLTAGE_ALARM_LO_CFG + 1]	= 0xfc,
	[TCPC_REG_VBUS_NONDEFAULT_TARGET]		= 0x00,
	[TCPC_REG_VBUS_NONDEFAULT_TARGET + 1]		= 0x00,
	[0x7c ... 0x7f]					= 0xff, /* Reserved */
	[0x80 ... TCPC_EMUL_REG_COUNT - 1]		= 0x00,
};


/**
 * @brief Reset role control and header info registers to default values.
 *
 * @param emul Pointer to TCPC emulator
 */
static void tcpc_emul_reset_role_ctrl(const struct emul *emul)
{
	struct tcpc_emul_data *data = emul->data;

	switch (tcpc_emul_get_reg(emul, TCPC_REG_DEV_CAP_1) &
		TCPC_REG_DEV_CAP_1_PWRROLE_MASK) {
	case TCPC_REG_DEV_CAP_1_PWRROLE_SRC_OR_SNK:
	case TCPC_REG_DEV_CAP_1_PWRROLE_SNK:
	case TCPC_REG_DEV_CAP_1_PWRROLE_SNK_ACC:
		data->reg[TCPC_REG_ROLE_CTRL]			= 0x0a;
		data->reg[TCPC_REG_MSG_HDR_INFO]		= 0x04;
		break;
	case TCPC_REG_DEV_CAP_1_PWRROLE_SRC:
		/* Dead batter */
		data->reg[TCPC_REG_ROLE_CTRL]			= 0x05;
		data->reg[TCPC_REG_MSG_HDR_INFO]		= 0x0d;
		break;
	case TCPC_REG_DEV_CAP_1_PWRROLE_DRP:
		/* Dead batter and dbg acc ind */
		data->reg[TCPC_REG_ROLE_CTRL]			= 0x4a;
		data->reg[TCPC_REG_MSG_HDR_INFO]		= 0x04;
		break;
	case TCPC_REG_DEV_CAP_1_PWRROLE_SRC_SNK_DRP_ADPT_CBL:
	case TCPC_REG_DEV_CAP_1_PWRROLE_SRC_SNK_DRP:
		/* Dead batter and dbg acc ind */
		data->reg[TCPC_REG_ROLE_CTRL]			= 0x4a;
		data->reg[TCPC_REG_MSG_HDR_INFO]		= 0x04;
		break;
	}
}

/**
 * @brief Reset registers to default values. Vendor and reserved registers
 *        are not changed.
 *
 * @param emul Pointer to TCPC emulator
 */
static void tcpc_emul_reset(const struct emul *emul)
{
	struct tcpc_emul_data *data = emul->data;

	data->reg[TCPC_REG_ALERT]				= 0x00;
	data->reg[TCPC_REG_ALERT + 1]				= 0x00;
	data->reg[TCPC_REG_ALERT_MASK]				= 0xff;
	data->reg[TCPC_REG_ALERT_MASK + 1]			= 0x7f;
	data->reg[TCPC_REG_POWER_STATUS_MASK]			= 0xff;
	data->reg[TCPC_REG_FAULT_STATUS_MASK]			= 0xff;
	data->reg[TCPC_REG_EXT_STATUS_MASK]			= 0x01;
	data->reg[TCPC_REG_ALERT_EXTENDED_MASK]			= 0x07;
	data->reg[TCPC_REG_CONFIG_STD_OUTPUT]			= 0x60;
	data->reg[TCPC_REG_TCPC_CTRL]				= 0x00;
	data->reg[TCPC_REG_FAULT_CTRL]				= 0x00;
	data->reg[TCPC_REG_POWER_CTRL]				= 0x60;
	data->reg[TCPC_REG_CC_STATUS]				= 0x00;
	data->reg[TCPC_REG_POWER_STATUS]			= 0x08;
	data->reg[TCPC_REG_FAULT_STATUS]			= 0x80;
	data->reg[TCPC_REG_EXT_STATUS]				= 0x00;
	data->reg[TCPC_REG_ALERT_EXT]				= 0x00;
	data->reg[TCPC_REG_COMMAND]				= 0x00;
	data->reg[TCPC_REG_CONFIG_EXT_1]			= 0x00;
	data->reg[TCPC_REG_GENERIC_TIMER]			= 0x00;
	data->reg[TCPC_REG_GENERIC_TIMER + 1]			= 0x00;
	data->reg[TCPC_REG_RX_DETECT]				= 0x00;
	data->reg[TCPC_REG_VBUS_VOLTAGE]			= 0x00;
	data->reg[TCPC_REG_VBUS_VOLTAGE + 1]			= 0x00;
	data->reg[TCPC_REG_VBUS_SINK_DISCONNECT_THRESH]		= 0x8c;
	data->reg[TCPC_REG_VBUS_SINK_DISCONNECT_THRESH + 1]	= 0x00;
	data->reg[TCPC_REG_VBUS_STOP_DISCHARGE_THRESH]		= 0x20;
	data->reg[TCPC_REG_VBUS_STOP_DISCHARGE_THRESH + 1]	= 0x00;
	data->reg[TCPC_REG_VBUS_VOLTAGE_ALARM_HI_CFG]		= 0x00;
	data->reg[TCPC_REG_VBUS_VOLTAGE_ALARM_HI_CFG + 1]	= 0x00;
	data->reg[TCPC_REG_VBUS_VOLTAGE_ALARM_LO_CFG]		= 0x00;
	data->reg[TCPC_REG_VBUS_VOLTAGE_ALARM_LO_CFG + 1]	= 0x00;
	data->reg[TCPC_REG_VBUS_NONDEFAULT_TARGET]		= 0x00;
	data->reg[TCPC_REG_VBUS_NONDEFAULT_TARGET + 1]		= 0x00;

	tcpc_emul_reset_role_ctrl(emul);

	if (data->dev_ops && data->dev_ops->reset) {
		data->dev_ops->reset(emul, data->dev_ops);
	}

	tcpc_emul_alert_changed(emul);
}

/**
 * @brief Set alert and fault registers to indicate i2c interface fault
 *
 * @param emul Pointer to TCPC emulator
 */
static void tcpc_emul_set_i2c_interface_err(const struct emul *emul)
{
	struct tcpc_emul_data *data = emul->data;

	data->reg[TCPC_REG_FAULT_STATUS] |=
					TCPC_REG_FAULT_STATUS_I2C_INTERFACE_ERR;
	data->reg[TCPC_REG_ALERT + 1] |= TCPC_REG_ALERT_FAULT >> 8;

	tcpc_emul_alert_changed(emul);
}

/**
 * @brief Handle read from RX buffer registers for TCPC rev 1.0 and rev 2.0
 *
 * @param emul Pointer to TCPC emulator
 * @param reg First byte of last i2c write message
 * @param val Pointer where byte to read should be stored
 * @param bytes Number of bytes already readded
 *
 * @return 0 on success
 * @return -EIO invalid read request
 */
static int tcpc_emul_handle_rx_buf(const struct emul *emul, int reg,
				   uint8_t *val, int bytes)
{
	struct tcpc_emul_data *data = emul->data;
	int is_rev1;

	if (data->rx_msg == NULL) {
		LOG_ERR("Accessing RX buffer when there is no msg");
		tcpc_emul_set_i2c_interface_err(emul);
		return -EIO;
	}

	is_rev1 = data->reg[TCPC_REG_PD_INT_REV] == TCPC_REG_PD_INT_REV_REV_1_0;

	if (!is_rev1 && reg != TCPC_REG_RX_BUFFER) {
		LOG_ERR("Register 0x%x defined only for revision 1.0", reg);
		tcpc_emul_set_i2c_interface_err(emul);
		return -EIO;
	}

	switch (reg) {
	case TCPC_REG_RX_BUFFER:
		if (bytes == 0) {
			*val = data->rx_msg->cnt;
		} else if (is_rev1) {
			LOG_ERR("Revision 1.0 has only byte count at 0x30");
			tcpc_emul_set_i2c_interface_err(emul);
			return -EIO;
		} else if (bytes == 1) {
			*val = data->rx_msg->type;
		} else if (data->rx_msg->idx < data->rx_msg->cnt) {
			*val = data->rx_msg->buf[data->rx_msg->idx];
			data->rx_msg->idx++;
		} else {
			LOG_ERR("Reading past RX buffer");
			tcpc_emul_set_i2c_interface_err(emul);
			return -EIO;
		}
		break;

	case TCPC_REG_RX_BUF_FRAME_TYPE:
		if (bytes != 0) {
			LOG_ERR("Reading byte %d from 1 byte register 0x%x",
				bytes, reg);
			if (data->error_on_sequential_read) {
				tcpc_emul_set_i2c_interface_err(emul);
				return -EIO;
			}

			*val = 0xff;
		} else {
			*val = data->rx_msg->type;
		}
		break;

	case TCPC_REG_RX_HDR:
		if (bytes > 1) {
			LOG_ERR("Reading byte %d from 2 byte register 0x%x",
				bytes, reg);
			if (data->error_on_sequential_read) {
				tcpc_emul_set_i2c_interface_err(emul);
				return -EIO;
			}

			*val = 0xff;
		} else {
			*val = data->rx_msg->buf[bytes];
		}
		break;

	case TCPC_REG_RX_DATA:
		if (bytes < data->rx_msg->cnt - 2) {
			/* rx_msg cnt include two bytes of header */
			*val = data->rx_msg->buf[bytes + 2];
			data->rx_msg->idx++;
		} else {
			LOG_ERR("Reading past RX buffer");
			tcpc_emul_set_i2c_interface_err(emul);
			return -EIO;
		}
		break;
	}

	return 0;
}

/**
 * @brief Function called for each byte of read message
 *
 * @param i2c_emul Pointer to TCPC emulator
 * @param reg First byte of last write message
 * @param val Pointer where byte to read should be stored
 * @param bytes Number of bytes already readded
 *
 * @return 0 on success
 */
static int tcpc_emul_read_byte(struct i2c_emul *i2c_emul, int reg, uint8_t *val,
			       int bytes)
{
	struct tcpc_emul_data *data;
	const struct emul *emul;

	emul = i2c_emul->parent;
	data = TCPC_DATA_FROM_I2C_EMUL(i2c_emul);

	if (data->dev_ops && data->dev_ops->read_byte) {
		switch (data->dev_ops->read_byte(emul, data->dev_ops, reg, val,
						 bytes)) {
		case TCPC_EMUL_CONTINUE:
			break;
		case TCPC_EMUL_DONE:
			return 0;
		case TCPC_EMUL_ERROR:
		default:
			return -EIO;
		}
	}

	switch (reg) {
	/* 16 bits values */
	case TCPC_REG_VENDOR_ID:
	case TCPC_REG_PRODUCT_ID:
	case TCPC_REG_BCD_DEV:
	case TCPC_REG_TC_REV:
	case TCPC_REG_PD_REV:
	case TCPC_REG_PD_INT_REV:
	case TCPC_REG_ALERT:
	case TCPC_REG_ALERT_MASK:
	case TCPC_REG_DEV_CAP_1:
	case TCPC_REG_DEV_CAP_2:
	case TCPC_REG_VBUS_VOLTAGE:
	case TCPC_REG_VBUS_SINK_DISCONNECT_THRESH:
	case TCPC_REG_VBUS_STOP_DISCHARGE_THRESH:
	case TCPC_REG_VBUS_VOLTAGE_ALARM_HI_CFG:
	case TCPC_REG_VBUS_VOLTAGE_ALARM_LO_CFG:
	case TCPC_REG_VBUS_NONDEFAULT_TARGET:
		if (bytes > 1) {
			LOG_ERR("Reading byte %d from 2 byte register 0x%x",
				bytes, reg);
			if (data->error_on_sequential_read) {
				tcpc_emul_set_i2c_interface_err(emul);
				return -EIO;
			}

			*val = 0xff;
		} else {
			*val = data->reg[reg + bytes];
		}
		break;

	/* 8 bits values */
	case TCPC_REG_POWER_STATUS_MASK:
	case TCPC_REG_FAULT_STATUS_MASK:
	case TCPC_REG_EXT_STATUS_MASK:
	case TCPC_REG_ALERT_EXTENDED_MASK:
	case TCPC_REG_CONFIG_STD_OUTPUT:
	case TCPC_REG_TCPC_CTRL:
	case TCPC_REG_ROLE_CTRL:
	case TCPC_REG_FAULT_CTRL:
	case TCPC_REG_POWER_CTRL:
	case TCPC_REG_CC_STATUS:
	case TCPC_REG_POWER_STATUS:
	case TCPC_REG_FAULT_STATUS:
	case TCPC_REG_EXT_STATUS:
	case TCPC_REG_ALERT_EXT:
	case TCPC_REG_STD_INPUT_CAP:
	case TCPC_REG_STD_OUTPUT_CAP:
	case TCPC_REG_CONFIG_EXT_1:
	case TCPC_REG_MSG_HDR_INFO:
	case TCPC_REG_RX_DETECT:
		if (bytes > 0) {
			LOG_ERR("Reading byte %d from 1 byte register 0x%x",
				bytes, reg);
			if (data->error_on_sequential_read) {
				tcpc_emul_set_i2c_interface_err(emul);
				return -EIO;
			}

			*val = 0xff;
		} else {
			*val = data->reg[reg];
		}
		break;

	case TCPC_REG_RX_BUFFER:
	case TCPC_REG_RX_BUF_FRAME_TYPE:
	case TCPC_REG_RX_HDR:
	case TCPC_REG_RX_DATA:
		return tcpc_emul_handle_rx_buf(emul, reg, val, bytes);

	default:
		LOG_ERR("Reading from reg 0x%x which is WO or undefined", reg);
		tcpc_emul_set_i2c_interface_err(emul);
		return -EIO;
	}

	return 0;
}

/**
 * @brief Function called for each byte of write message. Data are stored
 *        in write_data field of tcpc_emul_data or in tx_msg in case of
 *        writing to TX buffer.
 *
 * @param i2c_emul Pointer to TCPC emulator
 * @param reg First byte of write message
 * @param val Received byte of write message
 * @param bytes Number of bytes already received
 *
 * @return 0 on success
 * @return -EIO on invalid write to TX buffer
 */
static int tcpc_emul_write_byte(struct i2c_emul *i2c_emul, int reg, uint8_t val,
				int bytes)
{
	struct tcpc_emul_data *data;
	const struct emul *emul;
	int is_rev1;

	emul = i2c_emul->parent;
	data = TCPC_DATA_FROM_I2C_EMUL(i2c_emul);

	if (data->dev_ops && data->dev_ops->write_byte) {
		switch (data->dev_ops->write_byte(emul, data->dev_ops, reg, val,
						  bytes)) {
		case TCPC_EMUL_CONTINUE:
			break;
		case TCPC_EMUL_DONE:
			return 0;
		case TCPC_EMUL_ERROR:
		default:
			return -EIO;
		}
	}

	is_rev1 = data->reg[TCPC_REG_PD_INT_REV] == TCPC_REG_PD_INT_REV_REV_1_0;
	switch (reg) {
	case TCPC_REG_TX_BUFFER:
		if (is_rev1) {
			if (bytes > 1) {
				LOG_ERR("Rev 1.0 has only byte count at 0x51");
				tcpc_emul_set_i2c_interface_err(emul);
				return -EIO;
			}
			data->tx_msg->idx = val;
		}

		if (bytes == 1) {
			data->tx_msg->cnt = val;
		} else {
			if (data->tx_msg->cnt > 0) {
				data->tx_msg->cnt--;
				data->tx_msg->buf[data->tx_msg->idx] = val;
				data->tx_msg->idx++;
			} else {
				LOG_ERR("Writing past TX buffer");
				tcpc_emul_set_i2c_interface_err(emul);
				return -EIO;
			}
		}

		return 0;

	case TCPC_REG_TX_DATA:
		if (!is_rev1) {
			LOG_ERR("Register 0x%x defined only for revision 1.0",
				reg);
			tcpc_emul_set_i2c_interface_err(emul);
			return -EIO;
		}

		/* Skip header and account reg byte */
		bytes += 2 - 1;

		if (bytes > 29) {
			LOG_ERR("Writing past TX buffer");
			tcpc_emul_set_i2c_interface_err(emul);
			return -EIO;
		}
		data->tx_msg->buf[bytes] = val;
		return 0;

	case TCPC_REG_TX_HDR:
		if (!is_rev1) {
			LOG_ERR("Register 0x%x defined only for revision 1.0",
				reg);
			tcpc_emul_set_i2c_interface_err(emul);
			return -EIO;
		}

		/* Account reg byte */
		bytes -= 1;

		if (bytes > 1) {
			LOG_ERR("Writing byte %d to 2 byte register 0x%x",
				 bytes, reg);
			tcpc_emul_set_i2c_interface_err(emul);
			return -EIO;
		}
		data->tx_msg->buf[bytes] = val;
		return 0;
	}

	if (bytes == 1) {
		data->write_data = val;
	} else if (bytes == 2) {
		data->write_data |= (uint16_t)val << 8;
	}

	return 0;
}

/**
 * @brief Handle writes to command register
 *
 * @param emul Pointer to TCPC emulator
 *
 * @return 0 on success
 * @retunr -EIO on unknown command value
 */
static int tcpc_emul_handle_command(const struct emul *emul)
{
	struct tcpc_emul_data *data = emul->data;

	switch (data->write_data & 0xff) {
	case TCPC_REG_COMMAND_ENABLE_VBUS_DETECT:
	case TCPC_REG_COMMAND_SNK_CTRL_LOW:
	case TCPC_REG_COMMAND_SNK_CTRL_HIGH:
	case TCPC_REG_COMMAND_SRC_CTRL_LOW:
	case TCPC_REG_COMMAND_SRC_CTRL_HIGH:
	case TCPC_REG_COMMAND_LOOK4CONNECTION:
	case TCPC_REG_COMMAND_I2CIDLE:
		/*
		 * Set command register to allow easier inspection of last
		 * command sent
		 */
		tcpc_emul_set_reg(emul, TCPC_REG_COMMAND,
				  data->write_data & 0xff);
		return 0;
	default:
		tcpc_emul_set_i2c_interface_err(emul);
		return -EIO;
	}
}

/**
 * @brief Handle write to transmit register
 *
 * @param emul Pointer to TCPC emulator
 *
 * @return 0 on success
 */
static int tcpc_emul_handle_transmit(const struct emul *emul)
{
	struct tcpc_emul_data *data = emul->data;

	data->tx_msg->cnt = data->tx_msg->idx;
	data->tx_msg->type = TCPC_REG_TRANSMIT_TYPE(data->write_data);
	data->tx_msg->idx = 0;

	if (data->partner && data->partner->transmit) {
		data->partner->transmit(emul, data->partner, data->tx_msg,
				TCPC_REG_TRANSMIT_TYPE(data->write_data),
				TCPC_REG_TRANSMIT_RETRY(data->write_data));
	}

	return 0;
}

/**
 * @brief Handle I2C write message. It is checked if accessed register isn't RO
 *        and reserved bits are set to 0.
 *
 * @param i2c_emul Pointer to TCPC emulator
 * @param reg Register which is written
 * @param msg_len Length of handled I2C message
 *
 * @return 0 on success
 * @return -EIO on error
 */
static int tcpc_emul_handle_write(struct i2c_emul *i2c_emul, int reg,
				  int msg_len)
{
	struct tcpc_emul_data *data;
	const struct emul *emul;
	uint16_t rsvd_mask = 0;
	bool inform_partner = false;
	bool alert_changed = false;
	int reg_bytes;

	/* This write message was setting register before read */
	if (msg_len == 1) {
		return 0;
	}

	/* Exclude register address byte from message length */
	msg_len--;

	emul = i2c_emul->parent;
	data = TCPC_DATA_FROM_I2C_EMUL(i2c_emul);

	if (data->dev_ops && data->dev_ops->handle_write) {
		switch (data->dev_ops->handle_write(emul, data->dev_ops, reg,
						    msg_len)) {
		case TCPC_EMUL_CONTINUE:
			break;
		case TCPC_EMUL_DONE:
			return 0;
		case TCPC_EMUL_ERROR:
		default:
			return -EIO;
		}
	}

	switch (reg) {
	/* Alert registers */
	case TCPC_REG_ALERT:
		/* Overflow is cleared by Receive SOP message status */
		data->write_data &= ~TCPC_REG_ALERT_RX_BUF_OVF;
		if (data->write_data & TCPC_REG_ALERT_RX_STATUS) {
			data->write_data |= TCPC_REG_ALERT_RX_BUF_OVF;
			/* Load next message if possible */
			if (data->rx_msg && data->rx_msg->next) {
				data->write_data &= ~TCPC_REG_ALERT_RX_STATUS;
				data->rx_msg = data->rx_msg->next;
				data->rx_msg->idx = 0;
			} else {
				data->rx_msg = NULL;
			}
		}
	/* fallthrough */
	case TCPC_REG_FAULT_STATUS:
	case TCPC_REG_ALERT_EXT:
		/* Clear bits where TCPM set 1 */
		data->write_data = tcpc_emul_get_reg(emul, reg) &
				   (~data->write_data);
	/* fallthrough */
	case TCPC_REG_ALERT_MASK:
	case TCPC_REG_POWER_STATUS_MASK:
	case TCPC_REG_FAULT_STATUS_MASK:
	case TCPC_REG_EXT_STATUS_MASK:
	case TCPC_REG_ALERT_EXTENDED_MASK:
		alert_changed = true;
		break;

	/* Control registers */
	case TCPC_REG_TCPC_CTRL:
	case TCPC_REG_ROLE_CTRL:
	case TCPC_REG_FAULT_CTRL:
	case TCPC_REG_POWER_CTRL:
		inform_partner = true;
		break;

	/* Simple write registers */
	case TCPC_REG_VBUS_SINK_DISCONNECT_THRESH:
	case TCPC_REG_VBUS_STOP_DISCHARGE_THRESH:
	case TCPC_REG_VBUS_VOLTAGE_ALARM_HI_CFG:
	case TCPC_REG_VBUS_VOLTAGE_ALARM_LO_CFG:
	case TCPC_REG_VBUS_NONDEFAULT_TARGET:
	case TCPC_REG_CONFIG_STD_OUTPUT:
	case TCPC_REG_MSG_HDR_INFO:
	case TCPC_REG_RX_DETECT:
		break;

	case TCPC_REG_CONFIG_EXT_1:
		if (data->write_data & TCPC_REG_CONFIG_EXT_1_FR_SWAP_SNK_DIR &&
		    ((data->reg[TCPC_REG_STD_INPUT_CAP] &
		      TCPC_REG_STD_INPUT_CAP_SRC_FR_SWAP) == BIT(4)) &&
		    data->reg[TCPC_REG_STD_OUTPUT_CAP] &
		    TCPC_REG_STD_OUTPUT_CAP_SNK_DISC_DET) {
			tcpc_emul_set_i2c_interface_err(emul);
			return 0;
		}
		break;

	case TCPC_REG_COMMAND:
		if (data->error_on_sequential_write && msg_len != 1) {
			tcpc_emul_set_i2c_interface_err(emul);
			LOG_ERR("Writing byte %d to 1 byte register 0x%x",
				msg_len, reg);
			return -EIO;
		}
		return tcpc_emul_handle_command(emul);

	case TCPC_REG_TRANSMIT:
		if (data->error_on_sequential_write && msg_len != 1) {
			tcpc_emul_set_i2c_interface_err(emul);
			LOG_ERR("Writing byte %d to 1 byte register 0x%x",
				msg_len, reg);
			return -EIO;
		}
		return tcpc_emul_handle_transmit(emul);

	case TCPC_REG_GENERIC_TIMER:
		/* TODO: Add timer */
		return 0;

	/* Already handled in tcpc_emul_write_byte() */
	case TCPC_REG_TX_BUFFER:
	case TCPC_REG_TX_DATA:
	case TCPC_REG_TX_HDR:
		return 0;
	default:
		tcpc_emul_set_i2c_interface_err(emul);
		LOG_ERR("Write to reg 0x%x which is RO, undefined or unaligned",
			reg);
		return -EIO;
	}

	reg_bytes = tcpc_emul_reg_bytes(reg);

	/* Compute reserved bits mask */
	switch (reg_bytes) {
	case 2:
		rsvd_mask = tcpc_emul_rsvd_mask[reg + 1];
	case 1:
		rsvd_mask <<= 8;
		rsvd_mask |= tcpc_emul_rsvd_mask[reg];
		break;
	}

	/* Check reserved bits */
	if (data->error_on_rsvd_write && rsvd_mask & data->write_data) {
		tcpc_emul_set_i2c_interface_err(emul);
		LOG_ERR("Writing 0x%x to reg 0x%x with rsvd bits mask 0x%x",
			data->write_data, reg, rsvd_mask);
		return -EIO;
	}

	/* Check if I2C write message has correct length */
	if (data->error_on_sequential_write && msg_len != reg_bytes) {
		LOG_ERR("Writing byte %d to %d byte register 0x%x",
			 msg_len, reg_bytes, reg);
		tcpc_emul_set_i2c_interface_err(emul);
		return -EIO;
	}

	/* Set new value of register */
	tcpc_emul_set_reg(emul, reg, data->write_data);

	if (alert_changed) {
		tcpc_emul_alert_changed(emul);
	}

	if (inform_partner && data->partner && data->partner->control_change) {
		data->partner->control_change(emul, data->partner);
	}

	return 0;
}

/**
 * @brief Get currently accessed register, which always equals to selected
 *        register.
 *
 * @param i2c_emul Pointer to TCPC emulator
 * @param reg First byte of last write message
 * @param bytes Number of bytes already handled from current message
 * @param read If currently handled is read message
 *
 * @return Currently accessed register
 */
static int tcpc_emul_access_reg(struct i2c_emul *i2c_emul, int reg, int bytes,
				bool read)
{
	return reg;
}

/* Device instantiation */

/** Check description in emul_tcpc.h */
struct i2c_emul *tcpc_emul_get_i2c_emul(const struct emul *emul)
{
	struct tcpc_emul_data *data = emul->data;

	return &data->common.emul;
}

/**
 * @brief Set up a new TCPC emulator
 *
 * This should be called for each TCPC device that needs to be
 * emulated. It registers it with the I2C emulation controller.
 *
 * @param emul Emulation information
 * @param parent Device to emulate
 *
 * @return 0 indicating success (always)
 */
static int tcpc_emul_init(const struct emul *emul, const struct device *parent)
{
	const struct i2c_common_emul_cfg *cfg = emul->cfg;
	struct tcpc_emul_data *data = emul->data;
	int ret;

	data->common.emul.api = &i2c_common_emul_api;
	data->common.emul.addr = cfg->addr;
	data->common.emul.parent = emul;
	data->common.i2c = parent;
	data->common.cfg = cfg;
	i2c_common_emul_init(&data->common);

	ret = i2c_emul_register(parent, emul->dev_label, &data->common.emul);

	tcpc_emul_reset(emul);

	return ret;
}

#define TCPC_EMUL(n)							\
	uint8_t tcpc_emul_tx_buf_##n[128];				\
	static struct tcpc_emul_msg tcpc_emul_tx_msg_##n = {		\
		.buf = tcpc_emul_tx_buf_##n,				\
	};								\
									\
	static struct tcpc_emul_data tcpc_emul_data_##n = {		\
		.tx_msg = &tcpc_emul_tx_msg_##n,			\
		.error_on_ro_write = true,				\
		.error_on_rsvd_write = true,				\
		.error_on_sequential_read = true,			\
		.error_on_sequential_write = true,			\
		.common = {						\
			.write_byte = tcpc_emul_write_byte,		\
			.finish_write = tcpc_emul_handle_write,		\
			.read_byte = tcpc_emul_read_byte,		\
			.access_reg = tcpc_emul_access_reg,		\
		},							\
	};								\
									\
	static const struct i2c_common_emul_cfg tcpc_emul_cfg_##n = {	\
		.i2c_label = DT_INST_BUS_LABEL(n),			\
		.dev_label = DT_INST_LABEL(n),                          \
		.data = &tcpc_emul_data_##n.common,			\
		.addr = DT_INST_REG_ADDR(n),				\
	};								\
	EMUL_DEFINE(tcpc_emul_init, DT_DRV_INST(n), &tcpc_emul_cfg_##n,	\
		    &tcpc_emul_data_##n)

DT_INST_FOREACH_STATUS_OKAY(TCPC_EMUL)
