/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Link-specific temp sensor module for Chrome EC */

#include "temp_sensor.h"
#include "chip_temp_sensor.h"
#include "board.h"
#include "i2c.h"
#include "peci.h"

#define TEMP_CPU_REG_ADDR ((0x40 << 1) | I2C_FLAG_BIG_ENDIAN)
#define TEMP_PCH_REG_ADDR ((0x41 << 1) | I2C_FLAG_BIG_ENDIAN)
#define TEMP_DDR_REG_ADDR ((0x43 << 1) | I2C_FLAG_BIG_ENDIAN)
#define TEMP_CHARGER_REG_ADDR ((0x45 << 1) | I2C_FLAG_BIG_ENDIAN)

#define TEMP_CPU_ADDR TMP006_ADDR(I2C_PORT_THERMAL, TEMP_CPU_REG_ADDR)
#define TEMP_PCH_ADDR TMP006_ADDR(I2C_PORT_THERMAL, TEMP_PCH_REG_ADDR)
#define TEMP_DDR_ADDR TMP006_ADDR(I2C_PORT_THERMAL, TEMP_DDR_REG_ADDR)
#define TEMP_CHARGER_ADDR TMP006_ADDR(I2C_PORT_THERMAL, TEMP_CHARGER_REG_ADDR)

static int32_t poll_data[37];

/* Temperature sensors data. Must be in the same order as enum
 * temp_sensor_id.
 */
const struct temp_sensor_t temp_sensors[TEMP_SENSOR_COUNT] = {
	{"I2C_CPU-Die", TEMP_CPU_ADDR, temp_sensor_tmp006_read_die_temp,
	 temp_sensor_tmp006_print, temp_sensor_tmp006_poll, poll_data},
	{"I2C_CPU-Object", TEMP_CPU_ADDR, temp_sensor_tmp006_read_object_temp,
	 TEMP_SENSOR_NO_PRINT, TEMP_SENSOR_NO_POLL, poll_data},
	{"I2C_PCH-Die", TEMP_PCH_ADDR, temp_sensor_tmp006_read_die_temp,
	 temp_sensor_tmp006_print, temp_sensor_tmp006_poll, poll_data + 9},
	{"I2C_PCH-Object", TEMP_PCH_ADDR, temp_sensor_tmp006_read_object_temp,
	 TEMP_SENSOR_NO_PRINT, TEMP_SENSOR_NO_POLL, poll_data + 9},
	{"I2C_DDR-Die", TEMP_DDR_ADDR, temp_sensor_tmp006_read_die_temp,
	 temp_sensor_tmp006_print, temp_sensor_tmp006_poll, poll_data + 18},
	{"I2C_DDR-Object", TEMP_DDR_ADDR, temp_sensor_tmp006_read_object_temp,
	 TEMP_SENSOR_NO_PRINT, TEMP_SENSOR_NO_POLL, poll_data + 18},
	{"I2C_Charger-Die", TEMP_CHARGER_ADDR, temp_sensor_tmp006_read_die_temp,
	 temp_sensor_tmp006_print, temp_sensor_tmp006_poll, poll_data + 27},
	{"I2C_Charger-Object", TEMP_CHARGER_ADDR,
	 temp_sensor_tmp006_read_object_temp, TEMP_SENSOR_NO_PRINT,
	 TEMP_SENSOR_NO_POLL, poll_data + 27},
	{"ECInternal", TEMP_SENSOR_NO_ADDR, chip_temp_sensor_read,
	 TEMP_SENSOR_NO_PRINT, chip_temp_sensor_poll, poll_data + 36},
	{"PECI", TEMP_SENSOR_NO_ADDR, peci_temp_sensor_read,
	 TEMP_SENSOR_NO_PRINT, TEMP_SENSOR_NO_POLL, 0},
};
