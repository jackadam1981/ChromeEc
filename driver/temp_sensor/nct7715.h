/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_NCT7715_H
#define __CROS_EC_NCT7715_H

#include "i2c.h"

#define NCT7715_I2C_ADDR_FLAGS0 (0x48 | I2C_FLAG_BIG_ENDIAN)
#define NCT7715_I2C_ADDR_FLAGS1 (0x49 | I2C_FLAG_BIG_ENDIAN)
#define NCT7715_I2C_ADDR_FLAGS2 (0x4A | I2C_FLAG_BIG_ENDIAN)
#define NCT7715_I2C_ADDR_FLAGS3 (0x4B | I2C_FLAG_BIG_ENDIAN)
#define NCT7715_I2C_ADDR_FLAGS4 (0x4C | I2C_FLAG_BIG_ENDIAN)
#define NCT7715_I2C_ADDR_FLAGS5 (0x4D | I2C_FLAG_BIG_ENDIAN)
#define NCT7715_I2C_ADDR_FLAGS6 (0x4E | I2C_FLAG_BIG_ENDIAN)
#define NCT7715_I2C_ADDR_FLAGS7 (0x4F | I2C_FLAG_BIG_ENDIAN)

#define NCT7715_REG_TEMP 0x00
#define NCT7715_REG_CONF 0x01
#define NCT7715_REG_ALTL 0x02
#define NCT7715_REG_AHTL 0x03

#define NCT7715_CONF_OS BIT(15)
#define NCT7715_CONF_R_MASK (BIT(14) | BIT(13))
/* EM = 0: Normal mode 12 bit, 1: Extended mode 13 bit */
#define NCT7715_CONF_EM BIT(4)

#define NCT7715_REG_CHIP_ID 0xFD
#define NCT7715_CHIP_ID 0xD1B5

#define NCT7715_REG_VENDOR_ID 0xFE
#define NCT7715_VENDOR_ID 0x50FF

#define NCT7715_REG_DEVICE_ID 0xFF
#define NCT7715_DEVICE_ID 0x31FF

/*
 * I2C port and address information for all the board PCT2075 sensors should be
 * defined in an array of the following structures, with an enum PCT2075_sensor
 * indexing the array.  The enum PCT2075_sensor shall end with a PCT2075_COUNT
 * defining the maximum number of sensors for the board.
 */

struct nct7715_sensor_t {
	int i2c_port;
	int i2c_addr_flags;
};

extern const struct nct7715_sensor_t nct7715_sensors[];

/**
 * Get the last polled value of a sensor.
 *
 * @param idx		Index to read, from board's enum PCT2075_sensor
 *			definition
 *
 * @param temp_k_ptr	Destination for temperature in K.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int nct7715_get_val_k(int idx, int *temp_k_ptr);

/**
 * Get the last polled value of a sensor.
 *
 * @param idx		Index to read, from board's enum PCT2075_sensor
 *			definition
 *
 * @param temp_mk_ptr	Destination for temperature in mK.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int nct7715_get_val_mk(int idx, int *temp_mk_ptr);

/**
 * Init the sensors.  Note, this will run automatically on HOOK_INIT, but is
 * made available for boards which may not always power the sensor in all
 * states.
 */
void nct7715_init(void);

#ifdef CONFIG_ZEPHYR
void nct7715_update_temperature(int idx);
#endif /* CONFIG_ZEPHYR */

#endif /* __CROS_EC_NCT7715_H */
