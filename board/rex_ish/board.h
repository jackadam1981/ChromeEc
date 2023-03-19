/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Rex ISH board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#include "baseboard.h"

#define CONFIG_ACCELGYRO_LSM6DSO /* Base accel */
#define CONFIG_ACCEL_LSM6DSO_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(BASE_ACCEL)

/* TCS3400 ALS */
#define CONFIG_ALS
#define ALS_COUNT 1
#define CONFIG_ALS_TCS3400
#define CONFIG_ALS_TCS3400_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(CLEAR_ALS)

#define CONFIG_ACCEL_FORCE_MODE_MASK BIT(CLEAR_ALS)

/* Enable sensor fifo, must also define the _SIZE and _THRES */
#define CONFIG_ACCEL_FIFO
/* FIFO size is in power of 2. */
#define CONFIG_ACCEL_FIFO_SIZE 256
/* Depends on how fast the AP boots and typical ODRs */
#define CONFIG_ACCEL_FIFO_THRES (CONFIG_ACCEL_FIFO_SIZE / 3)

/* Lid accel */
#define CONFIG_LID_ANGLE
#define CONFIG_LID_ANGLE_UPDATE
#define CONFIG_LID_ANGLE_SENSOR_BASE BASE_ACCEL
#define CONFIG_LID_ANGLE_SENSOR_LID LID_ACCEL
#define CONFIG_ACCEL_LIS2DWL
#define CONFIG_ACCEL_LIS2DW12_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(LID_ACCEL)

/* I2C ports */
#define I2C_PORT_SENSOR ISH_I2C0

/*
 * Disable power management.
 * PM for ISH5.6 is not implemented in legacy ECOS ISH;
 * it will be implemented in Zephyr OS ISH.
 */
#undef CONFIG_LOW_POWER_IDLE
#undef CONFIG_ISH_PM_D0I1
#undef CONFIG_ISH_PM_D0I2
#undef CONFIG_ISH_PM_D0I3
#undef CONFIG_ISH_PM_D3
#undef CONFIG_ISH_PM_RESET_PREP
#undef CONFIG_ISH_IPAPG
#undef CONFIG_ISH_NEW_PM

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

/* Motion sensors */
enum sensor_id {
	LID_ACCEL = 0,
	BASE_ACCEL,
	BASE_GYRO,
	CLEAR_ALS,
	RGB_ALS,
	SENSOR_COUNT
};

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
