/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT zephyr_pi3usb9201

#define LOG_LEVEL CONFIG_I2C_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(emul_pi3usb9201);

#include <device.h>
#include <emul.h>
#include <drivers/i2c.h>
#include <drivers/i2c_emul.h>

#include "emul/emul_pi3usb9201.h"

/**
 * Describe if there is no ongoing I2C message or if there is message handled
 * at the moment (last message doesn't ended with stop or write is not followed
 * by read).
 */
enum pi3usb9201_emul_msg_state {
	PI3USB9201_EMUL_NONE_MSG,
	PI3USB9201_EMUL_IN_WRITE,
	PI3USB9201_EMUL_IN_READ
};

/** Run-time data used by the emulator */
struct pi3usb9201_emul_data {
	/** I2C emulator detail */
	struct i2c_emul emul;
	/** pi3usb9201 device being emulated */
	const struct device *i2c;
	/** Configuration information */
	const struct pi3usb9201_emul_cfg *cfg;

	/** Current state of all emulated pi3usb9201 registers */
	uint8_t reg[0x40];

	/** Current state of I2C bus (if emulator is handling message) */
	enum pi3usb9201_emul_msg_state msg_state;
	/** Number of already handled bytes in ongoing message */
	int msg_byte;
	/** Register selected in last write command */
	uint8_t cur_reg;
	/** Value of data byte in ongoing write message */
	uint8_t write_byte;

	/** Mutex used to control access to emulator data */
	struct k_mutex data_mtx;
};

/** Static configuration for the emulator */
struct pi3usb9201_emul_cfg {
	/** Label of the I2C bus this emulator connects to */
	const char *i2c_label;
	/** Pointer to run-time data */
	struct pi3usb9201_emul_data *data;
	/** Address of pi3usb9201 on i2c bus */
	uint16_t addr;
};

void pi3usb9201_emul_set_reg(struct i2c_emul *emul, int reg, uint8_t val)
{
	struct pi3usb9201_emul_data *data;

	if (reg < 0 || reg > PI3USB9201_REG_HOST_STS) {
		return;
	}

	data = CONTAINER_OF(emul, struct pi3usb9201_emul_data, emul);
	data->reg[reg] = val;
}

static void pi3usb9201_emul_reset(struct i2c_emul *emul)
{
	LOG_INF("pi3usb9201_emul_reset");
}

static int pi3usb9201_emul_handle_write(struct i2c_emul *emul, int reg, uint8_t val)
{
	struct pi3usb9201_emul_data *data;

	data = CONTAINER_OF(emul, struct pi3usb9201_emul_data, emul);

	LOG_INF("Writing 0x%x to reg 0x%x", val, reg);

	data->reg[reg] = val;

	return 0;
}

static int pi3usb9201_emul_handle_read(struct i2c_emul *emul, int reg, char *buf)
{
	struct pi3usb9201_emul_data *data;

	data = CONTAINER_OF(emul, struct pi3usb9201_emul_data, emul);

	LOG_INF("Reading reg 0x%x", reg);

	*buf = data->reg[reg];

	return 0;
}

/**
 * Emulate an I2C transfer to a pi3usb9201
 *
 * This handles simple reads and writes
 *
 * @param emul I2C emulation information
 * @param msgs List of messages to process
 * @param num_msgs Number of messages to process
 * @param addr Address of the I2C target device
 *
 * @retval 0 If successful
 * @retval -EIO General input / output error
 */
static int pi3usb9201_emul_transfer(struct i2c_emul *emul, struct i2c_msg *msgs,
			     int num_msgs, int addr)
{
	const struct pi3usb9201_emul_cfg *cfg;
	struct pi3usb9201_emul_data *data;
	unsigned int len;
	int ret, i, reg;
	bool read;

	data = CONTAINER_OF(emul, struct pi3usb9201_emul_data, emul);
	cfg = data->cfg;

	if (cfg->addr != addr) {
		LOG_ERR("Address mismatch, expected %02x, got %02x", cfg->addr,
			addr);
		return -EIO;
	}

	i2c_dump_msgs("emul", msgs, num_msgs, addr);

	for (; num_msgs > 0; num_msgs--, msgs++) {
		read = msgs->flags & I2C_MSG_READ;

		switch (data->msg_state) {
		case PI3USB9201_EMUL_NONE_MSG:
			data->msg_byte = 0;
			break;
		case PI3USB9201_EMUL_IN_WRITE:
			if (read) {
				/* Finish write command */
				if (data->msg_byte == 2) {
					k_mutex_lock(&data->data_mtx,
						     K_FOREVER);
					ret = pi3usb9201_emul_handle_write(emul,
							data->cur_reg,
							data->write_byte);
					k_mutex_unlock(&data->data_mtx);
					if (ret) {
						return -EIO;
					}
				}
				data->msg_byte = 0;
			}
			break;
		case PI3USB9201_EMUL_IN_READ:
			if (!read) {
				data->msg_byte = 0;
			}
			break;
		}
		data->msg_state = read ? PI3USB9201_EMUL_IN_READ : PI3USB9201_EMUL_IN_WRITE;

		if (msgs->flags & I2C_MSG_STOP) {
			data->msg_state = PI3USB9201_EMUL_NONE_MSG;
		}

		if (!read) {
			/* Dispatch wrtie command */
			for (i = 0; i < msgs->len; i++) {
				switch (data->msg_byte) {
				case 0:
					data->cur_reg = msgs->buf[i];
					break;
				case 1:
					data->write_byte = msgs->buf[i];
					break;
				default:
					data->msg_state = PI3USB9201_EMUL_NONE_MSG;
					LOG_ERR("Too long write command");
					return -EIO;
				}
				data->msg_byte++;
			}

			/* Execute write command */
			if (msgs->flags & I2C_MSG_STOP && data->msg_byte == 2) {
				k_mutex_lock(&data->data_mtx, K_FOREVER);
				ret = pi3usb9201_emul_handle_write(emul, data->cur_reg,
							    data->write_byte);
				k_mutex_unlock(&data->data_mtx);
				if (ret) {
					return -EIO;
				}
			}
		} else {
			/* Dispatch read command */
			for (i = 0; i < msgs->len; i++) {
				reg = data->cur_reg + data->msg_byte;
				data->msg_byte++;
				k_mutex_lock(&data->data_mtx, K_FOREVER);
				ret = pi3usb9201_emul_handle_read(emul, reg,
							   &(msgs->buf[i]));
				k_mutex_unlock(&data->data_mtx);
				if (ret) {
					return -EIO;
				}
			}
		}
	}

	return 0;
}

/* Device instantiation */

static struct i2c_emul_api pi3usb9201_emul_api = {
	.transfer = pi3usb9201_emul_transfer,
};

/**
 * @brief Set up a new pi3usb9201 emulator
 *
 * This should be called for each pi3usb9201 device that needs to be
 * emulated. It registers it with the I2C emulation controller.
 *
 * @param emul Emulation information
 * @param parent Device to emulate
 *
 * @return 0 indicating success (always)
 */
static int pi3usb9201_emul_init(const struct emul *emul,
			 const struct device *parent)
{
	const struct pi3usb9201_emul_cfg *cfg = emul->cfg;
	struct pi3usb9201_emul_data *data = cfg->data;
	int ret;

	data->emul.api = &pi3usb9201_emul_api;
	data->emul.addr = cfg->addr;
	data->i2c = parent;
	data->cfg = cfg;
	k_mutex_init(&data->data_mtx);

	ret = i2c_emul_register(parent, emul->dev_label, &data->emul);

	pi3usb9201_emul_reset(&data->emul);

	return ret;
}

#define PI3USB9201_EMUL(n)							\
	static struct pi3usb9201_emul_data pi3usb9201_emul_data_##n = {		\
		.msg_state = PI3USB9201_EMUL_NONE_MSG,				\
		.cur_reg = 0,						\
	};								\
									\
	static const struct pi3usb9201_emul_cfg pi3usb9201_emul_cfg_##n = {		\
		.i2c_label = DT_INST_BUS_LABEL(n),			\
		.data = &pi3usb9201_emul_data_##n,				\
		.addr = DT_INST_REG_ADDR(n),				\
	};								\
	EMUL_DEFINE(pi3usb9201_emul_init, DT_DRV_INST(n), &pi3usb9201_emul_cfg_##n)

DT_INST_FOREACH_STATUS_OKAY(PI3USB9201_EMUL)

#define PI3USB9201_EMUL_CASE(n)					\
	case DT_INST_DEP_ORD(n): return &pi3usb9201_emul_data_##n.emul;

struct i2c_emul *pi3usb9201_emul_get(int ord)
{
	switch (ord) {
	DT_INST_FOREACH_STATUS_OKAY(PI3USB9201_EMUL_CASE)

	default:
		return NULL;
	}
}
