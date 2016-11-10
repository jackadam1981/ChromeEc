/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dcrypto.h"

/* Constant time drop drop in replacement for memcmp(). */
int DCRYPTO_memcmp(const void *a, const void *b, size_t len)
{
	size_t i;
	const uint8_t *pa = a;
	const uint8_t *pb = b;

	int16_t borrow = 0;
	uint8_t notzero = 0;

	for (i = len - 1; i >= 0; i--) {
		borrow += ((int16_t) pa[i]) - pb[i];
		/* Track whether any result digit is ever not zero.
		 * Relies on !!(non-zero) evaluating to 1, e.g.,
		 * !!(-1) evaluating to 1.
		 */
		notzero |= !!((uint8_t) borrow);
		borrow >>= 8;
	}
	return ((int)borrow) | notzero;
}
