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
	/* SM5803 has three I2C addresses, each with different functions. */
	struct i2c_common_emul_data i2c_main;
	struct i2c_common_emul_data i2c_meas;
	struct i2c_common_emul_data i2c_chg;

	uint8_t fast_charge_current_limit;
};

struct sm5803_emul_cfg {
	const struct i2c_common_emul_cfg i2c_main;
	const struct i2c_common_emul_cfg i2c_meas;
	const struct i2c_common_emul_cfg i2c_chg;
};

static int i2c_main_write_byte(const struct emul *emul, int reg, uint8_t val,
			       int bytes)
{
	return ENOTSUP;
}

static int i2c_main_finish_write(const struct emul *emul, int reg, int bytes)
{
	return ENOTSUP;
}

static int i2c_main_read_byte(const struct emul *emul, int reg, uint8_t *val,
			      int bytes)
{
	return ENOTSUP;
}

static int i2c_meas_write_byte(const struct emul *emul, int reg, uint8_t val,
			       int bytes)
{
	return ENOTSUP;
}

static int i2c_meas_finish_write(const struct emul *emul, int reg, int bytes)
{
	return ENOTSUP;
}

static int i2c_meas_read_byte(const struct emul *emul, int reg, uint8_t *val,
			      int bytes)
{
	return ENOTSUP;
}

static int i2c_chg_write_byte(const struct emul *emul, int reg, uint8_t val,
			      int bytes)
{
	struct sm5803_emul_data *data = emul->data;
	__ASSERT(bytes == 1, "Writes must send only one byte");

	switch (reg) {
	case SM5803_REG_FAST_CONF4:
		data->fast_charge_current_limit = val & GENMASK(5, 0);
		return 0;


	}
	return ENOTSUP;
}

static int i2c_chg_finish_write(const struct emul *emul, int reg, int bytes)
{
	return ENOTSUP;
}

static int i2c_chg_read_byte(const struct emul *emul, int reg, uint8_t *val,
			     int bytes)
{
	struct sm5803_emul_data *data = emul->data;

	__ASSERT(bytes == 1, "register reads are 1 byte only");

	switch (reg) {
	case SM5803_REG_FAST_CONF4:
		*val = GENMASK(7, 6) | data->fast_charge_current_limit;
		return 0;

	}
	return ENOTSUP;
}

static int sm5803_emul_init(const struct emul *emul,
			    const struct device *parent)
{
	struct sm5803_emul_data *data = emul->data;

	data->i2c_main.i2c = parent;
	i2c_common_emul_init(&data->i2c_main);
	data->i2c_meas.i2c = parent;
	i2c_common_emul_init(&data->i2c_meas);
	data->i2c_chg.i2c = parent;
	i2c_common_emul_init(&data->i2c_chg);

	data->fast_charge_current_limit = 0;

	return 0;
}

#define SM5803_EMUL_I2C_DATA(page, n)                      \
	.i2c_##page = {                                    \
		.write_byte = i2c_##page##_write_byte,     \
		.read_byte = i2c_##page##_read_byte,       \
		.finish_write = i2c_##page##_finish_write, \
	}

#define SM5803_EMUL_I2C_CFG(page, _addr, n)                      \
	.i2c_##page = {                                         \
		.dev_label = DT_NODE_FULL_NAME(DT_DRV_INST(n)), \
		.addr = _addr,                                   \
	}

#define INIT_SM5803(n)                                                  \
	static struct sm5803_emul_data sm5803_emul_data_##n = {         \
		SM5803_EMUL_I2C_DATA(main, n),                          \
		SM5803_EMUL_I2C_DATA(meas, n),                          \
		SM5803_EMUL_I2C_DATA(chg, n),                           \
	};                                                              \
	static struct sm5803_emul_cfg sm5803_emul_cfg_##n = {           \
		SM5803_EMUL_I2C_CFG(main, 0x30, n),                     \
		SM5803_EMUL_I2C_CFG(meas, 0x31, n),                     \
		SM5803_EMUL_I2C_CFG(chg, DT_INST_REG_ADDR(n), n),       \
	};                                                              \
	EMUL_DT_INST_DEFINE(n, sm5803_emul_init, &sm5803_emul_data_##n, \
			    &sm5803_emul_cfg_##n, &i2c_common_emul_api, NULL);

DT_INST_FOREACH_STATUS_OKAY(INIT_SM5803)
DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE);

static void sm5803_emul_reset(struct sm5803_emul_data *data)
{
	data->i2c_main.write_fail_reg = I2C_COMMON_EMUL_NO_FAIL_REG;
	data->i2c_main.read_fail_reg = I2C_COMMON_EMUL_NO_FAIL_REG;
	data->i2c_meas.write_fail_reg = I2C_COMMON_EMUL_NO_FAIL_REG;
	data->i2c_meas.read_fail_reg = I2C_COMMON_EMUL_NO_FAIL_REG;
	data->i2c_chg.write_fail_reg = I2C_COMMON_EMUL_NO_FAIL_REG;
	data->i2c_chg.read_fail_reg = I2C_COMMON_EMUL_NO_FAIL_REG;
	data->fast_charge_current_limit = 0;
}

static void sm5803_emul_reset_before(const struct ztest_unit_test *test,
				     void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

#define SM5803_EMUL_RESET_RULE_AFTER(n) sm5803_emul_reset(&sm5803_emul_data_##n)
	DT_INST_FOREACH_STATUS_OKAY(SM5803_EMUL_RESET_RULE_AFTER);
}
ZTEST_RULE(sm5803_emul_reset, sm5803_emul_reset_before, NULL);
