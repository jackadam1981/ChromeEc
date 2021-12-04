/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for RGB keyboard.
 */

#include "common.h"
#include "console.h"
#include "rgb_keyboard.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

#define RGB_GRID0_COL 6
#define RGB_GRID0_ROW 11
#define RGB_GRID1_COL 6
#define RGB_GRID1_ROW 11
#define SPI_RGB0_DEVICE_ID 0
#define SPI_RGB1_DEVICE_ID 1

static struct rgb_s grid0[RGB_GRID0_COL * RGB_GRID0_ROW];
static struct rgb_s grid1[RGB_GRID1_COL * RGB_GRID1_ROW];

const struct rgbkbd_drv test_drv;

struct rgbkbd rgbkbds[] = {
	[0] = {
		.cfg = &(const struct rgbkbd_cfg) {
			.drv = &test_drv,
			.spi = SPI_RGB0_DEVICE_ID,
			.col_len = RGB_GRID0_COL,
			.row_len = RGB_GRID0_ROW,
		},
		.buf = grid0,
	},
	[1] = {
		.cfg = &(const struct rgbkbd_cfg) {
			.drv = &test_drv,
			.spi = SPI_RGB1_DEVICE_ID,
			.col_len = RGB_GRID1_COL,
			.row_len = RGB_GRID1_ROW,
		},
		.buf = grid1,
	},
};
const uint8_t rgbkbd_count = ARRAY_SIZE(rgbkbds);
const uint8_t rgbkbd_hsize = RGB_GRID0_COL + RGB_GRID1_COL;
const uint8_t rgbkbd_vsize = RGB_GRID0_ROW;

static uint32_t count_drv_reset;
static uint32_t count_drv_init;
static uint32_t count_drv_enable;
static uint32_t count_drv_set_color;
static uint32_t count_drv_set_scale;
static uint32_t count_drv_set_gcc;

static int test_drv_reset(struct rgbkbd *ctx)
{
	count_drv_reset++;
	return EC_SUCCESS;
}

static int test_drv_init(struct rgbkbd *ctx)
{
	count_drv_init++;
	return EC_SUCCESS;
}

static int test_drv_enable(struct rgbkbd *ctx, bool enable)
{
	count_drv_enable++;
	return EC_SUCCESS;
}

static int test_drv_set_color(struct rgbkbd *ctx, uint8_t offset,
			 struct rgb_s *color, uint8_t len)
{
	count_drv_set_color++;
	return EC_SUCCESS;
}


static int test_drv_set_scale(struct rgbkbd *ctx, uint8_t offset,
			      uint8_t scale, uint8_t len)
{
	count_drv_set_scale++;
	return EC_SUCCESS;
}

static int test_drv_set_gcc(struct rgbkbd *ctx, uint8_t level)
{
	count_drv_set_gcc++;
	return EC_SUCCESS;
}

const struct rgbkbd_drv test_drv = {
	.reset = test_drv_reset,
	.init = test_drv_init,
	.enable = test_drv_enable,
	.set_color = test_drv_set_color,
	.set_scale = test_drv_set_scale,
	.set_gcc = test_drv_set_gcc,
};

static int test_rgbkbd_startup(void)
{
	/* Let RGBKBD task run. */
	msleep(1);

	zassert_equal(count_drv_init, rgbkbd_count, "init() called");
	zassert_equal(count_drv_set_gcc, rgbkbd_count, "set_gcc() called");
	zassert_equal(count_drv_set_scale, rgbkbd_count, "set_scale() called");

	/* Let RGBKBD_DEMO_DOT run. */
	task_wait_event(-1);
	/* Once for on and once for off. */
	zassert_equal(count_drv_set_color, 2, "set_color() called");

	zassert_equal(count_drv_reset, 0, "reset() not called");
	zassert_equal(count_drv_enable, 0, "enable() not called");

	return EC_SUCCESS;
}

void run_test(int argc, char **argv)
{
	RUN_TEST(test_rgbkbd_startup);
	test_print_result();
}
