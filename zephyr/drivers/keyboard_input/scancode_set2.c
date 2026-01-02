/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "keyboard_8042_sharedlib.h"

#include <stdint.h>

#include <zephyr/toolchain.h>

#define DT_DRV_COMPAT cros_ec_scancode_set2

/* clang-format off */
#define DT_COL_CHECK(prop) \
	IF_ENABLED(DT_INST_NODE_HAS_PROP(0, prop), ( \
	BUILD_ASSERT(DT_INST_PROP_LEN(0, prop) == 8, \
		     STRINGIFY(prop) " must have 8 values"); \
	))
/* clang-format on */

DT_COL_CHECK(col0_codes)
DT_COL_CHECK(col1_codes)
DT_COL_CHECK(col2_codes)
DT_COL_CHECK(col3_codes)
DT_COL_CHECK(col4_codes)
DT_COL_CHECK(col5_codes)
DT_COL_CHECK(col6_codes)
DT_COL_CHECK(col7_codes)
DT_COL_CHECK(col8_codes)
DT_COL_CHECK(col9_codes)
DT_COL_CHECK(col10_codes)
DT_COL_CHECK(col11_codes)
DT_COL_CHECK(col12_codes)
DT_COL_CHECK(col13_codes)
DT_COL_CHECK(col14_codes)
DT_COL_CHECK(col15_codes)
DT_COL_CHECK(col16_codes)
DT_COL_CHECK(col17_codes)

#define DT_COL(prop) \
	IF_ENABLED(DT_INST_NODE_HAS_PROP(0, prop), (DT_INST_PROP(0, prop), ))

static uint16_t scancode_set2[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	/* clang-format off */
	DT_COL(col0_codes)
	DT_COL(col1_codes)
	DT_COL(col2_codes)
	DT_COL(col3_codes)
	DT_COL(col4_codes)
	DT_COL(col5_codes)
	DT_COL(col6_codes)
	DT_COL(col7_codes)
	DT_COL(col8_codes)
	DT_COL(col9_codes)
	DT_COL(col10_codes)
	DT_COL(col11_codes)
	DT_COL(col12_codes)
	DT_COL(col13_codes)
	DT_COL(col14_codes)
	DT_COL(col15_codes)
	DT_COL(col16_codes)
	DT_COL(col17_codes)
	/* clang-format on */
};

uint16_t get_scancode_set2(uint8_t row, uint8_t col)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS) {
		return scancode_set2[col][row];
	}

	return 0;
}

void set_scancode_set2(uint8_t row, uint8_t col, uint16_t val)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS) {
		scancode_set2[col][row] = val;
	}
}
