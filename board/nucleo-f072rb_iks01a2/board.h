/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nucleo-F072RB board configuration with ST X-NUCLEO-IKS01A2 daugtherboard */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* the UART console is on USART2 (PA14/PA15) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 2

/*
 * To Enable Sensors, select:
 *  CONFIG_ACCELGYRO_LSM6DSM - Accelerometer + Gyroscope
 *  CONFIG_MAG_LIS2MDL - Magnetometer
 *  CONFIG_MAG_LSM6DSM_LIS2MDL - Magnetometer cascade with Accelerometer
 *   For this mode to work, JP7/JP8 shunts should be 2-3.
 *  CONFIG_ACCEL_LIS2DH - Accelerometer
 *  CONFIG_ACCEL_LIS2DS - Accelerometer
 *  CONFIG_ACCEL_LIS2DE - Accelerometer
 *  CONFIG_ACCEL_LIS2DW12 - Accelerometer
 */
#define CONFIG_ACCELGYRO_LSM6DSM
#define CONFIG_MAG_LIS2MDL
#undef CONFIG_MAG_LSM6DSM_LIS2MDL
#undef CONFIG_ACCEL_LIS2DH
#undef CONFIG_ACCEL_LIS2DW12

#define CONFIG_MAG_CALIBRATE

/* Interrupt management. */
#define CONFIG_ACCEL_INTERRUPTS

/* Custom sensor option. */
#ifdef CONFIG_ACCELGYRO_LSM6DSM
#define CONFIG_ACCEL_LSM6DSM_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(BASE_ACCEL)
#endif
#ifdef CONFIG_ACCEL_LIS2DW12
#define CONFIG_ACCEL_LIS2DW12_INT_EVENT	\
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(LID_ACCEL)
#endif

#define CONFIG_ACCEL_FIFO
/* FIFO size is in power of 2. */
#define CONFIG_ACCEL_FIFO_SIZE 256
#define CONFIG_ACCEL_FIFO_THRES (CONFIG_ACCEL_FIFO_SIZE / 3)

/* Optional features */
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH
#undef CONFIG_HIBERNATE
#define CONFIG_STM_HWTIMER32
#define CONFIG_TASK_PROFILING
#define CONFIG_I2C
#define CONFIG_I2C_DEBUG
#define CONFIG_I2C_MASTER
#define CONFIG_CMD_ACCELS
#define CONFIG_CMD_ACCEL_INFO
#define CONFIG_LTO

/* I2C master port */
#define I2C_PORT_MASTER				STM32_I2C1_PORT


/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

#include "gpio_signal.h"

/* Timer selection */
#define TIM_CLOCK32 2

/* Motion sensors. */
enum sensor_id {
/*
 * Suppose to have:
 *  1) Lid Accelerometer (select LIS2DH, LIS2DW12
 *  2) Base Accelerometer (LSM6DSM)
 *  3) Magnetometer (LIS2MDL or LSM6DSM_LIS2MDL)
 */
#if defined(CONFIG_ACCEL_LIS2DH) || defined(CONFIG_ACCEL_LIS2DW12)
	LID_ACCEL,
#endif /* CONFIG_ACCEL_LIS2DH, CONFIG_ACCEL_LIS2DS, CONFIG_ACCEL_LIS2DW12 */

#ifdef CONFIG_ACCELGYRO_LSM6DSM
	BASE_ACCEL,
	BASE_GYRO,
#endif /* CONFIG_ACCELGYRO_LSM6DSM */

#ifdef CONFIG_MAG_LIS2MDL
	BASE_MAG,
#endif /* CONFIG_MAG_LIS2MDL */
	SENSOR_COUNT,
};

#ifdef CONFIG_ACCEL_LIS2DH
#define IKS01A2_LIS2DH_MODE_MASK (BIT(LID_ACCEL))
#else
#define IKS01A2_LIS2DH_MODE_MASK 0
#endif

#if defined(CONFIG_MAG_LIS2MDL) && !defined(CONFIG_MAG_LSM6DSM_LIS2MDL)
#define IKS01A2_LIS2MDL_MODE_MASK  (BIT(BASE_MAG))
#else
#define IKS01A2_LIS2MDL_MODE_MASK  0
#endif

#define CONFIG_ACCEL_FORCE_MODE_MASK \
	(IKS01A2_LIS2DH_MODE_MASK | IKS01A2_LIS2MDL_MODE_MASK)

#endif /* !__ASSEMBLER__ */

#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096

#endif /* __CROS_EC_BOARD_H */
