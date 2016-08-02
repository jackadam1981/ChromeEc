/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

void rand_bytes(void *buf, size_t num)
{
	while (num) {
		long int r = rand();
		size_t count = MIN(num, sizeof(long int));

		memcpy(buf, &r, count);
		buf += count;
		num -= count;
	}
}
