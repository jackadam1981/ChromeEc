/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "common.h"
#include "driver/accel_bma4xx.h"
#include "emul/emul_bma4xx.h"
#include "emul/emul_common_i2c.h"
#include "i2c.h"
#include "motion_sense.h"
#include "test/drivers/test_state.h"

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define EMUL EMUL_DT_GET(DT_NODELABEL(bma422_emul))
#define SENSOR bma4xx_emul_get_sensor_data(EMUL)

ZTEST_SUITE(bma4xx, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

static int read_wrong_chip_id(const struct emul *target, int reg, uint8_t *val,
			      int bytes, void *data)
{
	if (reg == BMA4_CHIP_ID_ADDR) {
		*val = 0x13;
		return 0;
	}
	return 1;
}

ZTEST_USER(bma4xx, test_init)
{
	/* Basic initialization works. */
	zassert_ok(bma4_accel_drv.init(SENSOR));

	/* Sensor gets turned off if it was already on. */
	bma4xx_emul_set_accel_enabled(EMUL, true);
	zassert_ok(bma4_accel_drv.init(SENSOR));
	zassert_false(bma4xx_emul_is_accel_enabled(EMUL));

	/* Unexpected chip ID is an error. */
	i2c_common_emul_set_read_func(bma4xx_emul_get_i2c(EMUL),
				      read_wrong_chip_id, NULL);
	zassert_equal(EC_ERROR_HW_INTERNAL, bma4_accel_drv.init(SENSOR));
}

ZTEST_USER(bma4xx, test_set_range)
{
	/* Sets to ±16g successfully. */
	zassert_ok(bma4_accel_drv.set_range(SENSOR, 16, 0));
	zassert_equal(16, bma4xx_emul_get_accel_range(EMUL));

	/* ±3g with roundup flag is ±4. */
	zassert_ok(bma4_accel_drv.set_range(SENSOR, 3, 1));
	zassert_equal(4, bma4xx_emul_get_accel_range(EMUL));

	/* .. ±2g without roundup flag. */
	zassert_ok(bma4_accel_drv.set_range(SENSOR, 3, 0));
	zassert_equal(2, bma4xx_emul_get_accel_range(EMUL));

	/* Communication errors bubble up and don't change the range. */
	i2c_common_emul_set_write_fail_reg(bma4xx_emul_get_i2c(EMUL),
					   BMA4_ACCEL_RANGE_ADDR);
	zassert_not_equal(0, bma4_accel_drv.set_range(SENSOR, 8, 0));
	zassert_equal(2, bma4xx_emul_get_accel_range(EMUL));
}

ZTEST_USER(bma4xx, test_data_rate)
{
	int odr;

	/* Requesting zero ODR disables the sensor. */
	bma4xx_emul_set_accel_enabled(EMUL, true);
	zassert_ok(bma4_accel_drv.set_data_rate(SENSOR, 0, 0));
	zassert_false(bma4xx_emul_is_accel_enabled(EMUL));

	/*
	 * Minimum supported ODR is 0.78125 Hz; smaller requested values should
	 * still yield a nonzero ODR and enable a previously-disabled sensor.
	 */
	zassert_ok(bma4_accel_drv.set_data_rate(SENSOR, 200, 0));
	zassert_true(bma4xx_emul_is_accel_enabled(EMUL));
	odr = bma4_accel_drv.get_data_rate(SENSOR);
	zassert_equal(781, odr, "actual reported data rate was %d mHz", odr);
	zassert_equal(781, bma4xx_emul_get_odr(EMUL),
		      "emulator ODR did not match driver ODR");

	/*
	 * Faster than can be supported goes to the maximum possible ODR. 4 kHz
	 * rounds down to 3.2 kHz which is still too high, so we actually get
	 * 1.6 kHz.
	 */
	zassert_ok(bma4_accel_drv.set_data_rate(SENSOR, 4000 * 1000, 0));
	odr = bma4_accel_drv.get_data_rate(SENSOR);
	zassert_equal(1600 * 1000, odr, "actual reported data rate was %d mHz",
		      odr);
	zassert_equal(1600 * 1000, bma4xx_emul_get_odr(EMUL),
		      "emulator ODR did not match driver ODR");

	/* Rounds up only if requested, otherwise down. */
	zassert_ok(bma4_accel_drv.set_data_rate(SENSOR, 160 * 1000, 0));
	odr = bma4xx_emul_get_odr(EMUL);
	zassert_equal(100 * 1000, odr, "actual ODR was %d", odr);
	zassert_ok(bma4_accel_drv.set_data_rate(SENSOR, 160 * 1000, 1));
	odr = bma4xx_emul_get_odr(EMUL);
	zassert_equal(200 * 1000, odr, "actual ODR was %d", odr);

	/* Communication errors bubble up and reported ODR is unchanged. */
	i2c_common_emul_set_write_fail_reg(bma4xx_emul_get_i2c(EMUL),
					   BMA4_ACCEL_CONFIG_ADDR);
	zassert_not_equal(0,
			  bma4_accel_drv.set_data_rate(SENSOR, 100 * 1000, 0));
	zassert_equal(200 * 1000, bma4_accel_drv.get_data_rate(SENSOR));
}

ZTEST_USER(bma4xx, test_read_data)
{
	intv3_t acceleration;

	bma4xx_emul_set_accel_data(EMUL, 627, 1, -809);
	zassert_ok(bma4_accel_drv.read(SENSOR, acceleration));
	/*
	 * read() returns raw sensor data, which is shifted. The consumer of the
	 * data is expected to know this and possibly also compensate for
	 * scaling when the sensitivity changes.
	 */
	zassert_equal(acceleration[0], 627 << 4, "actual value was %d",
		      acceleration[0]);
	zassert_equal(acceleration[1], 1 << 4, "actual value was %d",
		      acceleration[1]);
	zassert_equal(acceleration[2], -(809 << 4), "actual value was %d",
		      acceleration[2]);
}

ZTEST_USER(bma4xx, test_offset)
{
	// Offset compensation is applied to data only if NV_CONF.acc_off_en
	// is enabled.
}
