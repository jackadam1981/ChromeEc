/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "crc16.h"
#include "crc8.h"

#include <zephyr/sys/crc.h>

/* Polynomial representation for x^8 + x^2 + x + 1 is 0x07 */
#define SMBUS_POLYNOMIAL 0x07

/* Polynomial representation for x^8 + x^2 + x + 1 is 0x07 */
#define CPS8100_POLYNOMIAL 0x1021

inline uint8_t cros_crc8(const uint8_t *data, int len)
{
	return crc8(data, len, SMBUS_POLYNOMIAL, 0, false);
}

uint8_t cros_crc8_arg(const uint8_t *data, int len, uint8_t previous_crc)
{
	return crc8(data, len, SMBUS_POLYNOMIAL, previous_crc, false);
}

inline uint8_t cros_crc16(const uint16_t *data, int len)
{
	return crc16(CPS8100_POLYNOMIAL, 0, data, len);
}

uint8_t cros_crc16_arg(const uint16_t *data, int len, uint16_t previous_crc)
{
	return crc16(CPS8100_POLYNOMIAL, previous_crc, data, len);
}
