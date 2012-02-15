/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* BDS-specific temp sensor module for Chrome EC */

#include "temp_sensor.h"
#include "chip_temp_sensor.h"
#include "board.h"
#include "i2c.h"

#define TEMP_CASE_DIE_REG_ADDR ((0x40 << 1) | I2C_FLAG_BIG_ENDIAN)
#define TEMP_CASE_DIE_ADDR \
	TMP006_ADDR(I2C_PORT_THERMAL, TEMP_CASE_DIE_REG_ADDR)

static int32_t poll_data[7];

const struct temp_sensor_t temp_sensors[TEMP_SENSOR_COUNT] = {
	{"ECInternal", TEMP_SENSOR_NO_ADDR, chip_temp_sensor_read,
	 TEMP_SENSOR_NO_PRINT, chip_temp_sensor_poll, poll_data},
	{"CaseDie", TEMP_CASE_DIE_ADDR, temp_sensor_tmp006_read_die_temp,
	 temp_sensor_tmp006_print, temp_sensor_tmp006_poll, poll_data + 1},
	{"Object", TEMP_CASE_DIE_ADDR, temp_sensor_tmp006_read_object_temp,
	 TEMP_SENSOR_NO_PRINT, TEMP_SENSOR_NO_POLL, poll_data + 1}
};
