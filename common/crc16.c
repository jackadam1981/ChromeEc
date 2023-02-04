/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "common.h"
#include "crc16.h"

inline uint16_t cros_crc16(const uint8_t *data, int len)
{
	return cros_crc16_arg(data, len, 0);
}

uint16_t cros_crc16_arg(const uint8_t *data, int len, uint16_t previous_crc)
{
	int i, j;
	uint16_t crc_poly = 0x1021;
	uint16_t crc16;

	crc16 = previous_crc;
	for (i = 0; i < len; i++) {
		crc16 ^= (data[i] << 8);
		for (j = 0; j < 8; j++) {
			if (crc16 & 0x8000)
				crc16 = (crc16 << 1) ^ crc_poly;
			else
				crc16 = crc16 << 1;
		}
	}

	return crc16;
}
