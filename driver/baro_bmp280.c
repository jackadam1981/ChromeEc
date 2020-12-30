/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/baro_bmp280.h"
#include "i2c.h"
#include "timer.h"

#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

/* Include third_party code, to allow compiler to more easily inline functions
 * as required.
 */
#include <third_party/bmp280/bmp280.c>

static const uint16_t standby_durn[] = {1, 63, 125, 250, 500, 1000, 2000, 4000};

static int bmp280_set_range(struct motion_sensor_t *s,
				int range,
				int rnd)
{
	struct bmp280_drv_data_t *data = BMP280_GET_DATA(s);
	/*
	 * ->range contains the number of bit to right shift in order for the
	 * measurment to fit into 16 bits (or less if the AP wants to).
	 */
	data->range = 15 - __builtin_clz(range);
	s->current_range = 1 << (16 + data->range);
	return EC_SUCCESS;
}

static int bmp280_read(const struct motion_sensor_t *s, intv3_t v)
{
	int ret, pres;
	struct bmp280_drv_data_t *data = BMP280_GET_DATA(s);

	ret = bmp280_read_uncomp_pressure(s, &pres);

	if (ret)
		return ret;

	v[0] = bmp280_compensate_pressure(s, pres) >> data->range;
	v[1] = v[2] = 0;

	return EC_SUCCESS;
}

/*
 * Set data rate, rate in mHz.
 * Calculate the delay (in ms) to apply.
 */
static int bmp280_set_data_rate(const struct motion_sensor_t *s, int rate,
							int roundup)
{
	struct bmp280_drv_data_t *data = BMP280_GET_DATA(s);
	int durn, i, ret;
	int period; /* Period in ms */

	if (rate == 0) {
		/* Set to sleep mode */
		data->rate = 0;
		return bmp280_set_power_mode(s, BMP280_SLEEP_MODE);
	} else
		period = 1000000 / rate;

	/* reset power mode, waking from sleep */
	if (!data->rate) {
		ret = bmp280_set_power_mode(s, BMP280_NORMAL_MODE);
		if (ret)
			return ret;
	}

	durn = 0;
	for (i = BMP280_STANDBY_CNT-1;  i > 0; i--) {
		if (period >= standby_durn[i] + BMP280_COMPUTE_TIME) {
			durn = i;
			break;
		} else if (period > standby_durn[i-1] + BMP280_COMPUTE_TIME) {
			durn = roundup ? i-1 : i;
			break;
		}
	}
	ret = bmp280_set_standby_durn(s, durn);
	if (ret == EC_SUCCESS)
		/*
		 * The maximum frequency is around 76Hz. Be sure it fits in 16
		 * bits by shifting by one bit.
		 */
		data->rate = (1000000 >> BMP280_RATE_SHIFT) /
			     (standby_durn[durn] + BMP280_COMPUTE_TIME);
	return ret;
}

static int bmp280_get_data_rate(const struct motion_sensor_t *s)
{
	struct bmp280_drv_data_t *data = BMP280_GET_DATA(s);

	return data->rate << BMP280_RATE_SHIFT;
}

const struct accelgyro_drv bmp280_drv = {
	.init = bmp280_init,
	.read = bmp280_read,
	.set_range = bmp280_set_range,
	.set_data_rate = bmp280_set_data_rate,
	.get_data_rate = bmp280_get_data_rate,
};

#ifdef CONFIG_CMD_I2C_STRESS_TEST_ACCEL
struct i2c_stress_test_dev bmp280_i2c_stress_test_dev = {
	.reg_info = {
		.read_reg = BMP280_CHIP_ID_REG,
		.read_val = BMP280_CHIP_ID,
		.write_reg = BMP280_CONFIG_REG,
	},
	.i2c_read = &i2c_read8,
	.i2c_write = &i2c_write8,
};
#endif /* CONFIG_CMD_I2C_STRESS_TEST_ACCEL */
