/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This is code that will live in DRAM only.
 */

#include "common.h"
#include "console.h"
#include "util.h"

static int8_t bss_array[4];
static int8_t data_array[4] = { 0, 1, 2, 3 };

static const int8_t const_data_array[4] = { 1, 2, 3, 4 };

int command_my_dram_test(int argc, char **argv)
{
	ccprintf("self %p bss %p data %p const %p\n", &command_my_dram_test,
		bss_array, data_array, const_data_array);

	ccprintf("bss_array: %.*h\n", sizeof(bss_array), bss_array);
	ccprintf("data_array: %.*h\n", sizeof(data_array), data_array);
	memcpy(bss_array, data_array, sizeof(bss_array));
	ccprintf("bss_array: %.*h\n", sizeof(bss_array), bss_array);
	memcpy(data_array, const_data_array, sizeof(data_array));
	ccprintf("data_array: %.*h\n", sizeof(data_array), data_array);

	return EC_SUCCESS;
}
