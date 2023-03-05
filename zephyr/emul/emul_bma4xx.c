/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/accel_bma4xx.h"
#include "emul/emul_bma4xx.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_stub_device.h"
#include "i2c.h"
#include "motion_sense.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#define DT_DRV_COMPAT cros_bma4xx_emul

LOG_MODULE_REGISTER(bma4xx_emul, LOG_LEVEL_INF);

struct bma4xx_emul_data {
	struct i2c_common_emul_data i2c;
	struct motion_sensor_t motion_data;
};

struct bma4xx_emul_cfg {
	struct i2c_common_emul_cfg i2c;
};

void bma4xx_emul_reset(const struct emul *emul)
{
	struct bma4xx_emul_data *data = emul->data;

	i2c_common_emul_set_read_fail_reg(&data->i2c,
					  I2C_COMMON_EMUL_NO_FAIL_REG);
	i2c_common_emul_set_write_fail_reg(&data->i2c,
					   I2C_COMMON_EMUL_NO_FAIL_REG);
}

struct motion_sensor_t *bma4xx_emul_get_sensor_data(const struct emul *emul)
{
	struct bma4xx_emul_data *data = emul->data;

	return &data->motion_data;
}

#define INIT_BMA4XX(n)                                                  \
	static mutex_t bma4xx_emul_mutex_##n;                           \
	static struct accelgyro_saved_data_t bma4xx_emul_drv_data_##n;  \
	static struct bma4xx_emul_data bma4xx_emul_data_##n = {       \
		.i2c = {                                               \
			.write_byte = bma4xx_emul_write_byte,           \
			.read_byte = bma4xx_emul_read_byte,             \
		},                                                                    \
        	.motion_data = {        \
			.name = DT_NODE_PATH(DT_DRV_INST(n)),                   \
			.chip = MOTIONSENSE_CHIP_BMA422,                        \
			.type = MOTIONSENSE_TYPE_ACCEL,                         \
			.location = MOTIONSENSE_LOC_BASE,                       \
			.drv = &bma4_accel_drv,                                 \
			.mutex = &bma4xx_emul_mutex_##n,                        \
			.drv_data = &bma4xx_emul_drv_data_##n,                  \
			.port = I2C_PORT_BUS(DT_INST_BUS(n)),                   \
			.i2c_spi_addr_flags = DT_INST_REG_ADDR(n),              \
		},                                                                       \
	}; \
	static const struct bma4xx_emul_cfg bma4xx_emul_cfg_##n = {   \
		.i2c = {                                               \
			.dev_label = DT_NODE_FULL_NAME(DT_DRV_INST(n)),   \
			.addr = DT_INST_REG_ADDR(n),                      \
		},                                                        \
	}; \
	EMUL_DT_INST_DEFINE(n, bma4xx_emul_init, &bma4xx_emul_data_##n, \
			    &bma4xx_emul_cfg_##n, &i2c_common_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(INIT_BMA4XX)
DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE);

static void bma4xx_emul_reset_rule_before(const struct ztest_unit_test *test,
					  void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

#define BMA4XX_EMUL_RESET_RULE_BEFORE(n) \
	bma4xx_emul_reset(EMUL_DT_GET(DT_DRV_INST(n)))

	DT_INST_FOREACH_STATUS_OKAY(BMA4XX_EMUL_RESET_RULE_BEFORE);
}
ZTEST_RULE(bma4xx_emul_reset, bma4xx_emul_reset_rule_before, NULL);
