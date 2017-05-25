/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* STM32L-discovery board configuration. */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Enable console accels commands. */
#define CONFIG_CMD_ACCELS

/*
 * To Enable Sensors, select:
 *  CONFIG_MAG_LIS2MDL - Magnetometer
 *  CONFIG_MAG_LSM6DSM_LIS2MDL - Magnetometer cascade with Accelerometer
 *  CONFIG_ACCELGYRO_LSM6DSM - Accelerometer + Gyroscope
 *  CONFIG_ACCEL_LIS2DH - Accelerometer
 *  CONFIG_ACCEL_LIS2DS - Accelerometer
 *  CONFIG_ACCEL_LIS2DE - Accelerometer
 *  CONFIG_ACCEL_LIS2DW12 - Accelerometer
 *  CONFIG_BARO_LPS22HB - Barometer
 */
#undef CONFIG_ACCELGYRO_LSM6DSM
#undef CONFIG_MAG_LIS2MDL
#undef CONFIG_ACCEL_LIS2DH
#undef CONFIG_MAG_LSM6DSM_LIS2MDL
#undef CONFIG_BARO_LPS22HB
#undef  CONFIG_ACCEL_LIS2DS
#undef CONFIG_ACCEL_LIS2DE
#define CONFIG_ACCEL_LIS2DW12

/* Interrupt management. */
#define CONFIG_ACCEL_INTERRUPTS

/* Gesture Configuration. */
#define CONFIG_GESTURE_DETECTION
#define CONFIG_GESTURE_HOST_DETECTION
#define CONFIG_GESTURE_SAMPLING_INTERVAL_MS	5

/* First sensor is motion_sensor is used for significant motion. */
#define CONFIG_GESTURE_SIGMO			0
#define CONFIG_GESTURE_SIGMO_PROOF_MS		500
#define CONFIG_GESTURE_SIGMO_SKIP_MS		3000
#define CONFIG_GESTURE_SIGMO_THRES_MG		500

#define CONFIG_GESTURE_SENSOR_BATTERY_TAP	0
#define CONFIG_GESTURE_TAP_THRES_MG		100
#define CONFIG_GESTURE_TAP_MAX_INTERSTICE_T	500
#define CONFIG_GESTURE_DETECTION_MASK \
	((1 << CONFIG_GESTURE_SIGMO) | \
	 (1 << CONFIG_GESTURE_SENSOR_BATTERY_TAP))
#define CONFIG_GESTURE_TAP_EVENT		TASK_EVENT_CUSTOM(1024)
#define CONFIG_GESTURE_SIGMO_EVENT		TASK_EVENT_CUSTOM(2048)

/* Custom sensor option. */
#define CONFIG_ACCEL_LIS2DH_INT_EVENT		TASK_EVENT_CUSTOM(4)
#define CONFIG_ACCEL_LSM6DSM_INT_EVENT		TASK_EVENT_CUSTOM(5)
#define CONFIG_ACCEL_LIS2DS_INT_EVENT		TASK_EVENT_CUSTOM(6)
#define CONFIG_ACCEL_LIS2DE_INT_EVENT		TASK_EVENT_CUSTOM(7)
#define CONFIG_ACCEL_LIS2DW12_INT_EVENT		TASK_EVENT_CUSTOM(8)

/* Optional features. */
#undef CONFIG_LID_SWITCH
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define CONFIG_CMD_ACCELS
#define CONFIG_CMD_ACCEL_INFO

/* FIFO Support. */
#define CONFIG_ACCEL_FIFO			32
#define CONFIG_ACCEL_FIFO_THRES			(CONFIG_ACCEL_FIFO - 1)

/* I2C master port */
#define I2C_PORT_MASTER 			STM32_I2C2_PORT

/*
 * Allow dangerous commands all the time, since we
 * don't have a write protect switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK_MSB				3
#define TIM_CLOCK_LSB				4
#undef  CONFIG_WATCHDOG_HELP

/* Motion sensors. */
enum sensor_id {
/*
 * Suppose to have:
 *  1) Lid Accelerometer (select LIS2DH, LIS2DS, LIS2DW12 or LIS2DE
 *  2) Base Accelerometer (LSM6DSM)
 *  3) Magnetometer (LIS2MDL or LSM6DSM_LIS2MDL)
 *  4) Barometer (LPS22HB)
 */
#if defined(CONFIG_ACCEL_LIS2DH) || defined(CONFIG_ACCEL_LIS2DS) || \
	defined(CONFIG_ACCEL_LIS2DW12) || defined(CONFIG_ACCEL_LIS2DE)
	LID_ACCEL = 0,
#endif /* CONFIG_ACCEL_LIS2DH, CONFIG_ACCEL_LIS2DS, CONFIG_ACCEL_LIS2DW12 */

#ifdef CONFIG_ACCELGYRO_LSM6DSM
	BASE_ACCEL,
	BASE_GYRO,
#endif /* CONFIG_ACCELGYRO_LSM6DSM */

#ifdef CONFIG_MAG_LIS2MDL
	BASE_MAG,
#endif /* CONFIG_MAG_LIS2MDL */

#ifdef CONFIG_BARO_LPS22HB
	BASE_BARO,
#endif /* CONFIG_BARO_LPS22HB */
};

/* Accelerometer, Gyroscope, Mag and Barometer on the device port. */
#define I2C_PORT_GYRO				I2C_PORT_MASTER
#define I2C_PORT_ACCEL				I2C_PORT_MASTER
#define I2C_PORT_BARO				I2C_PORT_MASTER
#define I2C_PORT_MAG				I2C_PORT_MASTER

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
