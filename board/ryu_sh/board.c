/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* ryu sensor hub configuration */

#include "common.h"
#include "console.h"
#include "driver/accelgyro_lsm6ds0.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "motion_sense.h"
#include "power.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#include "gpio_list.h"

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_AP_IN_SUSPEND,  1, "SUSPEND_ASSERTED"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"slave",  I2C_PORT_SLAVE, 100,
		GPIO_SLAVE_I2C_SCL, GPIO_SLAVE_I2C_SDA},
	/* For testing purposes, enable master i2c*/
	{"master",  I2C_PORT_MASTER, 100,
		GPIO_MASTER_I2C_SCL, GPIO_MASTER_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* Sensor mutex */
static struct mutex g_mutex;

/* lsm6ds0 local sensor data (per-sensor) */
struct lsm6ds0_data g_lsm6ds0_data[2];

struct motion_sensor_t motion_sensors[] = {

	/*
	 * Note: lsm6ds0: supports accelerometer and gyro sensor
	 * Requirement: accelerometer sensor must init before gyro sensor
	 * DO NOT change the order of the following table.
	 */
	{.name = "Accel",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_LSM6DS0,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &lsm6ds0_drv,
	 .mutex = &g_mutex,
	 .drv_data = &g_lsm6ds0_data[0],
	 .i2c_addr = LSM6DS0_ADDR1,
	 .rot_standard_ref = NULL,
	 .default_odr = 119000,
	 .default_range = 2
	},

	{.name = "Gyro",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_LSM6DS0,
	 .type = MOTIONSENSE_TYPE_GYRO,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &lsm6ds0_drv,
	 .mutex = &g_mutex,
	 .drv_data = &g_lsm6ds0_data[1],
	 .i2c_addr = LSM6DS0_ADDR1,
	 .rot_standard_ref = NULL,
	 .default_odr = 119000,
	 .default_range = 2000
	},

};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

/*
 * Note: If a new sensor driver is added, make sure to update the following
 * assert.
 */
BUILD_ASSERT(ARRAY_SIZE(motion_sensors) == ARRAY_SIZE(g_lsm6ds0_data));

#ifdef CONFIG_DMA_HELP
#include "dma.h"
int command_dma_help(int argc, char **argv)
{
	dma_dump(STM32_DMA2_STREAM0);
	dma_test(STM32_DMA2_STREAM0);
	dma_dump(STM32_DMA2_STREAM0);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(dmahelp, command_dma_help,
			NULL,
			"Run DMA test",
			NULL);
#endif
