/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "accelgyro.h"
#include "driver/accelgyro_icm42607.h"
#include "driver/accelgyro_icm_common.h"
#include "emul/emul_icm42607.h"
#include "motion_sense.h"
#include "motion_sense_fifo.h"
#include "test/drivers/test_state.h"

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define ICM42607_NODE DT_NODELABEL(icm42607_emul)
#define ACC_SENSOR_ID SENSOR_ID(DT_NODELABEL(ms_icm42607_accel))
#define GYR_SENSOR_ID SENSOR_ID(DT_NODELABEL(ms_icm42607_gyro))

static const struct emul *emul = EMUL_DT_GET(ICM42607_NODE);

static void icm42607_set_temp(uint16_t val)
{
	icm42607_emul_write_reg(emul, ICM42607_REG_TEMP_DATA, val >> 8);
	icm42607_emul_write_reg(emul, ICM42607_REG_TEMP_DATA + 1, val & 0xFF);
}

static bool check_sensor_enabled(enum motionsensor_type type)
{
	int reg = icm42607_emul_peek_reg(emul, ICM42607_REG_PWR_MGMT0);

	if (type == MOTIONSENSE_TYPE_ACCEL) {
		int mode = reg & 3;

		if (mode == 0) {
			return false;
		} else if (mode == 2) {
			return true;
		}

		zassert_unreachable("bad sensor mode %d", mode);
	} else if (type == MOTIONSENSE_TYPE_GYRO) {
		int mode = (reg >> 2) & 3;

		if (mode == 0) {
			return false;
		} else if (mode == 3) {
			return true;
		}

		zassert_unreachable("bad sensor mode %d", mode);
	}

	zassert_unreachable("bad sensor type %d", type);
	return false;
}

static void icm42607_push_packet(const uint16_t *acc, const uint16_t *gyr)
{
	uint8_t buf[16];
	int fifo_size = 0;

	if (acc && gyr) {
		buf[0] = BIT(6) | BIT(5); /* acc + gyr */
		memcpy(buf + 1, acc, 6);
		memcpy(buf + 7, gyr, 6);
		fifo_size = 16;
	} else if (acc) {
		buf[0] = BIT(6); /* acc */
		memcpy(buf + 1, acc, 6);
		fifo_size = 8;
	} else if (gyr) {
		buf[0] = BIT(5); /* gyr */
		memcpy(buf + 1, gyr, 6);
		fifo_size = 8;
	} else {
		zassert_unreachable();
	}

	icm42607_emul_write_reg(emul, ICM42607_REG_INT_STATUS, BIT(2));
	icm42607_emul_write_reg(emul, ICM42607_REG_FIFO_COUNT, fifo_size);

	for (int i = 0; i < fifo_size; i++) {
		icm42607_emul_write_reg(emul, ICM42607_REG_FIFO_DATA + i,
					buf[i]);
	}
}

static void test_fifo(const uint16_t *acc_expected,
		      const uint16_t *gyr_expected)
{
	struct ec_response_motion_sensor_data v = {};
	uint16_t size;
	bool acc_ok = false, gyr_ok = false;
	struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
	struct motion_sensor_t *gyr = &motion_sensors[GYR_SENSOR_ID];

	motion_sense_fifo_reset();
	acc->oversampling_ratio = 1;
	gyr->oversampling_ratio = 1;

	icm42607_push_packet(acc_expected, gyr_expected);
	icm42607_interrupt(0);
	k_sleep(K_SECONDS(1));

	while (motion_sense_fifo_read(sizeof(v), 1, &v, &size)) {
		if (v.flags & MOTIONSENSE_SENSOR_FLAG_TIMESTAMP) {
			continue;
		}

		if (v.sensor_num == ACC_SENSOR_ID) {
			if (acc_ok) {
				zassert_unreachable(
					"more than 1 accel data found in the fifo");
			}

			if (acc_expected) {
				zassert_equal(acc_expected[0], v.data[0]);
				zassert_equal(acc_expected[1], v.data[1]);
				zassert_equal(acc_expected[2], v.data[2]);
				acc_ok = true;
			}
		}
		if (v.sensor_num == GYR_SENSOR_ID) {
			if (gyr_ok) {
				zassert_unreachable(
					"more than 1 gyro data found in the fifo");
			}

			if (gyr_expected) {
				zassert_equal(gyr_expected[0], v.data[0]);
				zassert_equal(gyr_expected[1], v.data[1]);
				zassert_equal(gyr_expected[2], v.data[2]);
				gyr_ok = true;
			}
		}
	}

	zassert_true(!acc_expected || acc_ok);
	zassert_true(!gyr_expected || gyr_ok);
}

ZTEST_USER(icm42607, test_fifo_read)
{
	/* 2 sensor packet */
	test_fifo((uint16_t[]){ 1111, 2222, 3333 },
		  (uint16_t[]){ 4444, 5555, 6666 });

	/* acc only */
	test_fifo((uint16_t[]){ 1111, 2222, 3333 }, NULL);

	/* gyr only */
	test_fifo(NULL, (uint16_t[]){ 4444, 5555, 6666 });
}

/* verify that set_data_rate enables or disables the sensor */
ZTEST_USER(icm42607, test_sensor_enable)
{
	struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
	struct motion_sensor_t *gyr = &motion_sensors[GYR_SENSOR_ID];

	zassert_false(check_sensor_enabled(MOTIONSENSE_TYPE_ACCEL));
	zassert_false(check_sensor_enabled(MOTIONSENSE_TYPE_GYRO));

	zassert_ok(acc->drv->set_data_rate(acc, 12500, 1));
	zassert_true(check_sensor_enabled(MOTIONSENSE_TYPE_ACCEL));
	zassert_false(check_sensor_enabled(MOTIONSENSE_TYPE_GYRO));

	zassert_ok(acc->drv->set_data_rate(gyr, 12500, 1));
	zassert_true(check_sensor_enabled(MOTIONSENSE_TYPE_ACCEL));
	zassert_true(check_sensor_enabled(MOTIONSENSE_TYPE_GYRO));

	zassert_ok(acc->drv->set_data_rate(gyr, 0, 1));
	zassert_true(check_sensor_enabled(MOTIONSENSE_TYPE_ACCEL));
	zassert_false(check_sensor_enabled(MOTIONSENSE_TYPE_GYRO));

	zassert_ok(acc->drv->set_data_rate(acc, 0, 1));
	zassert_false(check_sensor_enabled(MOTIONSENSE_TYPE_ACCEL));
	zassert_false(check_sensor_enabled(MOTIONSENSE_TYPE_GYRO));
}

ZTEST_USER(icm42607, test_data_rate)
{
	struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
	struct motion_sensor_t *gyr = &motion_sensors[GYR_SENSOR_ID];

	zassert_ok(acc->drv->set_data_rate(acc, 12500, 1));
	zassert_equal(acc->drv->get_data_rate(acc), 12500);
	zassert_equal(icm42607_emul_peek_reg(emul, ICM42607_REG_ACCEL_CONFIG0) &
			      0xF,
		      0xC);

	/* 24Hz should round up to 25Hz */
	zassert_ok(gyr->drv->set_data_rate(gyr, 24000, 1));
	zassert_equal(gyr->drv->get_data_rate(gyr), 25000);
	zassert_equal(icm42607_emul_peek_reg(emul, ICM42607_REG_GYRO_CONFIG0) &
			      0xF,
		      0xB);

	/* return error if data rate is out of supported range */
	zassert_not_equal(gyr->drv->set_data_rate(gyr, 6250, 0), 0);
	zassert_not_equal(acc->drv->set_data_rate(acc, 1600000, 0), 0);
}

ZTEST_USER(icm42607, test_offset_out_of_range)
{
	struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
	int16_t acc_offset[3];
	int16_t acc_offset_input[3] = { 10000, -10000, 0 };
	int16_t acc_temp;

	zassert_ok(acc->drv->init(acc));

	zassert_ok(acc->drv->set_offset(acc, acc_offset_input, 40));
	zassert_ok(acc->drv->get_offset(acc, acc_offset, &acc_temp));

	/* icm42607 internally stores offset in 12bit signed integer,
	 * input is clamped into range [2047, -2048], and then scaled to
	 * ec unit, so the result is [1023.5, -1024] => [1024, -1024]
	 */
	zassert_equal(acc_offset[0], 1024);
	zassert_equal(acc_offset[1], -1024, "got %d", acc_offset[1]);
	zassert_equal(acc_offset[2], 0);
	zassert_equal(acc_temp, EC_MOTION_SENSE_INVALID_CALIB_TEMP);
}

ZTEST_USER(icm42607, test_offset)
{
	struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
	struct motion_sensor_t *gyr = &motion_sensors[GYR_SENSOR_ID];
	int16_t acc_offset[3], gyr_offset[3];
	/* use multiplies of 32 to avoid rounding error */
	int16_t acc_offset_expected[3] = { 32, 32 * 2, 32 * 3 };
	int16_t gyr_offset_expected[3] = { 32 * 4, 32 * 5, 32 * 6 };
	int16_t acc_temp, gyr_temp;

	zassert_ok(acc->drv->init(acc));
	zassert_ok(gyr->drv->init(gyr));

	zassert_ok(acc->drv->set_offset(acc, acc_offset_expected, 40));
	zassert_ok(gyr->drv->set_offset(gyr, gyr_offset_expected, 80));
	zassert_ok(acc->drv->get_offset(acc, acc_offset, &acc_temp));
	zassert_ok(gyr->drv->get_offset(gyr, gyr_offset, &gyr_temp));

	zassert_equal(acc_offset[0], acc_offset_expected[0]);
	zassert_equal(acc_offset[1], acc_offset_expected[1]);
	zassert_equal(acc_offset[2], acc_offset_expected[2]);
	zassert_equal(acc_temp, EC_MOTION_SENSE_INVALID_CALIB_TEMP);
	zassert_equal(gyr_offset[0], gyr_offset_expected[0]);
	zassert_equal(gyr_offset[1], gyr_offset_expected[1]);
	zassert_equal(gyr_offset[2], gyr_offset_expected[2]);
	zassert_equal(gyr_temp, EC_MOTION_SENSE_INVALID_CALIB_TEMP);
}

ZTEST_USER(icm42607, test_scale)
{
	struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
	struct motion_sensor_t *gyr = &motion_sensors[GYR_SENSOR_ID];
	uint16_t acc_scale[3], gyr_scale[3];
	int16_t acc_temp, gyr_temp;

	zassert_ok(acc->drv->init(acc));
	zassert_ok(gyr->drv->init(gyr));

	zassert_ok(acc->drv->set_scale(acc, (uint16_t[3]){ 1, 2, 3 }, 4));
	zassert_ok(gyr->drv->set_scale(gyr, (uint16_t[3]){ 5, 6, 7 }, 8));
	zassert_ok(gyr->drv->get_scale(acc, acc_scale, &acc_temp));
	zassert_ok(gyr->drv->get_scale(gyr, gyr_scale, &gyr_temp));

	zassert_equal(acc_scale[0], 1);
	zassert_equal(acc_scale[1], 2);
	zassert_equal(acc_scale[2], 3);
	zassert_equal(acc_temp, EC_MOTION_SENSE_INVALID_CALIB_TEMP);
	zassert_equal(gyr_scale[0], 5);
	zassert_equal(gyr_scale[1], 6);
	zassert_equal(gyr_scale[2], 7);
	zassert_equal(gyr_temp, EC_MOTION_SENSE_INVALID_CALIB_TEMP);
}

ZTEST_USER(icm42607, test_read)
{
	struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
	struct motion_sensor_t *gyr = &motion_sensors[GYR_SENSOR_ID];
	intv3_t v;

	zassert_ok(acc->drv->init(acc));
	zassert_ok(gyr->drv->init(gyr));

	/* verify that sensor data format is configured to little endian */
	int intf_config0 =
		icm42607_emul_peek_reg(emul, ICM42607_REG_INTF_CONFIG0);
	zassert_equal(intf_config0 & ICM42607_SENSOR_DATA_ENDIAN, 0);

	/* test accel read, 16bit LE */
	icm42607_emul_write_reg(emul, ICM42607_REG_ACCEL_DATA_XYZ, 0x01);
	icm42607_emul_write_reg(emul, ICM42607_REG_ACCEL_DATA_XYZ + 1, 0x02);
	icm42607_emul_write_reg(emul, ICM42607_REG_ACCEL_DATA_XYZ + 2, 0x03);
	icm42607_emul_write_reg(emul, ICM42607_REG_ACCEL_DATA_XYZ + 3, 0x04);
	icm42607_emul_write_reg(emul, ICM42607_REG_ACCEL_DATA_XYZ + 4, 0x05);
	icm42607_emul_write_reg(emul, ICM42607_REG_ACCEL_DATA_XYZ + 5, 0x06);
	zassert_ok(acc->drv->read(acc, v));
	zassert_equal(v[0], 0x0201);
	zassert_equal(v[1], 0x0403);
	zassert_equal(v[2], 0x0605);

	/* test gyro read, 16bit LE */
	icm42607_emul_write_reg(emul, ICM42607_REG_GYRO_DATA_XYZ, 0x0A);
	icm42607_emul_write_reg(emul, ICM42607_REG_GYRO_DATA_XYZ + 1, 0x0B);
	icm42607_emul_write_reg(emul, ICM42607_REG_GYRO_DATA_XYZ + 2, 0x0C);
	icm42607_emul_write_reg(emul, ICM42607_REG_GYRO_DATA_XYZ + 3, 0x0D);
	icm42607_emul_write_reg(emul, ICM42607_REG_GYRO_DATA_XYZ + 4, 0x0E);
	icm42607_emul_write_reg(emul, ICM42607_REG_GYRO_DATA_XYZ + 5, 0x0F);
	zassert_ok(gyr->drv->read(gyr, v));
	zassert_equal(v[0], 0x0B0A);
	zassert_equal(v[1], 0x0D0C);
	zassert_equal(v[2], 0x0F0E);
}

/* read() immediately after sensor enabled should fail */
ZTEST_USER(icm42607, test_read_not_stabilized)
{
	struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
	intv3_t v;

	zassert_ok(acc->drv->set_data_rate(acc, 0, 1));
	zassert_ok(acc->drv->set_data_rate(acc, 10000, 1));
	zassert_not_equal(acc->drv->read(acc, v), 0);

	sleep(1);
	zassert_equal(acc->drv->read(acc, v), 0);
}

ZTEST_USER(icm42607, test_set_range)
{
	struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
	struct motion_sensor_t *gyr = &motion_sensors[GYR_SENSOR_ID];
	int reg_val;

	/* set 5G, round down to 4G, expect reg val = 2 */
	zassert_ok(acc->drv->set_range(acc, 5, 0));
	reg_val = (icm42607_emul_peek_reg(emul, ICM42607_REG_ACCEL_CONFIG0) >>
		   5) &
		  3;
	zassert_equal(reg_val, 2);

	/* set 5G, round up to 8G, expect reg val = 1 */
	zassert_ok(acc->drv->set_range(acc, 5, 1));
	reg_val = (icm42607_emul_peek_reg(emul, ICM42607_REG_ACCEL_CONFIG0) >>
		   5) &
		  3;
	zassert_equal(reg_val, 1);

	/* set 1500dps, round down to 1000dps, expect reg val = 1 */
	zassert_ok(gyr->drv->set_range(gyr, 1500, 0));
	reg_val =
		(icm42607_emul_peek_reg(emul, ICM42607_REG_GYRO_CONFIG0) >> 5) &
		3;
	zassert_equal(reg_val, 1);

	/* set 1500dps, round down to 2000dps, expect reg val = 0 */
	zassert_ok(gyr->drv->set_range(gyr, 1500, 1));
	reg_val =
		(icm42607_emul_peek_reg(emul, ICM42607_REG_GYRO_CONFIG0) >> 5) &
		3;
	zassert_equal(reg_val, 0);
}

/* Verify the temperature matches following formula:
 * Temperature in C = (REG_DATA / 128) + 25
 */
ZTEST_USER(icm42607, test_read_temp)
{
	struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
	int temp;

	/* expect 0C = 273.15K */
	icm42607_set_temp(-25 * 128);
	zassert_ok(acc->drv->read_temp(acc, &temp));
	zassert_equal(temp, 273);

	/* expect 100C = 373.15K */
	icm42607_set_temp(75 * 128);
	zassert_ok(acc->drv->read_temp(acc, &temp));
	zassert_equal(temp, 373);

	/* expect 25C = 298K */
	icm42607_set_temp(0);
	zassert_ok(acc->drv->read_temp(acc, &temp));
	zassert_equal(temp, 298);

	/* reset value */
	icm42607_set_temp(-32768);
	zassert_not_equal(acc->drv->read_temp(acc, &temp), 0);
}

ZTEST_USER(icm42607, test_init)
{
	struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
	struct i2c_common_emul_data *common_data =
		emul_icm42607_get_i2c_common_data(emul);

	icm42607_emul_write_reg(emul, ICM42607_REG_WHO_AM_I,
				ICM42607_CHIP_ICM42607P);
	zassert_ok(acc->drv->init(acc));

	icm42607_emul_write_reg(emul, ICM42607_REG_WHO_AM_I, 0x87);
	zassert_not_equal(acc->drv->init(acc), 0);

	i2c_common_emul_set_read_fail_reg(common_data, ICM42607_REG_WHO_AM_I);
	zassert_not_equal(acc->drv->init(acc), 0);
}

static void icm42607_before(void *fixture)
{
	struct i2c_common_emul_data *common_data =
		emul_icm42607_get_i2c_common_data(emul);
	struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
	struct motion_sensor_t *gyr = &motion_sensors[GYR_SENSOR_ID];

	ARG_UNUSED(fixture);

	i2c_common_emul_set_read_fail_reg(common_data,
					  I2C_COMMON_EMUL_NO_FAIL_REG);
	icm42607_emul_reset_regs(emul);
	icm_reset_stabilize_ts(acc);
	icm_reset_stabilize_ts(gyr);
}

ZTEST_SUITE(icm42607, drivers_predicate_post_main, NULL, icm42607_before, NULL,
	    NULL);
