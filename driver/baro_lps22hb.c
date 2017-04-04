/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LPS22HB pressure sensor module for Chrome EC. */

#include "accelgyro.h"
#include "math_util.h"
#include "common.h"
#include "console.h"
#include "host_command.h"
#include "i2c.h"
#include "task.h"
#include "driver/baro_lps22hb.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

static int lps22hb_set_data_rate(const struct motion_sensor_t *s,
				 int rate, int rnd)
{
	int normalized_rate, ret;
	struct stprivate_data *data = s->drv_data;
	uint8_t reg_val;

	/* Power Off device. */
	if (rate == 0)
		return st_write_data_with_mask(s, LPS22HB_REG_CTRL_REG1,
					       LPS22HB_MASK_ODR,
					       LPS22HB_POWER_DOWN);

	reg_val = LPS22HB_ODR_TO_REG(rate);
	normalized_rate = LPS22HB_ODR_TO_NORMALIZE(rate);

	if (rnd && (normalized_rate < rate)) {
		reg_val++;
		normalized_rate = LPS22HB_ODR_TO_NORMALIZE(reg_val);
	}

	/* Adjust rounded value. */
	if (reg_val > LPS22HB_ODR_75HZ) {
		reg_val = LPS22HB_ODR_75HZ;
		normalized_rate = LPS22HB_MAX_ODR;
	} else if (reg_val < LPS22HB_ODR_1HZ) {
		reg_val = LPS22HB_ODR_1HZ;
		normalized_rate = LPS22HB_MIN_ODR;
	}
	ret = st_write_data_with_mask(s, LPS22HB_REG_CTRL_REG1,
			LPS22HB_MASK_ODR, reg_val);
	if (ret)
		return ret;

	data->base.odr = normalized_rate;
	return EC_SUCCESS;
}

static int lps22hb_get_data_rate(const struct motion_sensor_t *s)
{
	return st_get_data_rate(s);
}

/**
 * lps22hb_set_range - set full scale range
 * @s: Motion sensor pointer
 * @range: Range
 * @rnd: Round up/down flag
 */
static int lps22hb_set_range(const struct motion_sensor_t *s,
			     int range, int rnd)
{
	struct stprivate_data *data = s->drv_data;
	/*
	 * ->resol contains the number of bit to right shift in order for the
	 * measurment to fit into 16 bits (or less if the AP wants to).
	 */
	data->base.range = 15 - __builtin_clz(range);
	return EC_SUCCESS;
}

/**
 * lps22hb_get_range - get full scale range
 * @s: Motion sensor pointer
 */
static int lps22hb_get_range(const struct motion_sensor_t *s)
{
	struct stprivate_data *data = s->drv_data;

	return 1 << (16 + data->base.range);
}

static int lps22hb_read(const struct motion_sensor_t *s, vector_3_t v)
{
	uint8_t raw[3];
	int ret;
	struct stprivate_data *data = s->drv_data;

	ret = st_raw_read_n(s->port, s->addr, LPS22HB_REG_PRESS_OUT_XL, raw, 3);
	if (ret)
		return ret;

	/*
	 * Output is in Pa, starting to raw in hPa/LSB
	 * V[0] are hPa
	 *
	 * Sensitivity is 4096 LSB/hPa, for Pa -> LSB * 100/4096
	 */
	v[0] = (uint32_t)((uint32_t)raw[2] << 16 |
			  (uint32_t)raw[1] << 8 |
			  (uint32_t)raw[0]);
	v[0] = (100 * v[0]) / LPS22HB_P_SENSITIVITY;
	v[0] >>= data->base.range;
	v[2] = v[1] = 0;

	return EC_SUCCESS;
}

static int lps22hb_init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp;
	uint32_t timeout = 100;

	ret = raw_read8(s->port, s->addr, LPS22HB_REG_WHO_AM_I, &tmp);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	if (tmp != LPS22HB_VAL_WHO_AM_I)
		return EC_ERROR_ACCESS_DENIED;

	/* Reset device and restore default reg values. */
	if (raw_write8(s->port, s->addr, LPS22HB_REG_CTRL_REG2,
		       LPS22HB_SWRESET | LPS22HB_IF_ADD_INC) != EC_SUCCESS) {
		ret = EC_ERROR_UNKNOWN;
		goto err_unlock;
	}

	do {
		/* Wait to Reset sticky bit cleared. */
		ret = raw_read8(s->port, s->addr, LPS22HB_REG_CTRL_REG2, &tmp);
		if (ret != EC_SUCCESS)
			return EC_ERROR_UNKNOWN;

		if (!(tmp & LPS22HB_SWRESET))
			break;

		msleep(10);
		timeout -= 10;
	} while (timeout > 0);

	if (timeout == 0) {
		CPRINTF("[%T %s: MS Init type:0x%X Reset Timeout]\n",
			s->name, s->type);
		return EC_ERROR_TIMEOUT;
	}

	if (st_write_data_with_mask(s, LPS22HB_REG_CTRL_REG1,
				    LPS22HB_BDU_MASK,
				    LPS22HB_BDU_EN) != EC_SUCCESS) {
		ret = EC_ERROR_UNKNOWN;
		goto err_unlock;
	}

	return sensor_init_done(s);

err_unlock:
	CPRINTF("[%T %s: MS Init type:0x%X Error]\n", s->name, s->type);

	return ret;
}

const struct accelgyro_drv lps22hb_drv = {
	.init = lps22hb_init,
	.read = lps22hb_read,
	.set_range = lps22hb_set_range,
	.get_range = lps22hb_get_range,
	.set_data_rate = lps22hb_set_data_rate,
	.get_data_rate = lps22hb_get_data_rate,
};
