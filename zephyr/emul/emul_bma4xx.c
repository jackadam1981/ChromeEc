/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
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
#include <zephyr/sys/math_extras.h>
#include <zephyr/ztest.h>

#define DT_DRV_COMPAT cros_bma4xx_emul

LOG_MODULE_REGISTER(bma4xx_emul, LOG_LEVEL_INF);

struct bma4xx_emul_data {
	struct i2c_common_emul_data i2c;
	struct motion_sensor_t motion_data;
	/** True if the sensor is currently enabled. */
	bool accel_enabled;
	/** Current sensor range, ±2/4/8/16g; value is positive gs. */
	uint8_t accel_range;
	/** Raw register value of ACC_CONF:acc_odr. */
	uint8_t odr_raw;
	intv3_t acceleration;
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

	data->accel_enabled = false;
	data->accel_range = 2;
	data->odr_raw = 8;
	memset(data->acceleration, 0, sizeof(data->acceleration));
}

struct i2c_common_emul_data *bma4xx_emul_get_i2c(const struct emul *emul)
{
	struct bma4xx_emul_data *data = emul->data;

	return &data->i2c;
}

struct motion_sensor_t *bma4xx_emul_get_sensor_data(const struct emul *emul)
{
	struct bma4xx_emul_data *data = emul->data;

	return &data->motion_data;
}

bool bma4xx_emul_is_accel_enabled(const struct emul *emul)
{
	struct bma4xx_emul_data *data = emul->data;

	return data->accel_enabled;
}

void bma4xx_emul_set_accel_enabled(const struct emul *emul, bool enabled)
{
	struct bma4xx_emul_data *data = emul->data;

	data->accel_enabled = enabled;
}

uint8_t bma4xx_emul_get_accel_range(const struct emul *emul)
{
	struct bma4xx_emul_data *data = emul->data;

	return data->accel_range;
}

uint32_t bma4xx_emul_get_odr(const struct emul *emul)
{
	struct bma4xx_emul_data *data = emul->data;

	/*
	 * This function deliberately differs from bma4_reg_to_odr() to provide
	 * an obviously-correct reference for tests.
	 */
	switch (data->odr_raw) {
	case 1:
		return 781; /* 25/32 Hz */
	case 2:
		return 1562; /* 25/16 Hz */
	case 3:
		return 3125;
	case 4:
		return 6250;
	case 5:
		return 12500;
	case 6:
		return 25000;
	case 7:
		return 50000;
	case 8:
		return 100000;
	case 9:
		return 200000;
	case 10:
		return 400000;
	case 11:
		return 800000;
	case 12:
		return 1600000;
	default:
		LOG_ERR("ODR register value %#x is reserved", data->odr_raw);
		return 0;
	}
}

void bma4xx_emul_set_accel_data(const struct emul *emul, int x, int y, int z)
{
	struct bma4xx_emul_data *data = emul->data;

	data->acceleration[0] = x;
	data->acceleration[1] = y;
	data->acceleration[2] = z;
}

static int bma4xx_emul_read_byte(const struct emul *target, int reg,
				 uint8_t *val, int bytes)
{
	struct bma4xx_emul_data *data = target->data;

	if (reg != BMA4_FIFO_DATA_ADDR) {
		/*
		 * Burst reads autoincrement register addresses, except for
		 * FIFO data which reads from the FIFO instead.
		 */
		reg += bytes;
	}

	switch (reg) {
	case BMA4_CHIP_ID_ADDR:
		*val = 0x12; /* BMA422_CHIP_ID */
		return 0;
	case BMA4_DATA_8_ADDR: /* ACC_X(LSB) */
		*val = (data->acceleration[0] & GENMASK(3, 0)) << 4;
		return 0;
	case BMA4_DATA_8_ADDR + 1: /* ACC_X(MSB) */
		*val = (data->acceleration[0] >> 4) & GENMASK(7, 0);
		return 0;
	case BMA4_DATA_8_ADDR + 2: /* ACC_Y(LSB) */
		*val = (data->acceleration[1] & GENMASK(3, 0)) << 4;
		return 0;
	case BMA4_DATA_8_ADDR + 3: /* ACC_Y(MSB) */
		*val = (data->acceleration[1] >> 4) & GENMASK(7, 0);
		return 0;
	case BMA4_DATA_8_ADDR + 4: /* ACC_Z(LSB) */
		*val = (data->acceleration[2] & GENMASK(3, 0)) << 4;
		return 0;
	case BMA4_DATA_8_ADDR + 5: /* ACC_Z(MSB) */
		*val = (data->acceleration[2] >> 4) & GENMASK(7, 0);
		return 0;
	case BMA4_ACCEL_CONFIG_ADDR:
		*val = data->odr_raw | 0xA0;
		return 0;
	case BMA4_ACCEL_RANGE_ADDR:
		*val = u32_count_trailing_zeros(data->accel_range) - 1;
		__ASSERT_NO_MSG(*val >= 0 && *val <= 3);
		return 0;
	case BMA4_POWER_CTRL_ADDR:
		*val = data->accel_enabled ? BMA4_ACCEL_ENABLE_MSK : 0;
		return 0;
	}

	LOG_WRN("unhandled I2C read from register %#x", reg);
	return -ENOTSUP;
}

static int bma4xx_emul_write_byte(const struct emul *target, int reg,
				  uint8_t val, int bytes)
{
	struct bma4xx_emul_data *data = target->data;

	if (bytes != 1) {
		LOG_ERR("multi-byte writes are not supported");
		return -ENOTSUP;
	}

	switch (reg) {
	case BMA4_ACCEL_CONFIG_ADDR:
		if ((val & 0xF0) != 0xA0) {
			LOG_ERR("unsupported acc_bwp/acc_perf_mode: %#x", val);
			return -EINVAL;
		}
		data->odr_raw = val & BMA4_ACCEL_ODR_MSK;
		return 0;
	case BMA4_ACCEL_RANGE_ADDR:
		if ((val & GENMASK(1, 0)) != val) {
			LOG_ERR("reserved bits set in ACC_RANGE write: %#x",
				val);
			return -EINVAL;
		}
		/* 0 => 2, 1 => 4, ... 3 => 16 */
		data->accel_range = 2 << val;
		return 0;
	case BMA4_POWER_CTRL_ADDR:
		if ((val & ~BMA4_ACCEL_ENABLE_MSK) != 0) {
			LOG_ERR("unhandled bits in POWER_CTRL write: %#x", val);
			return -ENOTSUP;
		}
		data->accel_enabled = (val & BMA4_ACCEL_ENABLE_MSK) != 0;
		return 0;
	}

	LOG_WRN("unhandled I2C write to register %#x", reg);
	return -ENOTSUP;
}

static int bma4xx_emul_init(const struct emul *emul,
			    const struct device *parent)
{
	struct bma4xx_emul_data *data = emul->data;

	data->i2c.i2c = parent;
	i2c_common_emul_init(&data->i2c);
	bma4xx_emul_reset(emul);

	return 0;
}

#define INIT_BMA4XX(n)                                                    \
	static mutex_t bma4xx_emul_mutex_##n;                             \
	static struct accelgyro_saved_data_t bma4xx_emul_drv_data_##n;    \
	static struct bma4xx_emul_data bma4xx_emul_data_##n = {         \
		.i2c = {                                                \
			.write_byte = bma4xx_emul_write_byte,           \
			.read_byte = bma4xx_emul_read_byte,             \
			.i2c = DEVICE_DT_GET(DT_INST_PARENT(n)),        \
		},                                                      \
		.motion_data = {					\
			.name = DT_NODE_PATH(DT_DRV_INST(n)),           \
			.chip = MOTIONSENSE_CHIP_BMA422,                \
			.type = MOTIONSENSE_TYPE_ACCEL,                 \
			.location = MOTIONSENSE_LOC_BASE,               \
			.drv = &bma4_accel_drv,                         \
			.mutex = &bma4xx_emul_mutex_##n,                \
			.drv_data = &bma4xx_emul_drv_data_##n,          \
			.port = I2C_PORT_BUS(DT_INST_BUS(n)),           \
			.i2c_spi_addr_flags = DT_INST_REG_ADDR(n),      \
		},                                                      \
	}; \
	static const struct bma4xx_emul_cfg bma4xx_emul_cfg_##n = {	\
		.i2c = {                                                \
			.dev_label = DT_NODE_FULL_NAME(DT_DRV_INST(n)), \
			.addr = DT_INST_REG_ADDR(n),                    \
		},                                                      \
	};     \
	EMUL_DT_INST_DEFINE(n, bma4xx_emul_init, &bma4xx_emul_data_##n,   \
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
