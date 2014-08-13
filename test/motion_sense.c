/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test motion sense code.
 */

#include <math.h>

#include "accelgyro.h"
#include "common.h"
#include "hooks.h"
#include "host_command.h"
#include "motion_sense.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

/*****************************************************************************/
/* Mock functions */
struct mock_acc_data {
	int x;
	int y;
	int z;
};

static int accel_init(const struct motion_sensor_t *s)
{
	return EC_SUCCESS;
}

static int accel_read(const struct motion_sensor_t *s,
	int *x_acc, int *y_acc, int *z_acc)
{
	struct mock_acc_data *p = (struct mock_acc_data *)s->drv_data;
	/* Return the mock values. */
	if (p) {
		*x_acc = p->x;
		*y_acc = p->y;
		*z_acc = p->z;
	}
	return EC_SUCCESS;
}

static int accel_set_range(const struct motion_sensor_t *s,
			   const int range,
			   const int rnd)
{
	return EC_SUCCESS;
}

static int accel_get_range(const struct motion_sensor_t *s,
			   int * const range)
{
	return EC_SUCCESS;
}

static int accel_set_resolution(const struct motion_sensor_t *s,
				const int res,
				const int rnd)
{
	return EC_SUCCESS;
}

static int accel_get_resolution(const struct motion_sensor_t *s,
				int * const res)
{
	return EC_SUCCESS;
}

static int accel_set_data_rate(const struct motion_sensor_t *s,
			      const int rate,
			      const int rnd)
{
	return EC_SUCCESS;
}

static int accel_get_data_rate(const struct motion_sensor_t *s,
			      int * const rate)
{
	return EC_SUCCESS;
}

const struct accelgyro_drv test_motion_sense = {
	.init = accel_init,
	.read = accel_read,
	.set_range = accel_set_range,
	.get_range = accel_get_range,
	.set_resolution = accel_set_resolution,
	.get_resolution = accel_get_resolution,
	.set_data_rate = accel_set_data_rate,
	.get_data_rate = accel_get_data_rate,
};

static struct mock_acc_data test_drv_data[2];

const struct motion_sensor_t motion_sensors[] = {
	{"base", SENSOR_CHIP_LSM6DS0, SENSOR_ACCELEROMETER, LOCATION_BASE,
		&test_motion_sense, NULL, &test_drv_data[0], 0},
	{"lid", SENSOR_CHIP_KXCJ9, SENSOR_ACCELEROMETER, LOCATION_LID,
		&test_motion_sense, NULL, &test_drv_data[1], 0},
};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

/*****************************************************************************/
/* Test utilities */
static int test_lid_angle(void)
{
	uint8_t *lpc_status = host_get_memmap(EC_MEMMAP_ACC_STATUS);
	uint8_t sample;

	const struct motion_sensor_t *base = &motion_sensors[0];
	const struct motion_sensor_t *lid = &motion_sensors[1];

	struct mock_acc_data *pbase = (struct mock_acc_data *)base->drv_data;
	struct mock_acc_data *plid = (struct mock_acc_data *)lid->drv_data;

	hook_notify(HOOK_CHIPSET_STARTUP);

	/*
	 * Set the base accelerometer as if it were sitting flat on a desk
	 * and set the lid to closed.
	 */
	pbase->x = 0;
	pbase->y = 0;
	pbase->z = 1000;
	plid->x = 0;
	plid->y = 0;
	plid->z = 1000;
	sample = *lpc_status & EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK;
	task_wake(TASK_ID_MOTIONSENSE);
	while ((*lpc_status & EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK) == sample)
		msleep(5);
	TEST_ASSERT(motion_get_lid_angle() == 0);

	/* Set lid open to 90 degrees. */
	plid->x = -1000;
	plid->y = 0;
	plid->z = 0;
	sample = *lpc_status & EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK;
	task_wake(TASK_ID_MOTIONSENSE);
	while ((*lpc_status & EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK) == sample)
		msleep(5);
	TEST_ASSERT(motion_get_lid_angle() == 90);

	/* Set lid open to 225. */
	plid->x = 500;
	plid->y = 0;
	plid->z = -500;
	sample = *lpc_status & EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK;
	task_wake(TASK_ID_MOTIONSENSE);
	while ((*lpc_status & EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK) == sample)
		msleep(5);
	TEST_ASSERT(motion_get_lid_angle() == 225);

	/*
	 * Align base with hinge and make sure it returns unreliable for angle.
	 * In this test it doesn't matter what the lid acceleration vector is.
	 */
	pbase->x = 0;
	pbase->y = 1000;
	pbase->z = 0;
	sample = *lpc_status & EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK;
	task_wake(TASK_ID_MOTIONSENSE);
	while ((*lpc_status & EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK) == sample)
		msleep(5);
	TEST_ASSERT(motion_get_lid_angle() == LID_ANGLE_UNRELIABLE);

	/*
	 * Use all three axes and set lid to negative base and make sure
	 * angle is 180.
	 */
	pbase->x = 500;
	pbase->y = 400;
	pbase->z = 300;
	plid->x = -500;
	plid->y = -400;
	plid->z = -300;
	sample = *lpc_status & EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK;
	task_wake(TASK_ID_MOTIONSENSE);
	while ((*lpc_status & EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK) == sample)
		msleep(5);
	TEST_ASSERT(motion_get_lid_angle() == 180);

	return EC_SUCCESS;
}


void run_test(void)
{
	test_reset();

	RUN_TEST(test_lid_angle);

	test_print_result();
}
