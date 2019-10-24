/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* BC-Link master */

#include "bc.h"
#include "common.h"
#include "config.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_config.h"
#include "keyboard_raw.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define BC_ID 1

/*
 * Wilco KB scancodes, see https://docs.google.com/spreadsheets/d/
 *          1WwJZpwaxdZFdfDLItfCaeUEstqJAQJ-CTeREkRvsJ70/edit#gid=0
 */
struct {
	uint8_t row;
	uint8_t col;
	uint16_t sc1;
	uint16_t sc2;
} keys[] = {
	{ 0x0, 0 , 0x22, 0x34 },
	{ 0x0, 2 , 0x3e, 0x0c },
	{ 0x0, 3, 0x01, 0x0e }, /* ESC key, check docs */
	{ 0x0, 8 , 0x38, 0x11 },
	{ 0x0, 9 , 0xe048, 0xe075 },
	{ 0x0, 12 , 0x3f, 0x3 },
	{ 0x0, 13 , 0x28, 0x52 },
	{ 0x0, 15 , 0x40, 0x0b },
	{ 0x0, 16 , 0x23, 0x33 },
	{ 0x1, 0 , 0x14, 0x2c },
	{ 0x1, 1 , 0x3a, 0x58 },
	{ 0x1, 2 , 0x3d, 0x4 },
	{ 0x1, 3 , 0x0f, 0x0d },
	{ 0x1, 6 , 0x2a, 0x12 },
	{ 0x1, 12 , 0x0e, 0x66 },
	{ 0x1, 13 , 0x1a, 0x54 },
	{ 0x1, 14 , 0x41, 0x83 },
	{ 0x1, 15 , 0x1b, 0x5b },
	{ 0x1, 16 , 0x15, 0x35 },
	{ 0x2, 0 , 0x13, 0x2d },
	{ 0x2, 1 , 0x11, 0x1d },
	{ 0x2, 2 , 0x12, 0x24 },
	{ 0x2, 3 , 0x10, 0x15 },
	{ 0x2, 4 , 0xe049, 0xe07d },
	{ 0x2, 13 , 0x19, 0x4d },
	{ 0x2, 14 , 0x18, 0x44 },
	{ 0x2, 15 , 0x17, 0x43 },
	{ 0x2, 16 , 0x16, 0x3c },
	{ 0x3, 0 , 0x6, 0x2e },
	{ 0x3, 1 , 0x3b, 0x5 },
	{ 0x3, 2 , 0x3c, 0x6 },
	{ 0x3, 3 , 0x29, 0x0e },
	{ 0x3, 7 , 0x1d, 0x14 },
	{ 0x3, 10 , 0xe052, 0xe070 },
	{ 0x3, 11 , 0xe053, 0xe071 },
	{ 0x3, 12 , 0x43, 0x1 },
	{ 0x3, 13 , 0x0c, 0x4e },
	{ 0x3, 14 , 0x42, 0x0a },
	{ 0x3, 15 , 0x0d, 0x55 },
	{ 0x3, 16 , 0x7, 0x36 },
	{ 0x4, 0 , 0x21, 0x2b },
	{ 0x4, 1 , 0x1f, 0x1b },
	{ 0x4, 2 , 0x20, 0x23 },
	{ 0x4, 3 , 0x1e, 0x1c },
	{ 0x4, 4 , 0xe051, 0xe07a },
	/* { 0x4, 0x5 , 0x, 0x  - Fn key */
	{ 0x4, 12 , 0x2b, 0x5d },
	{ 0x4, 13 , 0x27, 0x4c },
	{ 0x4, 14 , 0x26, 0x4b },
	{ 0x4, 15 , 0x25, 0x43 },
	{ 0x4, 16 , 0x24, 0x3b },
	{ 0x5, 0 , 0x5, 0x25 },
	{ 0x5, 1 , 0x3, 0x1e },
	{ 0x5, 2 , 0x4, 0x26 },
	{ 0x5, 3 , 0x2, 0x16 },
	/* { 0x5, 0x4 , 0x, 0x - Win key */
	{ 0x5, 10 , 0x58, 0x7 },
	{ 0x5, 11 , 0x57, 0x78 },
	{ 0x5, 12 , 0x44, 0x9 },
	{ 0x5, 13 , 0x0b, 0x45 },
	{ 0x5, 14 , 0x0a, 0x46 },
	{ 0x5, 15 , 0x9, 0x3e },
	{ 0x5, 16 , 0x8, 0x3d },
	{ 0x6, 0 , 0x2f, 0x2a },
	{ 0x6, 1 , 0x2d, 0x22 },
	{ 0x6, 2 , 0x2e, 0x21 },
	{ 0x6, 3 , 0x2c, 0x1a },
	{ 0x6, 6 , 0x36, 0x59 },
	{ 0x6, 7 , 0xe01d, 0xe014 },
	{ 0x6, 12 , 0x1c, 0x5a },
	{ 0x6, 14 , 0x34, 0x49 },
	{ 0x6, 15 , 0x33, 0x41 },
	{ 0x6, 16 , 0x32, 0x3a },
	{ 0x7, 0 , 0x30, 0x32 },
	{ 0x7, 3 , 0xe037, 0xe07c },
	{ 0x7, 8 , 0xe038, 0xe011 },
	{ 0x7, 9 , 0xe04b, 0xe06b },
	{ 0x7, 10 , 0xe04d, 0xe074 },
	{ 0x7, 11 , 0xe050, 0xe072 },
	{ 0x7, 12 , 0x39, 0x29 },
	{ 0x7, 13 , 0x35, 0x4a },
	{ 0x7, 16 , 0x31, 0x31 },
};

static void scantable_init(void)
{
	int i, j;

	for (j = 0; j < KEYBOARD_COLS_MAX; ++j)
		for (i = 0; i < KEYBOARD_ROWS; ++i)
			scancode_set2[j][i] = 0;

	for (i = 0; i < ARRAY_SIZE(keys); ++i)
		scancode_set2[keys[i].col][keys[i].row] = keys[i].sc2;
}

static void bc_wait_busy(void)
{
	while (MEC17XX_BC_STATUS(BC_ID) & MEC17XX_BC_STATUS_BUSY);
}

void bc_write(uint8_t addr, uint8_t data)
{
	bc_wait_busy();
	MEC17XX_BC_ADDR(BC_ID) = addr;
	MEC17XX_BC_DATA(BC_ID) = data;
	bc_wait_busy();
}

#if 0
static uint8_t bc_read(uint8_t addr)
{
	bc_wait_busy();
	MEC17XX_BC_ADDR(BC_ID) = addr;
	MEC17XX_BC_DATA(BC_ID);
	bc_wait_busy();
	return MEC17XX_BC_DATA(BC_ID);
}
#endif

void keyboard_raw_init(void)
{
	scantable_init();

	ccprintf("Initializing BC-Link %x\n", MEC17XX_BC_STATUS(BC_ID));

	gpio_config_module(MODULE_I2C, 1);

	/* Initialize BC inteface */
	MEC17XX_BC_STATUS(BC_ID) = MEC17XX_BC_STATUS_RESET;
	while (!(MEC17XX_BC_STATUS(BC_ID) & MEC17XX_BC_STATUS_BUSY));
	MEC17XX_BC_CLK_SEL(BC_ID) = 15;
	MEC17XX_BC_STATUS(BC_ID) = 0;
	bc_wait_busy();

	/* Reset  I/O expander */
	bc_write(0xd0, 0x40);
	bc_write(0xd0, 0);

	/* gpio0 / 1 / 21 / 22 / 23 are KSO */
	bc_write(0x0a, 0x40);
	bc_write(0x0b, 0x40);
	bc_write(0x1b, 0x40);
	bc_write(0x1c, 0x40);
	bc_write(0x1d, 0x40);

	/* Assert interrupt on any KSI fall */
	bc_write(0x42, 0xff);
	bc_write(0x43, 0xff);
	bc_write(0xfa, 0x12);
	bc_write(0xfb, 0x08);

	/* Drive all KSO low */
	// bc_write(0x40, 0x20);

	ccprintf("BC-Link init done\n");
}

void keyboard_raw_task_start(void)
{
	// gpio_enable_interrupt(GPIO_BC_INT_L);
	// bc_write(0x42, 0xff);
}

test_mockable void keyboard_raw_drive_column(int out)
{
#if 0
	uint8_t reg;

	switch (out) {
	case KEYBOARD_COLUMN_ALL:
		reg = 0x20;
		break;
	case KEYBOARD_COLUMN_NONE:
		reg = 0;
		break;
	default:
		reg = out;
		/* KSI[7:10] don't exist on-chip */
		if (out > 6)
			reg += 4;
		break;
	}

	bc_write(0x40, reg);
#endif
}

test_mockable int keyboard_raw_read_rows(void)
{
#if 0
	return ~bc_read(0x41) & 0xff;
#else
	return 0;
#endif
}

void keyboard_raw_enable_interrupt(int enable)
{
#if 0
	ccprintf("keyboard_raw_enable_interrupt(%d)\n", enable);
	if (enable) {
		gpio_clear_pending_interrupt(GPIO_BC_INT_L);
		gpio_enable_interrupt(GPIO_BC_INT_L);
		bc_write(0x42, 0xff);
	} else {
		gpio_disable_interrupt(GPIO_BC_INT_L);
	}
#endif
}

void bc_link_interrupt(void)
{
#if 0
	ccprintf("@");
	bc_write(0x42, 0xff);
	task_wake(TASK_ID_KEYSCAN);
#endif
}

#if 0
static void bc_test(void)
{
	int i;
	uint8_t read;
	uint8_t write;
	// ccprintf("TICK: %x %x\n", bc_read(0x41), MEC17XX_BC_STATUS(BC_ID));

	bc_write(0x40, 0x20);
	udelay(50);
	if (bc_read(0x41) == 0xff)
		return;

	for (i = 0; i < 0x16; ++i) {
		if (i == 7) {
			i = 0xa;
			continue;
		}
		bc_write(0x40, i);
		udelay(50);
		read = bc_read(0x41);
		if (read != 0xff) {
			ccprintf("WR 0x%02x GOT 0x%02x 0x%02x TR %d %d\n",
				 write, ~read, i > 0xa ? i - 4 : i,
				 __builtin_ctz(~read),
				 i > 0xa ? i - 4 : i);
		}
		udelay(50);
	}

	bc_write(0x40, 0x1f);
	udelay(50);
}
DECLARE_HOOK(HOOK_TICK, bc_test, HOOK_PRIO_DEFAULT);
#endif
