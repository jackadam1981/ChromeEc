/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "common.h"
#include "crc8.h"

/**
 * crc8
 * Return CRC-8 of the data, using x^8 + x^2 + x + 1 polynomial.  A table-based
 * algorithm would be faster, but for only a few bytes it isn't worth the code
 * size.
 * @param data uint8_t *, input, a pointer to input data
 * @param len int, input, size of iput data in byte
 * @return the crc-8 of the input data.
 */
uint8_t crc8(const uint8_t *data, int len)
{
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

/**
 * crc8_append
 * - Compute crc8 incrementally
 * - Provided By chei-hong.ho@quantatw.com
 * @param pec uint8_t *, inout, a pointer to inital & final pec value
 * @param src uint8_t *, input, a pointer to input data
 * @param len int, size of input data in byte
 * @return void
 */
#define BIT7 7
#define IS_MASK_CLEAR(v, b) !(v & (1<<b))
void crc8_append(uint8_t *pec, uint8_t *src, int len)
{
	uint8_t temp;
	int i;
	for (i = 0; i < len; i++) {
		*pec ^= src[i];
		temp = *pec;
		if (IS_MASK_CLEAR(temp, BIT7)) {
			temp = temp << 1;
			*pec ^= temp;
		} else {
			temp = temp << 1;
			*pec ^= 0x09;
			*pec ^= temp;
		}
		if (IS_MASK_CLEAR(temp, BIT7)) {
			temp = temp << 1;
			*pec ^= temp;
		} else {
			temp = temp << 1;
			*pec ^= 0x07;
			*pec ^= temp;
		}
	}
}
