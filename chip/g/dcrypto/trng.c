/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "registers.h"

void init_trng(void)
{
	GWRITE(TRNG, POWER_DOWN_B, 1);
	GWRITE(TRNG, GO_EVENT, 1);
	while (GREAD(TRNG, EMPTY))
		;
	GREAD(TRNG, READ_DATA);
}

uint32_t rand(void)
{
	while (GREAD(TRNG, EMPTY))
		;
	return GREAD(TRNG, READ_DATA);
}

void rand_bytes(uint8_t *buf, uint32_t len)
{
	while (len) {
		int i;
		uint32_t r = rand();

		for (i = 0; i < 4 && len; i++) {
			uint8_t b = ((uint8_t *) &r)[i];

			*buf++ = b;
			len--;
		}
	}
}
