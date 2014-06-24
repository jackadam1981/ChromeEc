/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "common.h"
#include "crc.h"

/**
 * Return CRC-8 of the data, using x^8 + x^2 + x + 1 polynomial.  A table-based
 * algorithm would be faster, but for only a few bytes it isn't worth the code
 * size. */
uint8_t crc8(const void *vptr, int len)
{
	const uint8_t *data = vptr;
	unsigned crc = 0;
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
}


#define BIT7 7
#define IS_MASK_CLEAR(v, b) !(v & (1<<b))
void Crc8(uint8_t src, uint8_t *pec)
{
	uint8_t temp;
	*pec ^= src;
	temp = *pec;
	if (IS_MASK_CLEAR(temp, BIT7)) {
		temp = temp<<1;
		*pec ^= temp;
	} else {
		temp = temp<<1;
		*pec ^= 0x09;
		*pec ^= temp;
	}
	if (IS_MASK_CLEAR(temp, BIT7)) {
		temp = temp<<1;
		*pec ^= temp;
	} else {
		temp = temp<<1;
		*pec ^= 0x07;
		*pec ^= temp;
	}
}
