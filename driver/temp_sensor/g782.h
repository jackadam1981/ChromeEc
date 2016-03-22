/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* G782 temperature sensor module for Chrome EC */

#ifndef __CROS_EC_G782_H
#define __CROS_EC_G782_H

#define G782_I2C_ADDR		0x98 /* 7-bit address is 0x4C */

#define G782_IDX_INTERNAL	0
#define G782_IDX_EXTERNAL1	1
#define G782_IDX_EXTERNAL2	2

/* Chip-specific commands */
#define G782_TEMP_LOCAL			0x00
#define G782_TEMP_REMOTE1		0x01
#define G782_TEMP_REMOTE2		0x02
#define G782_STATUS			0x03
#define G782_CONFIGURATION		0x04
#define G782_CONVERSION_RATE		0x05
#define G782_LOCAL_TEMP_HIGH_LIMIT	0x06
#define G782_LOCAL_TEMP_LOW_LIMIT	0x07
#define G782_REMOTE1_TEMP_HIGH_LIMIT	0x08
#define G782_REMOTE1_TEMP_LOW_LIMIT	0x09
#define G782_REMOTE2_TEMP_HIGH_LIMIT	0x0a
#define G782_REMOTE2_TEMP_LOW_LIMIT	0x0b
#define G782_ONESHOT			0x0c
#define G782_REMOTE1_TEMP_EXTENDED	0x0d
#define G782_REMOTE1_TEMP_OFFSET_HIGH	0x0e
#define G782_REMOTE1_TEMP_OFFSET_EXTD	0x0f
#define G782_REMOTE1_T_HIGH_LIMIT_EXTD	0x10
#define G782_REMOTE1_T_LOW_LIMIT_EXTD	0x11
#define G782_REMOTE1_TEMP_THERM_LIMIT	0x12
#define G782_REMOTE2_TEMP_EXTENDED	0x13
#define G782_REMOTE2_TEMP_OFFSET_HIGH	0x14
#define G782_REMOTE2_TEMP_OFFSET_EXTD	0x15
#define G782_REMOTE2_T_HIGH_LIMIT_EXTD	0x16
#define G782_REMOTE2_T_LOW_LIMIT_EXTD	0x17
#define G782_REMOTE2_TEMP_THERM_LIMIT	0x18
#define G782_STATUS1			0x19
#define G782_LOCAL_TEMP_THERM_LIMIT	0x20
#define G782_THERM_HYSTERESIS		0x21
#define G782_ALERT_FAULT_QUEUE_CODE	0x22
#define G782_MANUFACTURER_ID		0xFE
#define G782_DEVICE_ID			0xFF

/* Config register bits */
#define G782_CONFIGURATION_REMOTE2_DIS	(1 << 5)
#define G782_CONFIGURATION_STANDBY	(1 << 6)
#define G782_CONFIGURATION_ALERT_MASK	(1 << 7)

/* Status register bits */
#define G782_STATUS_LOCAL_TEMP_LOW_ALARM	(1 << 0)
#define G782_STATUS_LOCAL_TEMP_HIGH_ALARM	(1 << 1)
#define G782_STATUS_LOCAL_TEMP_THERM_ALARM	(1 << 2)
#define G782_STATUS_REMOTE2_TEMP_THERM_ALARM	(1 << 3)
#define G782_STATUS_REMOTE1_TEMP_THERM_ALARM	(1 << 4)
#define G782_STATUS_REMOTE2_TEMP_FAULT		(1 << 5)
#define G782_STATUS_REMOTE1_TEMP_FAULT		(1 << 6)
#define G782_STATUS_BUSY			(1 << 7)

/* Status1 register bits */
#define G782_STATUS_REMOTE2_TEMP_LOW_ALARM	(1 << 4)
#define G782_STATUS_REMOTE2_TEMP_HIGH_ALARM	(1 << 5)
#define G782_STATUS_REMOTE1_TEMP_LOW_ALARM	(1 << 6)
#define G782_STATUS_REMOTE1_TEMP_HIGH_ALARM	(1 << 7)

/**
 * Get the last polled value of a sensor.
 *
 * @param idx		Index to read. Idx indicates whether to read die
 *			temperature or external temperature.
 * @param temp_ptr	Destination for temperature in K.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int g782_get_val(int idx, int *temp_ptr);

#endif  /* __CROS_EC_G782_H */
