/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Soraka ISH board-specific configuration */

#include "als.h"
#include "console.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/baro_bmp280.h"
#include "gpio.h"
#include "gpio_list.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "math_util.h"
#include "motion_sense.h"
#include "task.h"
#include "uart.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

/* I2C port map */
const struct i2c_port_t i2c_ports[]  = {
	{"accelgyro", I2C_PORT_GYRO,   400, GPIO_I2C0_SCL,   GPIO_I2C0_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* Lid Sensor mutex */
static struct mutex g_lid_mutex;

struct bmi160_drv_data_t g_bmi160_data;
struct bmp280_drv_data_t bmp280_drv_data;

/* Matrices to rotate sensors into standard reference frame */
const matrix_3x3_t mag_standard_ref = {
	{ FLOAT_TO_FP(-1), 0, 0},
	{ 0,  FLOAT_TO_FP(1), 0},
	{ 0, 0, FLOAT_TO_FP(-1)}
};

const matrix_3x3_t lid_standard_ref = {
	{ 0,  FLOAT_TO_FP(1),  0},
	{FLOAT_TO_FP(-1),  0,  0},
	{ 0,  0, FLOAT_TO_FP(-1)}
};

/* This is placeholder before CONFIG_ACCEL_INTERRUPTS is defined */
void bmi160_interrupt(enum gpio_signal signal)
{
	CPRINTS("Inside ISH bmi160_interrupt");
}

struct motion_sensor_t motion_sensors[] = {
	[LID_ACCEL] = {
	 .name = "Lid Accel",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_BMI160,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &bmi160_drv,
	 .mutex = &g_lid_mutex,
	 .drv_data = &g_bmi160_data,
	 .port = I2C_PORT_GYRO,
	 .addr = BMI160_ADDR0,
	 .rot_standard_ref = &lid_standard_ref,
	 .default_range = 2,  /* g, enough for laptop. */
	 .config = {
		 /* AP: by default use EC settings */
		 [SENSOR_CONFIG_AP] = {
			.odr = 0,
			.ec_rate = 0,
		 },
		 /* EC use accel for angle detection */
		 [SENSOR_CONFIG_EC_S0] = {
			.odr = 10000 | ROUND_UP_FLAG,
			.ec_rate = 100 * MSEC,
		 },
		 /* Sensor off in S3/S5 */
		 [SENSOR_CONFIG_EC_S3] = {
			.odr = 0,
			.ec_rate = 0
		 },
		 /* Sensor off in S3/S5 */
		 [SENSOR_CONFIG_EC_S5] = {
			.odr = 0,
			.ec_rate = 0
		 },
	 },
	},

	[LID_GYRO] = {
	 .name = "Lid Gyro",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_BMI160,
	 .type = MOTIONSENSE_TYPE_GYRO,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &bmi160_drv,
	 .mutex = &g_lid_mutex,
	 .drv_data = &g_bmi160_data,
	 .port = I2C_PORT_GYRO,
	 .addr = BMI160_ADDR0,
	 .default_range = 1000, /* dps */
	 .rot_standard_ref = &lid_standard_ref,
	 .config = {
		 /* AP: by default shutdown all sensors */
		 [SENSOR_CONFIG_AP] = {
			.odr = 0,
			.ec_rate = 0,
		 },
		 /* EC does not need in S0 */
		 [SENSOR_CONFIG_EC_S0] = {
			.odr = 0,
			.ec_rate = 0,
		 },
		 /* Sensor off in S3/S5 */
		 [SENSOR_CONFIG_EC_S3] = {
			.odr = 0,
			.ec_rate = 0,
		 },
		 /* Sensor off in S3/S5 */
		 [SENSOR_CONFIG_EC_S5] = {
			.odr = 0,
			.ec_rate = 0,
		 },
	 },
	},

	[LID_MAG] = {
	 .name = "Lid Mag",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_BMI160,
	 .type = MOTIONSENSE_TYPE_MAG,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &bmi160_drv,
	 .mutex = &g_lid_mutex,
	 .drv_data = &g_bmi160_data,
	 .port = I2C_PORT_GYRO,
	 .addr = BMI160_ADDR0,
	 .default_range = 1 << 11, /* 16LSB / uT, fixed */
	 .rot_standard_ref = &mag_standard_ref,
	 .config = {
		 /* AP: by default shutdown all sensors */
		 [SENSOR_CONFIG_AP] = {
			.odr = 0,
			.ec_rate = 0,
		 },
		 /* EC does not need in S0 */
		 [SENSOR_CONFIG_EC_S0] = {
			.odr = 0,
			.ec_rate = 0,
		 },
		 /* Sensor off in S3/S5 */
		 [SENSOR_CONFIG_EC_S3] = {
			.odr = 0,
			.ec_rate = 0,
		 },
		 /* Sensor off in S3/S5 */
		 [SENSOR_CONFIG_EC_S5] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
	 },
	},

	[LID_BARO] = {
	 .name = "Base Baro",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_BMP280,
	 .type = MOTIONSENSE_TYPE_BARO,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &bmp280_drv,
	 .drv_data = &bmp280_drv_data,
	 .port = I2C_PORT_BARO,
	 .addr = BMP280_I2C_ADDRESS1,
	 .default_range = 1 << 18, /*  1bit = 4 Pa, 16bit ~= 2600 hPa */
	 .config = {
		/* AP: by default shutdown all sensors */
		[SENSOR_CONFIG_AP] = {
			.odr = 0,
			.ec_rate = 0,
		},
		/* EC does not need in S0 */
		[SENSOR_CONFIG_EC_S0] = {
			.odr = 0,
			.ec_rate = 0,
		},
		/* Sensor off in S3/S5 */
		[SENSOR_CONFIG_EC_S3] = {
			.odr = 0,
			.ec_rate = 0,
		},
		/* Sensor off in S3/S5 */
		[SENSOR_CONFIG_EC_S5] = {
			.odr = 0,
			.ec_rate = 0,
		},
	 },
	},
};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

/* dummy functions to remove 'undefined' symbol link error for acpi.o
 * due to CONFIG_LPC flag
 */
#ifdef CONFIG_LPC
int lpc_query_host_event_state(void)
{
	return 0;
}

void lpc_set_acpi_status_mask(uint8_t mask)
{
}
#endif
