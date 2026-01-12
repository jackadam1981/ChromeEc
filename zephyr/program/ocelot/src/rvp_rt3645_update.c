/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "drivers/imvp/rt3645.h"

const int crc = 0x92;

const struct rt3645_info update_data[] = {
	{ RT3645_PAGE_5, REG_04, 0xA5 },
	{ RT3645_PAGE_5, REG_0C, 0xC1 },
	{ RT3645_PAGE_9, REG_00, 0x32 },
	{ RT3645_PAGE_9, REG_01, 0x27 },
	{ RT3645_PAGE_9, REG_03, 0xAF },
	{ RT3645_PAGE_9, REG_04, 0xD0 },
	{ RT3645_PAGE_9, REG_08, 0xAD },
	{ RT3645_PAGE_9, REG_0B, 0xF9 },
	{ RT3645_PAGE_A, REG_01, 0x2A },
	{ RT3645_PAGE_A, REG_08, 0x89 },
	{ RT3645_PAGE_A, REG_0B, 0xFA },
	{ RT3645_PAGE_B, REG_00, 0x24 },
	{ RT3645_PAGE_B, REG_01, 0x28 },
	{ RT3645_PAGE_B, REG_03, 0xF2 },
	{ RT3645_PAGE_B, REG_04, 0x00 },
	{ RT3645_PAGE_B, REG_05, 0x3E },
	{ RT3645_PAGE_B, REG_06, 0x3c },
	{ RT3645_PAGE_B, REG_07, 0x2B },
	{ RT3645_PAGE_B, REG_08, 0x03 },
	{ RT3645_PAGE_B, REG_0B, 0xF7 },
	{ RT3645_PAGE_B, REG_0C, 0x11 },
	{ RT3645_PAGE_B, REG_0D, 0xF8 },
	{ RT3645_PAGE_B, REG_0E, 0xBF },
	{ RT3645_PAGE_B, REG_0F, 0x4F },
	{ RT3645_PAGE_B, REG_10, 0x01 },
	{ RT3645_PAGE_B, REG_12, 0x00 },
	{ RT3645_PAGE_C, REG_01, 0x29 },
	{ RT3645_PAGE_C, REG_07, 0x30 },
	{ RT3645_PAGE_C, REG_08, 0xED },
	{ RT3645_PAGE_C, REG_0B, 0xF9 },
	{ RT3645_PAGE_C, REG_10, 0x13 },
	{ RT3645_PAGE_D, REG_05, 0x70 },
	{ RT3645_PAGE_D, REG_07, 0x40 }
};

const size_t update_data_size = ARRAY_SIZE(update_data);	
