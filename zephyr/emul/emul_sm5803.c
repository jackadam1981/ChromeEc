/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "driver/charger/sm5803.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_sm5803.h"
#include "emul/emul_stub_device.h"

#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#define DT_DRV_COMPAT cros_sm5803_emul

LOG_MODULE_REGISTER(sm5803_emul, CONFIG_SM5803_EMUL_LOG_LEVEL);

struct sm5803_emul_data {
	struct i2c_common_emul_data i2c_main;
	struct i2c_common_emul_data i2c_chg;

	uint8_t fast_charge_current_limit;
};

struct sm5803_emul_cfg {
	const struct i2c_common_emul_cfg i2c_main;
	const struct i2c_common_emul_cfg i2c_chg;
};

static void sm5803_emul_reset(const struct device *device)
{
	struct sm5803_emul_data *data = device->data;

	data->fast_charge_current_limit = 0;
}

int sm5803_main_read_byte(const struct emul *target, int reg, uint8_t *val,
			  int bytes)
{
	LOG_INF("sm5803 main read byte");
	return ENOTSUP;
}

int sm5803_main_write_byte(const struct emul *target, int reg, uint8_t val,
			   int bytes)
{
	return ENOTSUP;
}

int sm5803_chg_read_byte(const struct emul *target, int reg, uint8_t *val,
			 int bytes)
{
	struct sm5803_emul_data *data = target->data;

	switch (reg) {
	case SM5803_REG_FAST_CONF4:
		*val = data->fast_charge_current_limit | GENMASK(7, 6);
		return 0;
	}
	return ENOTSUP;
}

int sm5803_chg_write_byte(const struct emul *target, int reg, uint8_t val,
			  int bytes)
{
	struct sm5803_emul_data *data = target->data;

	switch (reg) {
	case SM5803_REG_FAST_CONF4:
		data->fast_charge_current_limit = val & GENMASK(5, 0);
		return 0;
	}
	return ENOTSUP;
}

int sm5803_emul_i2c_transfer(const struct emul *target, struct i2c_msg *msgs,
			     int num_msgs, int addr)
{
	struct sm5803_emul_data *data = target->data;
	const struct sm5803_emul_cfg *cfg = target->cfg;

	if (addr == cfg->i2c_main.addr) {
		LOG_INF("i2c transfer delegate to i2c_main");
		return i2c_common_emul_transfer_workhorse(target,
							  &data->i2c_main,
							  &cfg->i2c_main, msgs,
							  num_msgs, addr);
	} else if (addr == cfg->i2c_chg.addr) {
		LOG_INF("i2c transfer delegate to i2c_chg");
		return i2c_common_emul_transfer_workhorse(target,
							  &data->i2c_chg,
							  &cfg->i2c_chg, msgs,
							  num_msgs, addr);
	}
	LOG_ERR("I2C transaction for address %#x not supported by SM5803 emulator",
		addr);
	return ENOTSUP;
}

const static struct i2c_emul_api sm5803_emul_api = {
	.transfer = sm5803_emul_i2c_transfer,
};

static int sm5803_emul_init(const struct emul *emul,
			    const struct device *parent)
{
	struct sm5803_emul_data *data = emul->data;
	// const struct sm5803_emul_cfg *cfg = emul->cfg;
	int rv;

	rv = i2c_emul_register(parent, &data->i2c_chg.emul);
	if (rv != 0) {
		k_oops();
	}
	i2c_common_emul_init(&data->i2c_chg);

	rv = i2c_emul_register(parent, &data->i2c_main.emul);
	if (rv != 0) {
		k_oops();
	}
	i2c_common_emul_init(&data->i2c_main);

	return 0;
}

#define INIT_SM5803(n)                                                       \
	const static struct sm5803_emul_cfg sm5803_emul_cfg_##n;             \
	static struct sm5803_emul_data sm5803_emul_data_##n = {              \
		.i2c_main =                                                  \
			(struct i2c_common_emul_data){                       \
				.i2c = DEVICE_DT_GET(DT_INST_PARENT(n)),     \
				.emul =                                      \
					(struct i2c_emul){                   \
						.target = EMUL_DT_GET(       \
							DT_DRV_INST(n)),     \
						.api = &sm5803_emul_api,     \
						.addr = DT_INST_REG_ADDR(n), \
					},                                   \
				.cfg = &sm5803_emul_cfg_##n.i2c_main,        \
				.read_byte = &sm5803_main_read_byte,         \
				.write_byte = &sm5803_main_write_byte,       \
			},                                                   \
		.i2c_chg =                                                   \
			(struct i2c_common_emul_data){                       \
				.i2c = DEVICE_DT_GET(DT_INST_PARENT(n)),     \
				.emul =                                      \
					(struct i2c_emul){                   \
						.target = EMUL_DT_GET(       \
							DT_DRV_INST(n)),     \
						.api = &sm5803_emul_api,     \
						.addr = DT_INST_PROP(        \
							n, chg_addr),        \
					},                                   \
				.cfg = &sm5803_emul_cfg_##n.i2c_chg,         \
				.read_byte = &sm5803_chg_read_byte,          \
				.write_byte = &sm5803_chg_write_byte,        \
			}                                                    \
	};                                                                   \
	const static struct sm5803_emul_cfg sm5803_emul_cfg_##n = {          \
		.i2c_main =                                                  \
			(struct i2c_common_emul_cfg){                        \
				.dev_label =                                 \
					DT_NODE_FULL_NAME(DT_DRV_INST(n)),   \
				.addr = DT_INST_REG_ADDR(n),                 \
				.data = &sm5803_emul_data_##n.i2c_main,      \
			},                                                   \
		.i2c_chg =                                                   \
			(struct i2c_common_emul_cfg){                        \
				.dev_label =                                 \
					DT_NODE_FULL_NAME(DT_DRV_INST(n)),   \
				.addr = DT_INST_PROP(n, chg_addr),          \
				.data = &sm5803_emul_data_##n.i2c_chg,       \
			},                                                   \
	};                                                                   \
	EMUL_DT_INST_DEFINE(n, sm5803_emul_init, &sm5803_emul_data_##n,      \
			    &sm5803_emul_cfg_##n, &sm5803_emul_api, NULL);

DT_INST_FOREACH_STATUS_OKAY(INIT_SM5803)
DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE)

static void sm5803_emul_reset_before(const struct ztest_unit_test *test,
				     void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

#define SM5803_EMUL_RESET_RULE_BEFORE(n) \
	sm5803_emul_reset(DEVICE_DT_GET(DT_DRV_INST(n)));

	DT_INST_FOREACH_STATUS_OKAY(SM5803_EMUL_RESET_RULE_BEFORE);
}
ZTEST_RULE(sm5803_emul_reset, sm5803_emul_reset_before, NULL);
