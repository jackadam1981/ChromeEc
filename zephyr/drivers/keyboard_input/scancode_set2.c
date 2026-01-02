/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "keyboard_8042_sharedlib.h"

#include <stdint.h>

#include <zephyr/toolchain.h>

#define DT_DRV_COMPAT cros_ec_scancode_set2

/* clang-format off */

#define FOREACH_COL(fn) \
	fn(0) fn(1) fn(2) fn(3) fn(4) fn(5) fn(6) fn(7) fn(8) fn(9) fn(10) \
	fn(11) fn(12) fn(13) fn(14) fn(15) fn(16) fn(17)

#define DT_COL_CHECK(n) \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(0, col##n##_codes), ( \
	BUILD_ASSERT(n < KEYBOARD_COLS_MAX, "extra col codes: " STRINGIFY(n)); \
	BUILD_ASSERT(DT_INST_PROP_LEN(0, col##n##_codes) == 8, \
		     STRINGIFY(col##n##_codes) " must have 8 entries"); \
	),( \
	BUILD_ASSERT(n >= KEYBOARD_COLS_MAX, "missing col codes: " STRINGIFY(n)); \
	))

#define DT_COL(n) \
	IF_ENABLED(DT_INST_NODE_HAS_PROP(0, col##n##_codes), ( \
	DT_INST_PROP(0, col##n##_codes), \
	))

FOREACH_COL(DT_COL_CHECK)

static uint16_t scancode_set2[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	FOREACH_COL(DT_COL)
};

/* clang-format on */

test_mockable uint16_t get_scancode_set2(uint8_t row, uint8_t col)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS) {
		return scancode_set2[col][row];
	}

	return 0;
}

test_mockable void set_scancode_set2(uint8_t row, uint8_t col, uint16_t val)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS) {
		scancode_set2[col][row] = val;
	}
}
