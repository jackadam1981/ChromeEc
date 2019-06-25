/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C/SPI Slave Address Chrome EC */

#ifndef __CROS_EC_I2C_SPI_SLAVE_H
#define __CROS_EC_I2C_SPI_SLAVE_H

#include "common.h"

struct slave_addr_t {
	/* I2C/SPI device address */
	union {
		uint16_t	i2c_addr__7b:10;
		uint16_t	spi_dev_id:10;
	};

	/* Condition flags */
	uint16_t	is_spi:1;
	uint16_t	is_big_endian:1;
	uint16_t	is_10bit_address:1;
	uint16_t	__reserved__:3;

	/* Port selector */
	uint16_t	port;
};

#endif  /* __CROS_EC_I2C_SPI_SLAVE_H */
