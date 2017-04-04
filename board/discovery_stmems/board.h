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
#define CONFIG_ACCEL_LIS2DH

/* Interrupt management. */
#define CONFIG_ACCEL_INTERRUPTS

/* Custom sensor option. */
#define CONFIG_ACCEL_LIS2DH_INT_EVENT TASK_EVENT_CUSTOM(4)
#define CONFIG_ACCEL_LSM6DSM_INT_EVENT TASK_EVENT_CUSTOM(5)

/* Optional features. */
#undef CONFIG_LID_SWITCH
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define CONFIG_CMD_ACCELS
#define CONFIG_CMD_ACCEL_INFO

/* FIFO Support. */
#define CONFIG_ACCEL_FIFO 32
#define CONFIG_ACCEL_FIFO_THRES (CONFIG_ACCEL_FIFO / 2)

/* I2C master port */
#define I2C_PORT_MASTER STM32_I2C2_PORT

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK_MSB 3
#define TIM_CLOCK_LSB 4
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
};


/* Accelerometer and Gyroscope are the same device. */
#define I2C_PORT_GYRO			I2C_PORT_MASTER
#define I2C_PORT_ACCEL			I2C_PORT_MASTER

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
