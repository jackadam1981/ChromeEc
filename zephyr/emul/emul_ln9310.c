/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_ln9310_emul

#include <device.h>
#include <drivers/i2c.h>
#include <drivers/i2c_emul.h>
#include <emul.h>

#include "driver/ln9310.h"
#include "emul/emul_common_i2c.h"
#include "i2c.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(ln9310_emul);

#define LN9310_DATA_FROM_I2C_EMUL(_emul)                                     \
	CONTAINER_OF(CONTAINER_OF(_emul, struct i2c_common_emul_data, emul), \
		     struct ln9310_emul_data, common)

struct ln9310_emul_data {
	/** Common I2C data */
	struct i2c_common_emul_data common;
	/** The current emulated battery cell type */
	enum battery_cell_type battery_cell_type;
};

static const struct emul *singleton;

void ln9310_set_battery_cell_type(const struct emul *emul,
				  enum battery_cell_type type)
{
	struct ln9310_emul_data *data = emul->data;

	data->battery_cell_type = type;
}

enum battery_cell_type board_get_battery_cell_type(void)
{
	struct ln9310_emul_data *data = singleton->data;

	return data->battery_cell_type;
}

static struct i2c_emul_api ln9310_emul_api_i2c = {
	.transfer = i2c_common_emul_transfer,
};

static int ln9310_emul_start_write(struct i2c_emul *emul, int reg)
{
	LOG_INF("%s(reg=0x%x)", __func__, reg);
	return 0;
}

static int ln9310_emul_finish_write(struct i2c_emul *emul, int reg, int bytes)
{
	LOG_INF("%s(reg=0x%x, bytes=%d)", __func__, reg, bytes);
	return 0;
}

static int ln9310_emul_write_byte(struct i2c_emul *emul, int reg, uint8_t val,
				  int bytes)
{
	LOG_INF("%s(reg=0x%x, val=%u, bytes=%d)", __func__, reg, val, bytes);
	return 0;
}

static int ln9310_emul_start_read(struct i2c_emul *emul, int reg)
{
	LOG_INF("%s(reg=0x%x)", __func__, reg);
	return 0;
}

static int ln9310_emul_finish_read(struct i2c_emul *emul, int reg, int bytes)
{
	LOG_INF("%s(reg=0x%x, bytes=%d)", __func__, reg, bytes);
	return 0;
}

static int ln9310_emul_read_byte(struct i2c_emul *emul, int reg, uint8_t *val,
				 int bytes)
{
	LOG_INF("%s(reg=0x%x, bytes=%d)", __func__, reg, bytes);
	return 0;
}

static int ln9310_emul_access_reg(struct i2c_emul *emul, int reg, int bytes,
				  bool read)
{
	LOG_INF("%s(reg=0x%x, bytes=%d, read=%d)", __func__, reg, bytes, read);
	return 0;
}

/*
static int ln9310_emul_read(struct i2c_emul *emul, int reg, uint8_t *val,
			    int bytes, void *data)
{
	LOG_INF("ln9310_emul_read()");
	return 0;
}
static int ln9310_emul_write(struct i2c_emul *emul, int reg, uint8_t val,
			     int bytes, void *data)
{
	LOG_INF("ln9310_emul_write()");
	return 0;
}
*/

static int emul_ln9310_init(const struct emul *emul,
			    const struct device *parent)
{
	const struct i2c_common_emul_cfg *cfg = emul->cfg;
	struct ln9310_emul_data *data = emul->data;

	data->common.emul.api = &ln9310_emul_api_i2c;
	data->common.emul.addr = cfg->addr;
	data->common.emul.parent = emul;
	data->common.i2c = parent;
	data->common.cfg = cfg;
	i2c_common_emul_init(&data->common);

	singleton = emul;

	return i2c_emul_register(parent, emul->dev_label, &data->common.emul);
}

#define INIT_LN9310(n)                                                         \
	const struct ln9310_config_t ln9310_config = {                         \
		.i2c_port = NAMED_I2C(power),                                  \
		.i2c_addr_flags = DT_INST_REG_ADDR(n),                         \
	};                                                                     \
	static struct ln9310_emul_data ln9310_emul_data_##n = {                \
		.battery_cell_type = BATTERY_CELL_TYPE_UNKNOWN,                \
		.common = {                                                    \
			.start_write = ln9310_emul_start_write,                \
			.write_byte = ln9310_emul_write_byte,                  \
			.finish_write = ln9310_emul_finish_write,              \
			.start_read = ln9310_emul_start_read,                  \
			.read_byte = ln9310_emul_read_byte,                    \
			.finish_read = ln9310_emul_finish_read,                \
			.access_reg = ln9310_emul_access_reg,                  \
		},                                                             \
	};                                                                     \
	static const struct i2c_common_emul_cfg ln9310_emul_cfg_##n = {        \
		.i2c_label = DT_INST_BUS_LABEL(n),                             \
		.dev_label = DT_INST_LABEL(n),                                 \
		.addr = DT_INST_REG_ADDR(n),                                   \
	};                                                                     \
	EMUL_DEFINE(emul_ln9310_init, DT_DRV_INST(n), &ln9310_emul_cfg_##n,    \
		    &ln9310_emul_data_##n);

DT_INST_FOREACH_STATUS_OKAY(INIT_LN9310);
