/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Volteer ISH board-specific configuration */

#include "console.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/accel_bma2x2.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "motion_sense.h"
#include "power.h"
#include "task.h"

#include "gpio_list.h" /* has to be included last */

/* I2C port map */
const struct i2c_port_t i2c_ports[] = {
	{
		.name = "sensor",
		.port = I2C_PORT_SENSOR,
		.kbps = 1000
	},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* Sensor config */
static struct mutex g_lid_mutex;

/* sensor private data */
static struct accelgyro_saved_data_t g_bma253_data;

/*
 * TODO(b/124337208): P0 boards don't have this sensor mounted so the rotation
 * matrix can't be tested properly. This needs to be revisited after EVT to make
 * sure the rotaiton matrix for the lid sensor is correct.
 */
static const mat33_fp_t lid_standard_ref = {
	{ 0, FLOAT_TO_FP(-1), 0},
	{ FLOAT_TO_FP(-1), 0, 0},
	{ 0, 0, FLOAT_TO_FP(-1)}
};

/* Drivers */
struct motion_sensor_t motion_sensors[] = {
	[LID_ACCEL] = {
		.name = "Lid Accel",
		.active_mask = SENSOR_ACTIVE_S0_S3,
		.chip = MOTIONSENSE_CHIP_BMA255,
		.type = MOTIONSENSE_TYPE_ACCEL,
		.location = MOTIONSENSE_LOC_LID,
		.drv = &bma2x2_accel_drv,
		.mutex = &g_lid_mutex,
		.drv_data = &g_bma253_data,
		.port = I2C_PORT_SENSOR,
		.i2c_spi_addr_flags = BMA2x2_I2C_ADDR1_FLAGS,
		.rot_standard_ref = &lid_standard_ref,
		.min_frequency = BMA255_ACCEL_MIN_FREQ,
		.max_frequency = BMA255_ACCEL_MAX_FREQ,
		.default_range = 2, /* g, to support tablet mode */
		.config = {
			/* EC use accel for angle detection */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 10000 | ROUND_UP_FLAG,
			},
			/* Sensor on in S3 */
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 10000 | ROUND_UP_FLAG,
			},
		},
	},
};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

int chipset_in_state(int state_mask)
{
	return state_mask & CHIPSET_STATE_ON;
}

int chipset_in_or_transitioning_to_state(int state_mask)
{
	return state_mask & CHIPSET_STATE_ON;
}

void chipset_force_shutdown(enum chipset_shutdown_reason reason)
{
}

int board_idle_task(void *unused)
{
	while (1)
		task_wait_event(-1);
}
