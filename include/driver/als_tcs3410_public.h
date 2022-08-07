/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * AMS TCS3410 light sensor driver
 */

#ifndef __CROS_EC_DRIVER_ALS_TCS3410_PUBLIC_H
#define __CROS_EC_DRIVER_ALS_TCS3410_PUBLIC_H

#include "accelgyro.h"

/* I2C Interface */
#define TCS3410_I2C_ADDR_FLAGS 0x39

/* Min and Max sampling frequency in mHz */
#define TCS3410_LIGHT_MIN_FREQ 149
#define TCS3410_LIGHT_MAX_FREQ 1000
#if (CONFIG_EC_MAX_SENSOR_FREQ_MILLIHZ <= TCS3410_LIGHT_MAX_FREQ)
#error "EC too slow for light sensor"
#endif

extern const struct accelgyro_drv tcs3410_drv;
extern const struct accelgyro_drv tcs3410_rgb_drv;

void tcs3410_interrupt(enum gpio_signal signal);

#if defined(CONFIG_ZEPHYR)
#if DT_NODE_EXISTS(DT_ALIAS(tcs3410_int))
/*
 * Get the mostion sensor ID of the TCS3410 sensor that
 * generates the interrupt.
 * The interrupt is converted to the event and transferred to motion
 * sense task that actually handles the interrupt.
 *
 * Here, we use alias to get the motion sensor ID
 *
 * e.g) als_clear below is the label of a child node in /motionsense-sensors
 * aliases {
 *     tcs3410-int = &als_clear;
 * };
 */
#define CONFIG_ALS_TCS3410_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(SENSOR_ID(DT_ALIAS(tcs3410_int)))
#endif
#endif /* CONFIG_ZEPHYR */

#endif /* __CROS_EC_DRIVER_ALS_TCS3410_PUBLIC_H */
