/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* STM32L-discovery board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* =============== Enabled Sensors ================ */
#define CONFIG_ACCEL_LIS2DH

/* ============= Interrupt management ============== */
#define CONFIG_ACCEL_INTERRUPTS

/* ============= Custom sensor option ============== */
#if defined(CONFIG_ACCEL_LIS2DH)
#define CONFIG_ACCEL_LIS2DH_INT_EVENT TASK_EVENT_CUSTOM(4)
#endif /* CONFIG_ACCEL_LIS2DH */

/* =============== Optional features =============== */
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH
#define CONFIG_I2C
#undef CONFIG_I2C_DEBUG
#define CONFIG_I2C_MASTER
#define CONFIG_CMD_ACCELS
#define CONFIG_CMD_ACCEL_INFO

/* =============== FIFO Support =============== */
#ifdef CONFIG_ACCEL_LIS2DH
#define CONFIG_ACCEL_FIFO 32
#define CONFIG_ACCEL_FIFO_THRES (CONFIG_ACCEL_FIFO / 2)
#endif /* CONFIG_ACCEL_LIS2DH */

/* =============== I2C master port =============== */
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

/* =============== Motion sensors =============== */
enum sensor_id {
#ifdef CONFIG_ACCEL_LIS2DH
	LID_ACCEL = 0,
#endif /* CONFIG_ACCEL_LIS2DH */
};

#define I2C_PORT_ACCEL			I2C_PORT_MASTER

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
