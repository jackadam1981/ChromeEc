/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef UM_PPM_THIRD_PARTY_I2C_AARDVARK_H_
#define UM_PPM_THIRD_PARTY_I2C_AARDVARK_H_

#include "include/smbus.h"

/**
 * Open an Aardvark i2c driver.
 *
 * @param bus_num: Corresponds to Aardvark port from aadetect.
 * @param chip_address: What chip address to open i2c operations on.
 * @param gpio_chip: Which gpiochip has the i2c alert line?
 * @param gpio_line: What line on that gpiochip has the i2c alert?
 * @param transport: Type of transport for underlying SMBUS/I2C. Must be I2C.
 *
 * @return Smbus driver for chosen bus + chip + gpio (alert#) or NULL on error.
 */
struct smbus_driver *i2c_aardvark_open(int bus_num, uint8_t chip_address,
				       int gpio_chip, int gpio_line,
				       uint8_t transport);

#endif // UM_PPM_THIRD_PARTY_I2C_AARDVARK_H_
