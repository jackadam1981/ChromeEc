/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>

#include "rtc_common.h"

int bcd_to_dec(uint8_t bcd, enum bcd_mask mask)
{
	int tens = ((bcd & mask) >> 4) * 10;
	int ones = (bcd & 0xf);

	return tens + ones;
}

uint8_t dec_to_bcd(uint32_t val, enum bcd_mask mask)
{
	int tens = val / 10;
	int ones = val - (tens * 10);

	return ((tens << 4) & mask) | ones;
}
