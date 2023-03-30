/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "emul/emul_common_i2c.h"
#include "emul/emul_stub_device.h"

#include <zephyr/device.h>

#define DT_DRV_COMPAT zephyr_rt1739_emul

#define RT1739_REG_MAX 0x61

struct rt1739_data {
	struct i2c_common_emul_data common;
	uint8_t regs[RT1739_REG_MAX + 1];
};

void rt1739_emul_reset_regs(const struct emul *emul)
{
}

static int rt1739_emul_read(const struct emul *emul, int reg, uint8_t *val,
			    int bytes, void *unused_data)
{
	struct rt1739_data *data = emul->data;
	uint8_t *regs = data->regs;
	int pos = reg + bytes;

	if (!IN_RANGE(pos, 0, RT1739_REG_MAX)) {
		return -1;
	}
	*val = regs[pos];

	return 0;
}

static int rt1739_emul_write(const struct emul *emul, int reg, uint8_t val,
			     int bytes, void *unused_data)
{
	struct rt1739_data *data = emul->data;
	uint8_t *regs = data->regs;
	int pos = reg + bytes - 1;

	if (!IN_RANGE(pos, 0, RT1739_REG_MAX) || !IN_RANGE(val, 0, UINT8_MAX)) {
		return -1;
	}
	regs[pos] = val;

	return 0;
}

static int rt1739_emul_init(const struct emul *emul,
			    const struct device *parent)
{
	struct rt1739_data *data = (struct rt1739_data *)emul->data;
	struct i2c_common_emul_data *common_data = &data->common;

	i2c_common_emul_init(common_data);
	i2c_common_emul_set_read_func(common_data, rt1739_emul_read, NULL);
	i2c_common_emul_set_write_func(common_data, rt1739_emul_write, NULL);

	rt1739_emul_reset_regs(emul);
	return 0;
}

#define INIT_RT1739_EMUL(n)                                        \
	static struct i2c_common_emul_cfg common_cfg_##n;          \
	static struct rt1739_data rt1739_data_##n = {              \
		.common = { .cfg = &common_cfg_##n }               \
	};                                                         \
	static struct i2c_common_emul_cfg common_cfg_##n = {       \
		.dev_label = DT_NODE_FULL_NAME(DT_DRV_INST(n)),    \
		.data = &rt1739_data_##n.common,                   \
		.addr = DT_INST_REG_ADDR(n)                        \
	};                                                         \
	EMUL_DT_INST_DEFINE(n, rt1739_emul_init, &rt1739_data_##n, \
			    &common_cfg_##n, &i2c_common_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(INIT_RT1739_EMUL)

DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE);
