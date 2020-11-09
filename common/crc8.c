/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifdef CONFIG_ZEPHYR
#include <sys/crc.h>
#endif

#include "common.h"
#include "crc8.h"

inline uint8_t crc_8(const uint8_t *data, int len)
{
	return crc_8_arg(data, len, 0);
}

uint8_t crc_8_arg(const uint8_t *data, int len, uint8_t previous_crc)
{
#ifdef CONFIG_ZEPHYR
	/* Polynomial representation for x^8 + x^2 + x + 1 is 0x07 */
	#define SMBUS_POLYNOMIAL 0x07
	return crc8(data, len, SMBUS_POLYNOMIAL, previous_crc, false);
#else
	unsigned crc = previous_crc << 8;
	int i, j;

	for (j = len; j; j--, data++) {
		crc ^= (*data << 8);
		for (i = 8; i; i--) {
			if (crc & 0x8000)
				crc ^= (0x1070 << 3);
			crc <<= 1;
		}
	}

	return (uint8_t)(crc >> 8);
#endif
}
