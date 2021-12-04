/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include "board.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "rgb_keyboard.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#undef DEBUG

/* Console output macros */
#define CPUTS(outstr) cputs(CC_RGBKBD, outstr)
#define CPRINTF(fmt, args...) cprintf(CC_RGBKBD, "RGBKBD: " fmt, ##args)
#define CPRINTS(fmt, args...) cprints(CC_RGBKBD, "RGBKBD: " fmt, ##args)

/*
 * The matrix consists of multiple grids:
 *
 *   +=========-== Matrix =============+
 *   | +--- Grid1 ---+ +--- Grid2 ---+ |
 *   | | A C         | | Y           | |
 *   | | B           | |             | |
 *   | |           X | |             | |
 *   | +-------------+ +-------------+ |
 *   +=================================+
 *
 * Grids are assumed to be horizontally adjacent. That is, the matrix row size
 * is fixed and the matrix column size is a multiple of the grid's column size.
 *
 * Coordinate format is (column, row). In the diagram above, A, B, and C are:
 *   A = (0, 0)
 *   B = (0, 1)
 *   C = (1, 0)
 *
 * In each grid, LEDs are also sequentially indexed. That is, A = 0, B, 1, ...
 *   C = GRID_ROW_SIZE,
 *   X = GRID_ROW_SIZE * GRID_COLUMN_SIZE - 1
 *   Y = 0
 */

/* TODO: Should be defined in rgbkbd_cfg by each board. */
#define GRID_ROW_SIZE		6
#define MATRIX_ROW_SIZE		GRID_ROW_SIZE
#define GRID_COL_SIZE		11
#define GRID_SIZE		(GRID_ROW_SIZE * GRID_COL_SIZE)
#define GRID_COUNT		2

static struct rgb_s grid[GRID_COUNT][GRID_SIZE];

static int rgbkbd_set_color_single(struct rgb_s color, int row, int col)
{
	struct rgbkbd *ctx;
	const int gid = col / GRID_COL_SIZE; /* COL11 belongs to CH1. */
	const int offset = row + (col - gid * GRID_COL_SIZE) * GRID_ROW_SIZE;

	if (gid >= rgbkbd_count)
		return EC_ERROR_OVERFLOW;

	grid[gid][offset] = color;
	ctx = &rgbkbds[gid];

	return ctx->cfg->drv->set_color(ctx, offset, &grid[gid][offset], 1);
}

static void rgbkbd_sync(void)
{
	struct rgbkbd *ctx;
	uint8_t len;
	int i;

	for (i = 0; i < rgbkbd_count; i++) {
		ctx = &rgbkbds[i];
		len = ctx->cfg->col_len * ctx->cfg->row_len;
		ctx->cfg->drv->set_color(ctx, 0, grid[i], len);
	}
}

static int demo = 1;

static void rgbkbd_demo(int pattern)
{
	struct rgb_s color = {};
	const int step = 32;
	int i;

	rgbkbd_sync();

	/* Shift LED colors (for the next call). */
	color.r += step;
	if (color.r == 0) {
		color.g += step;
		if (color.g == 0)
			color.b += step;
	}

	for (i = 1; i < GRID_SIZE; i++)
		grid[1][GRID_SIZE - i] = grid[1][GRID_SIZE - i - 1];
	grid[1][0] = grid[0][GRID_SIZE - 1];
	for (i = 1; i < GRID_SIZE; i++)
		grid[0][GRID_SIZE - i] = grid[0][GRID_SIZE - i - 1];
	grid[0][0] = color;
}

void rgbkbd_task(void *u)
{
	uint32_t event;
	int i;

#ifdef GPIO_RGBKBD_POWER
	/* Power on the RGB keyboard module. */
	gpio_set_level(GPIO_RGBKBD_POWER, 1);
	msleep(10);
#endif

	for (i = 0; i < rgbkbd_count; i++) {
		struct rgbkbd *ctx = &rgbkbds[i];
		ctx->cfg->drv->init(ctx);
	}

	while (1) {
		event = task_wait_event(100 * MSEC);
		if (IS_ENABLED(DEBUG))
			CPRINTS("event=0x%08x", event);
		if (demo)
			rgbkbd_demo(demo);
	}
}

static int cc_rgbk(int argc, char **argv)
{
	struct rgbkbd *ctx;
	char *end, *comma;
	struct rgb_s color;
	int gcc, col, row, val;
	int i, j;

	if (5 < argc)
		return EC_ERROR_PARAM_COUNT;

	if (argc < 2) {
		ccprintf("Start demo\n");
		demo = 1;
		return EC_SUCCESS;
	}

	comma = strstr(argv[1], ",");
	if (comma && strlen(comma) > 1) {
		/* Usage 2 */
		/* Found ',' and more string after that. Split it into two. */
		*comma = '\0';
		col = strtoi(argv[1], &end, 0);
		if (*end || col >= GRID_COL_SIZE * 2)
			return EC_ERROR_PARAM1;
		row = strtoi(comma + 1, &end, 0);
		if (*end || row >= GRID_ROW_SIZE)
			return EC_ERROR_PARAM1;
	} else if (!strcasecmp(argv[1], "all")) {
		/* Usage 3 */
		col = -1;
		row = -1;
	} else {
		/* Usage 1 */
		if (argc != 2)
			return EC_ERROR_PARAM_COUNT;
		gcc = strtoi(argv[1], &end, 0);
		if (*end || gcc < 0 || gcc > UINT8_MAX)
			return EC_ERROR_PARAM1;
		demo = 0;
		for (i = 0; i < rgbkbd_count; i++) {
			ctx = &rgbkbds[i];
			ctx->cfg->drv->set_gcc(ctx, gcc);
		}
		return EC_SUCCESS;
	}

	if (argc != 5)
		return EC_ERROR_PARAM_COUNT;

	val = strtoi(argv[2], &end, 0);
	if (*end || val < 0 || val > UINT8_MAX)
		return EC_ERROR_PARAM2;
	color.r = val;
	val = strtoi(argv[3], &end, 0);
	if (*end || val < 0 || val > UINT8_MAX)
		return EC_ERROR_PARAM3;
	color.g = val;
	val = strtoi(argv[4], &end, 0);
	if (*end || val < 0 || val > UINT8_MAX)
		return EC_ERROR_PARAM4;
	color.b = val;

	demo = 0;
	if (row < 0 && col < 0) {
		/* Usage 3 */
		for (i = 0; i < rgbkbd_count; i++) {
			for (j = 0; j < GRID_SIZE; j++)
				grid[i][j] = color;
		}
		rgbkbd_sync();
	} else if (row < 0) {
		/* Usage 2: all LEDs of a column */
		ccprintf("Set column %d to 0x%02x%02x%02x\n", col,
			 color.r, color.g, color.b);
		for (i = 0; i < GRID_ROW_SIZE; i++)
			rgbkbd_set_color_single(color, i, col);
	} else if (col < 0) {
		/* Usage 2: all LEDs of a row. */
		ccprintf("Set row %d to 0x%02x%02x%02x\n", row,
			 color.r, color.g, color.b);
		for (i = 0; i < GRID_COL_SIZE * 2; i++)
			rgbkbd_set_color_single(color, row, i);
	} else {
		/* Usage 2 */
		ccprintf("Set (%d,%d) to 0x%02x%02x%02x\n", col, row,
			 color.r, color.g, color.b);
		rgbkbd_set_color_single(color, row, col);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rgbk, cc_rgbk,
			"\n"
			"1. rgbk <global-brightness>\n"
			"2. rgbk <col,row> <r-bright> <g-bright> <b-bright>\n"
			"3. rgbk all <r-bright> <g-bright> <b-bright>\n"
			"4. rgbk\n",
			"Set color of RGB keyboard"
			);
