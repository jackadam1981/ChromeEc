/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_ps8xxx_emul

#include <logging/log.h>
LOG_MODULE_REGISTER(ps8xxx_emul, CONFIG_TCPCI_EMUL_LOG_LEVEL);

#include <device.h>
#include <emul.h>
#include <drivers/i2c.h>
#include <drivers/i2c_emul.h>

#include "tcpm/tcpci.h"

#include "emul/emul_common_i2c.h"
#include "emul/emul_tcpci.h"

#define PS8XXX_REG_FW_REV		0x82
#define PS8XXX_REG_I2C_DBG_EN		0xa0
#define PS8XXX_REG_MUX_IN_HPD_ASSERTION	0xd0

#define PS8805_P0_REG_CHIP_REVISION	0x62
#define PS8XXX_P1_REG_MUX_USB_DCI_CFG	0x4B

enum ps8xxx_emul_port {
	PS8XXX_EMUL_PORT_0,
	PS8XXX_EMUL_PORT_1,
	PS8XXX_EMUL_PORT_INVAL,
};

/** Run-time data used by the emulator */
struct ps8xxx_emul_data {
	/** Common I2C data */
	struct i2c_common_emul_data p0_data;
	struct i2c_common_emul_data p1_data;

	int prod_id;
	const struct emul *tcpci_emul;

	uint8_t chip_rev;
	uint8_t dci_cfg;
};

struct ps8xxx_emul_cfg {
	const char *tcpci_emul;

	const struct i2c_common_emul_cfg p0_cfg;
	const struct i2c_common_emul_cfg p1_cfg;
};

/**
 * @brief Function called for each byte of read message
 *
 * @param emul Pointer to TCPCI emulator
 * @param ops Pointer to device operations structure
 * @param reg First byte of last write message
 * @param val Pointer where byte to read should be stored
 * @param bytes Number of bytes already readded
 *
 * @return TCPCI_EMUL_CONTINUE to continue with default handler
 * @return TCPCI_EMUL_DONE to immedietly return success
 * @return TCPCI_EMUL_ERROR to immedietly return error
 */
enum tcpci_emul_ops_resp ps8xxx_emul_tcpci_read_byte(
					const struct emul *emul,
					const struct tcpci_emul_dev_ops *ops,
					int reg, uint8_t *val, int bytes)
{
	uint16_t reg_val;

	switch (reg) {
	case PS8XXX_REG_FW_REV:
	case PS8XXX_REG_I2C_DBG_EN:
	case PS8XXX_REG_MUX_IN_HPD_ASSERTION:
		if (bytes != 0) {
			LOG_ERR("Reading byte %d from 1 byte register 0x%x",
				bytes, reg);
			return TCPCI_EMUL_ERROR;
		}

		tcpci_emul_get_reg(emul, reg, &reg_val);
		*val = reg_val & 0xff;
		return TCPCI_EMUL_DONE;
	default:
		return TCPCI_EMUL_CONTINUE;
	}
}

/**
 * @brief Function called for each byte of write message
 *
 * @param emul Pointer to TCPCI emulator
 * @param ops Pointer to device operations structure
 * @param reg First byte of write message
 * @param val Received byte of write message
 * @param bytes Number of bytes already received
 *
 * @return TCPCI_EMUL_CONTINUE to continue with default handler
 * @return TCPCI_EMUL_DONE to immedietly return success
 * @return TCPCI_EMUL_ERROR to immedietly return error
 */
enum tcpci_emul_ops_resp ps8xxx_emul_tcpci_write_byte(
					const struct emul *emul,
					const struct tcpci_emul_dev_ops *ops,
					int reg, uint8_t val, int bytes)
{
	switch (reg) {
	case PS8XXX_REG_I2C_DBG_EN:
	case PS8XXX_REG_MUX_IN_HPD_ASSERTION:
		if (bytes != 1) {
			LOG_ERR("Writing byte %d to 1 byte register 0x%x",
				bytes, reg);
			return TCPCI_EMUL_ERROR;
		}

		tcpci_emul_set_reg(emul, reg, val);
		return TCPCI_EMUL_DONE;
	default:
		return TCPCI_EMUL_CONTINUE;
	}
}

/**
 * @brief Function called on the end of write message
 *
 * @param emul Pointer to TCPCI emulator
 * @param ops Pointer to device operations structure
 * @param reg Register which is written
 * @param msg_len Length of handled I2C message
 *
 * @return TCPCI_EMUL_CONTINUE to continue with default handler
 * @return TCPCI_EMUL_DONE to immedietly return success
 * @return TCPCI_EMUL_ERROR to immedietly return error
 */
enum tcpci_emul_ops_resp ps8xxx_emul_tcpci_handle_write(
					const struct emul *emul,
					const struct tcpci_emul_dev_ops *ops,
					int reg, int msg_len)
{
	switch (reg) {
	case PS8XXX_REG_I2C_DBG_EN:
	case PS8XXX_REG_MUX_IN_HPD_ASSERTION:
		return TCPCI_EMUL_DONE;
	default:
		return TCPCI_EMUL_CONTINUE;
	}
}

/**
 * @brief Function called on reset
 *
 * @param emul Pointer to TCPCI emulator
 * @param ops Pointer to device operations structure
 */
void ps8xxx_emul_tcpci_reset(const struct emul *emul,
			     struct tcpci_emul_dev_ops *ops)
{
	/* TODO: Check for other than PS8805 */
	tcpci_emul_set_reg(emul, PS8XXX_REG_I2C_DBG_EN, 0x31);
	tcpci_emul_set_reg(emul, PS8XXX_REG_MUX_IN_HPD_ASSERTION, 0x00);
}

/** TCPCI specific device operations. Not all of them need to be implemented. */
struct tcpci_emul_dev_ops ps8xxx_emul_ops = {
	.read_byte = ps8xxx_emul_tcpci_read_byte,
	.write_byte = ps8xxx_emul_tcpci_write_byte,
	.handle_write = ps8xxx_emul_tcpci_handle_write,
	.reset = ps8xxx_emul_tcpci_reset,
};

static enum ps8xxx_emul_port ps8xxx_emul_get_port(struct i2c_emul *i2c_emul)
{
	const struct ps8xxx_emul_cfg *cfg;
	const struct emul *emul;

	emul = i2c_emul->parent;
	cfg = emul->cfg;

	if (cfg->p0_cfg.addr == i2c_emul->addr) {
		return PS8XXX_EMUL_PORT_0;
	}

	if (cfg->p1_cfg.addr == i2c_emul->addr) {
		return PS8XXX_EMUL_PORT_1;
	}

	return PS8XXX_EMUL_PORT_INVAL;
}

/**
 * @brief Function called for each byte of read message
 *
 * @param i2c_emul Pointer to PS8xxx emulator
 * @param reg First byte of last write message
 * @param val Pointer where byte to read should be stored
 * @param bytes Number of bytes already readded
 *
 * @return 0 on success
 */
static int ps8xxx_emul_read_byte(struct i2c_emul *i2c_emul, int reg,
				 uint8_t *val, int bytes)
{
	struct ps8xxx_emul_data *data;
	enum ps8xxx_emul_port port;
	const struct emul *emul;

	emul = i2c_emul->parent;
	data = emul->data;

	port = ps8xxx_emul_get_port(i2c_emul);

	if (bytes != 0) {
		LOG_ERR("Reading more than one byte at once");
		return -EIO;
	}

	switch (port) {
	case PS8XXX_EMUL_PORT_0:
		if (reg == PS8805_P0_REG_CHIP_REVISION) {
			*val = data->chip_rev;
			return 0;
		}
		break;
	case PS8XXX_EMUL_PORT_1:
		if (reg == PS8XXX_P1_REG_MUX_USB_DCI_CFG) {
			*val = data->dci_cfg;
			return 0;
		}
	case PS8XXX_EMUL_PORT_INVAL:
		LOG_ERR("Invalid I2C address");
		return -EIO;
	}

	LOG_ERR("Reading from reg 0x%x which is WO or undefined", reg);
	return -EIO;
}

/**
 * @brief Function called for each byte of write message
 *
 * @param i2c_emul Pointer to PS8xxx emulator
 * @param reg First byte of write message
 * @param val Received byte of write message
 * @param bytes Number of bytes already received
 *
 * @return 0 on success
 * @return -EIO on invalid write to TX buffer
 */
static int ps8xxx_emul_write_byte(struct i2c_emul *i2c_emul, int reg,
				  uint8_t val, int bytes)
{
	struct ps8xxx_emul_data *data;
	enum ps8xxx_emul_port port;
	const struct emul *emul;

	emul = i2c_emul->parent;
	data = emul->data;

	port = ps8xxx_emul_get_port(i2c_emul);

	if (bytes != 1) {
		LOG_ERR("Writing more than one byte at once");
		return -EIO;
	}

	switch (port) {
	case PS8XXX_EMUL_PORT_0:
		break;
	case PS8XXX_EMUL_PORT_1:
		if (reg == PS8XXX_P1_REG_MUX_USB_DCI_CFG) {
			data->dci_cfg = val;
			return 0;
		}
	case PS8XXX_EMUL_PORT_INVAL:
		LOG_ERR("Invalid I2C address");
		return -EIO;
	}

	LOG_ERR("Writing to reg 0x%x which is RO or undefined", reg);
	return -EIO;
}

/**
 * @brief Set up a new ps8xxx emulator secondary i2c device
 *
 * This should be called for each ps8xxx secondary device that needs to be
 * emulated. It registers it with the I2C emulation controller.
 *
 * @param emul Emulation information
 * @param parent Device to emulate
 *
 * @return 0 indicating success (always)
 */
static int ps8xxx_emul_init(const struct emul *emul,
			    const struct device *parent)
{
	const struct ps8xxx_emul_cfg *cfg = emul->cfg;
	struct ps8xxx_emul_data *data = emul->data;
	const struct device *i2c_dev;
	int ret;

	data->tcpci_emul = emul_get_binding(cfg->tcpci_emul);
	i2c_dev = parent;

	data->p0_data.emul.api = &i2c_common_emul_api;
	data->p0_data.emul.addr = cfg->p0_cfg.addr;
	data->p0_data.emul.parent = emul;
	data->p0_data.i2c = i2c_dev;
	data->p0_data.cfg = &cfg->p0_cfg;
	i2c_common_emul_init(&data->p0_data);

	data->p1_data.emul.api = &i2c_common_emul_api;
	data->p1_data.emul.addr = cfg->p1_cfg.addr;
	data->p1_data.emul.parent = emul;
	data->p1_data.i2c = i2c_dev;
	data->p1_data.cfg = &cfg->p1_cfg;
	i2c_common_emul_init(&data->p1_data);

	ret = i2c_emul_register(i2c_dev, emul->dev_label, &data->p0_data.emul);
	ret |= i2c_emul_register(i2c_dev, emul->dev_label, &data->p1_data.emul);

	tcpci_emul_set_dev_ops(data->tcpci_emul, &ps8xxx_emul_ops);
	ps8xxx_emul_tcpci_reset(data->tcpci_emul, &ps8xxx_emul_ops);

	return ret;
}

#define PS8XXX_EMUL(n)							\
	static struct ps8xxx_emul_data ps8xxx_emul_data_##n = {		\
		.prod_id = 0,						\
		.p0_data = {						\
			.write_byte = ps8xxx_emul_write_byte,		\
			.read_byte = ps8xxx_emul_read_byte,		\
		},							\
		.p1_data = {						\
			.write_byte = ps8xxx_emul_write_byte,		\
			.read_byte = ps8xxx_emul_read_byte,		\
		},							\
	};								\
									\
	static const struct ps8xxx_emul_cfg ps8xxx_emul_cfg_##n = {	\
		.tcpci_emul = DT_LABEL(DT_INST_PHANDLE(n, tcpci_i2c)),	\
		.p0_cfg = {						\
			.i2c_label = DT_INST_BUS_LABEL(n),		\
			.dev_label = DT_INST_LABEL(n),			\
			.data = &ps8xxx_emul_data_##n.p0_data,		\
			.addr = DT_INST_PROP(n, p0_i2c_addr),		\
		},							\
		.p1_cfg = {						\
			.i2c_label = DT_INST_BUS_LABEL(n),		\
			.dev_label = DT_INST_LABEL(n),			\
			.data = &ps8xxx_emul_data_##n.p1_data,		\
			.addr = DT_INST_PROP(n, p1_i2c_addr),		\
		},							\
	};								\
	EMUL_DEFINE(ps8xxx_emul_init, DT_DRV_INST(n),			\
		    &ps8xxx_emul_cfg_##n, &ps8xxx_emul_data_##n)

DT_INST_FOREACH_STATUS_OKAY(PS8XXX_EMUL)
