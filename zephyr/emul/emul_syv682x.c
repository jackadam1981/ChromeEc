/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT zephyr_syv682x

#define LOG_LEVEL CONFIG_I2C_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(syv682x);

#include <device.h>
#include <emul.h>
#include <drivers/i2c.h>
#include <drivers/i2c_emul.h>

#include "emul/emul_smart_battery.h"

#include "crc8.h"
#include "battery_smart.h"

struct syv682x_emul_data {
	/** I2C emulator detail */
	struct i2c_emul emul;
	/** Smart battery device being emulated */
	const struct device *i2c;
	/** Configuration information */
	const struct syv682x_emul_cfg *cfg;

	/** Current state of all emulated SYV682x registers */
	uint8_t reg[5];
};

/** Static configuration for the emulator */
struct syv682x_emul_cfg {
	/** Label of the I2C bus this emulator connects to */
	const char *i2c_label;
	/** Address of smart battery on i2c bus */
	uint16_t addr;
	struct syv682x_emul_data *data;
};

/**
 * Emulator an I2C transfer to an SYV682x
 *
 * This handles simple reads and writes
 *
 * @param emul I2C emulation information
 * @param msgs List of messages to process. For 'read' messages, this function
 *             updates the 'buf' member with the data that was read
 * @param num_msgs Number of messages to process
 * @param addr Address of the I2C target device.
 *
 * @retval 0 If successful
 * @retval -EIO General input / output error
 */
static int syv682x_emul_transfer(struct i2c_emul *emul, struct i2c_msg *msgs,
			      int num_msgs, int addr)
{
	LOG_ERR("Emul transfer");
#if 0
	const struct sbat_emul_cfg *cfg;
	struct sbat_emul_data *data;
	unsigned int len;
	int ret = 0, i;

	data = CONTAINER_OF(emul, struct sbat_emul_data, emul);
	cfg = data->cfg;

	if (cfg->addr != addr) {
		LOG_ERR("Address mismatch, expected %02x, got %02x", cfg->addr,
			addr);
		return -EIO;
	}

	i2c_dump_msgs("emul", msgs, num_msgs, addr);

	for (; num_msgs > 0; num_msgs--, msgs++) {
		if (!(msgs->flags & I2C_MSG_READ)) {
			/* Disscard any ongoing read */
			data->num_to_read = 0;

			/* Save incoming data to buffer */
			for (i = 0; i < msgs->len; i++, data->ong_write++) {
				if (data->ong_write < MSG_BUF_LEN) {
					data->msg_buf[data->ong_write] =
								msgs->buf[i];
				}
			}

			/* Handle write message when we receive stop signal */
			if (msgs->flags & I2C_MSG_STOP) {
				k_mutex_lock(&data->bat_mtx, K_FOREVER);
				ret = sbat_emul_finalize_write_msg(emul);
				k_mutex_unlock(&data->bat_mtx);
			}
		} else {
			/* Finalize any ongoing write message before read */
			if (data->ong_write) {
				k_mutex_lock(&data->bat_mtx, K_FOREVER);
				ret = sbat_emul_finalize_write_msg(emul);
				k_mutex_unlock(&data->bat_mtx);
				if (ret)
					return -EIO;
			}

			/* Prepare read message */
			if (!data->num_to_read) {
				k_mutex_lock(&data->bat_mtx, K_FOREVER);
				ret = sbat_emul_handle_read_msg(emul);
				k_mutex_unlock(&data->bat_mtx);
				data->cur_cmd = SBAT_EMUL_NO_CMD;
				data->ong_read = 0;
			}

			for (i = 0; i < msgs->len; i++, data->ong_read++) {
				if (data->ong_read >= data->num_to_read) {
					/* We wrote everything */
					data->num_to_read = 0;
					break;
				}
				msgs->buf[i] = data->msg_buf[data->ong_read];
			}

			if (msgs->flags & I2C_MSG_STOP) {
				/* Disscard any data that wern't read */
				data->num_to_read = 0;
			}
		}

		if (ret)
			return -EIO;
	}

#endif
	return 0;
}

/* Device instantiation */

static struct i2c_emul_api syv682x_emul_api = {
	.transfer = syv682x_emul_transfer,
};

/**
 * @brief Set up a new SYV682x emulator
 *
 * This should be called for each SYV682x device that needs to be emulated. It
 * registers it with the I2C emulation controller.
 *
 * @param emul Emulation information
 * @param parent Device to emulate
 *
 * @return 0 indicating success (always)
 */
static int syv682x_emul_init(const struct emul *emul,
			  const struct device *parent)
{
	const struct syv682x_emul_cfg *cfg = emul->cfg;
	struct syv682x_emul_data *data = cfg->data;
	int ret;

	data->emul.api = &syv682x_emul_api;
	data->emul.addr = cfg->addr;
	data->i2c = parent;
	data->cfg = cfg;

	ret = i2c_emul_register(parent, emul->dev_label, &data->emul);

	return ret;
}

#define SYV682X_EMUL(n)						\
	static struct syv682x_emul_data syv682x_emul_data_##n = {	\
	};								\
									\
	static const struct syv682x_emul_cfg syv682x_emul_cfg_##n = {	\
		.i2c_label = DT_INST_BUS_LABEL(n),			\
		.data = &syv682x_emul_data_##n,				\
		.addr = DT_INST_REG_ADDR(n),				\
	};								\
	EMUL_DEFINE(syv682x_emul_init, DT_DRV_INST(n), &syv682x_emul_cfg_##n)

DT_INST_FOREACH_STATUS_OKAY(SYV682X_EMUL)

#define SYV682X_EMUL_CASE(n)					\
	case DT_INST_DEP_ORD(n): return &syv682x_emul_data_##n.emul;


struct i2c_emul *syv682x_emul_get_ptr(int ord)
{
	switch (ord) {
	DT_INST_FOREACH_STATUS_OKAY(SYV682X_EMUL_CASE)

	default:
		return NULL;
	}
}
