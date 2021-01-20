/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Sensor configuration on Volteer board */

#ifndef __ZEPHYR_SENSOR_MAP_H
#define __ZEPHYR_SENSOR_MAP_H

/*
 * TODO(b/173507858) : Move CONFIG_xxx below to "Kconfig.motionsense"
 * #define CONFIG_CMD_ACCELS
 * #define CONFIG_CMD_ACCEL_INFO
*/
#define CONFIG_LID_ANGLE_UPDATE

/*
 * TODO(b/173507858) : Move below to dts and crates map to ECOS definitions
*/

enum sensor_id {
	LID_ACCEL = 0,
	BASE_ACCEL,
	BASE_GYRO,
	CLEAR_ALS,
	RGB_ALS,
	SENSOR_COUNT,
};

#ifdef CONFIG_ALS_TCS3400
#define CONFIG_ALS_TCS3400_INT_EVENT \
        TASK_EVENT_MOTION_SENSOR_INTERRUPT(CLEAR_ALS)
#endif

#ifdef CONFIG_ACCELGYRO_BMI260
#define CONFIG_ACCELGYRO_BMI260_INT_EVENT \
        TASK_EVENT_MOTION_SENSOR_INTERRUPT(BASE_ACCEL)
#endif

#ifdef CONFIG_LID_ANGLE
#define CONFIG_LID_ANGLE_SENSOR_BASE            BASE_ACCEL
#define CONFIG_LID_ANGLE_SENSOR_LID             LID_ACCEL
#endif

#ifdef CONFIG_GMR_TABLET_MODE
#define GMR_TABLET_MODE_GPIO_L                  GPIO_TABLET_MODE_L
#endif

/* Sensors without hardware FIFO are in forced mode */
#define CONFIG_ACCEL_FORCE_MODE_MASK    (BIT(LID_ACCEL) | BIT(CLEAR_ALS))

#endif /* __ZEPHYR_SENSOR_MAP_H */
