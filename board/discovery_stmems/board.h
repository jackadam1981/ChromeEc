/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* STM32L-discovery board configuration. */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/*
 * Enable Sensors:
 * Select LSM6DSM, LIS2DH or both
 */
#define CONFIG_ACCELGYRO_LSM6DSM
#define CONFIG_MAG_LIS2MDL
#undef CONFIG_ACCEL_LIS2DH
#define CONFIG_MAG_LSM6DSM_LIS2MDL
#define CONFIG_BARO_LPS22HB

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

/* Optional features. */
#undef CONFIG_LID_SWITCH
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define CONFIG_CMD_ACCELS
#define CONFIG_CMD_ACCEL_INFO

/* FIFO Support. */
#define CONFIG_ACCEL_FIFO			32
#define CONFIG_ACCEL_FIFO_THRES			(CONFIG_ACCEL_FIFO / 2)

/* I2C master port */
#define I2C_PORT_MASTER STM32_I2C2_PORT

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
#ifdef CONFIG_ACCEL_LIS2DH
	LID_ACCEL,
#endif /* CONFIG_ACCEL_LIS2DH */

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

/* Accelerometer and Gyroscope are the same device. */
#define I2C_PORT_GYRO				I2C_PORT_MASTER
#define I2C_PORT_ACCEL				I2C_PORT_MASTER
#define I2C_PORT_BARO				I2C_PORT_MASTER

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
