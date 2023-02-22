/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "driver/charger/sm5803.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_sm5803.h"
#include "emul/emul_stub_device.h"

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#define DT_DRV_COMPAT cros_sm5803_chg_emul

#define DT_DRV_COMPAT cros_sm5803_chg_emul
LOG_MODULE_DECLARE(sm5803_emul, CONFIG_SM5803_EMUL_LOG_LEVEL);

struct sm5803_emul_chg_data {
	struct i2c_common_emul_data i2c;
	uint8_t fast_charge_current_limit;
};

struct sm5803_emul_chg_cfg {
	const struct emul *base;
	const struct i2c_common_emul_cfg i2c;
};

static int i2c_write_byte(const struct emul *emul, int reg, uint8_t val,
			  int bytes)
{
	struct sm5803_emul_chg_data *data = emul->data;
	__ASSERT(bytes == 1, "Writes must send only one byte");

	switch (reg) {
	case SM5803_REG_FAST_CONF4:
		data->fast_charge_current_limit = val & GENMASK(5, 0);
		return 0;
	}
	return ENOTSUP;
}

static int i2c_finish_write(const struct emul *emul, int reg, int bytes)
{
	return ENOTSUP;
}

static int i2c_read_byte(const struct emul *emul, int reg, uint8_t *val,
			 int bytes)
{
	struct sm5803_emul_chg_data *data = emul->data;

	__ASSERT(bytes == 1, "register reads are 1 byte only");

	switch (reg) {
	case SM5803_REG_FAST_CONF4:
		*val = GENMASK(7, 6) | data->fast_charge_current_limit;
		return 0;
	}
	return ENOTSUP;
}

void sm5803_chg_emul_reset(const struct emul *emul)
{
	struct sm5803_emul_chg_data *data = emul->data;

	data->fast_charge_current_limit = 0;
}

static int sm5803_chg_emul_init(const struct emul *emul,
				const struct device *parent)
{
	struct sm5803_emul_chg_data *data = emul->data;

	data->i2c.i2c = parent;
	i2c_common_emul_init(&data->i2c);

	data->fast_charge_current_limit = 0;
	return 0;
}

#define INIT_SM5803_CHG(n)                                                       \
	static struct sm5803_emul_chg_data sm5803_chg_emul_data_##n = {        \
                .i2c = {                                                \
			.write_byte = i2c_write_byte,                          \
			.read_byte = i2c_read_byte,                                      \
			.finish_write = i2c_finish_write,\
		},                                                        \
	}; \
	static struct sm5803_emul_chg_cfg sm5803_chg_emul_cfg_##n = {          \
                .i2c = {                                                \
			.dev_label = DT_NODE_FULL_NAME(DT_DRV_INST(n)),         \
			.addr = DT_INST_REG_ADDR(n),\
		},                                                                    \
		.base = EMUL_DT_GET(DT_INST_PHANDLE(n, emul_base)), \
	}; \
	EMUL_DT_INST_DEFINE(n, sm5803_chg_emul_init,                             \
			    &sm5803_chg_emul_data_##n,                           \
			    &sm5803_chg_emul_cfg_##n, &i2c_common_emul_api,      \
			    NULL);

DT_INST_FOREACH_STATUS_OKAY(INIT_SM5803_CHG)
DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE);
