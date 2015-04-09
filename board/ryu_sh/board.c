/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* ryu sensor hub configuration */

#include "common.h"
#include "console.h"
#include "driver/accelgyro_bmi160.h"
#include "ec_version.h"
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

struct motion_sensor_t motion_sensors[] = {

	/*
	 * Note: lsm6ds0: supports accelerometer and gyro sensor
	 * Requriement: accelerometer sensor must init before gyro sensor
	 * DO NOT change the order of the following table.
	 */
	{SENSOR_ACTIVE_S0_S3, "Accel", MOTIONSENSE_CHIP_BMI160,
		MOTIONSENSE_TYPE_ACCEL, MOTIONSENSE_LOC_LID,
		&bmi160_drv, &g_mutex, NULL,
		BMI160_ADDR0, NULL, 100000, 2},

	{SENSOR_ACTIVE_S0_S3, "Gyro", MOTIONSENSE_CHIP_BMI160,
		MOTIONSENSE_TYPE_GYRO, MOTIONSENSE_LOC_LID,
		&bmi160_drv, &g_mutex, NULL,
		BMI160_ADDR0, NULL, 100000, 2000},

};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

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
