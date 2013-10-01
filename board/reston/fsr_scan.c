/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* FSR matrix scanning */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "key_mappings.h"
#include "task.h"
#include "timer.h"
#include "usb_hid.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_KEYSCAN, outstr)
#define CPRINTF(format, args...) cprintf(CC_KEYSCAN, format, ## args)

BUILD_ASSERT(ADC_CH_COUNT == KB_COLS);

/* delay to get the right after setting a row */
#define SETTLE_DELAY_US 50
/* delay between 2 scans */
#define SCAN_IDLE_DELAY_US 40000

/* Voltage threshold to decide if a key is pressed */
#define KEY_PRESS_ADC_THRESHOLD 2000 /* mV */

/* last values sampled on the matrix */
static int matrix_state[KB_ROWS][KB_COLS];

/* Matrix from previous scan */
static uint16_t prev_state[KB_ROWS];

static void fsr_drive_row(int row)
{
	int i;

	/* reset rows */
	for (i = 0; i < KB_ROWS; i++)
		gpio_set_level(GPIO_FSR_ROW1 + i, 1);

	/* set active row if any */
	if (row >= 0)
		gpio_set_level(GPIO_FSR_ROW1 + row, 0);
}

static void read_matrix(void)
{
	int row;

	/* read each row */
	for (row = 0; row < KB_ROWS; row++) {
		fsr_drive_row(row);
		/* wait for the colums to settle */
		udelay(SETTLE_DELAY_US);
		/* sample all column voltages */
		adc_read_all_channels(matrix_state[row]);
	}
	/* relax the matrix */
	fsr_drive_row(-1);
}

static void keyboard_state_changed(int row, int col, int is_pressed)
{
	int gpio_row = mappings[row][col].hap_row;
	int gpio_col = mappings[row][col].hap_col;

	CPRINTF("[%T KEY %d%c =%d]\n", 1 + row, 'A' + col, is_pressed);

	/* starts the haptic feedback on the key */
	if (is_pressed && gpio_row && gpio_col)
		trigger_feedback(gpio_row, gpio_col);
}

static int has_ghosting(const uint16_t *state)
{
	int r, r2;

	for (r = 0; r < KB_ROWS; r++) {
		if (!state[r])
			continue;

		for (r2 = r + 1; r2 < KB_ROWS; r2++) {
			/*
			 * A little bit of cleverness here.  Ghosting happens
			 * if 2 rows share at least 2 keys.  So we OR the
			 * rows together and then see if more than one bit
			 * is set.  x&(x-1) is non-zero only if x has more than
			 * one bit set.
			 */
			uint8_t common = state[r] & state[r2];

			if (common & (common - 1))
				return 1;
		}
	}

	return 0;
}

/* First key code of a modifier key */
#define MODIFIER_KEY_MIN 0xE0

static int check_keys_changed(uint16_t *state)
{
	int r, c;
	int any_change = 0;
	static uint16_t new_state[KB_ROWS];
	uint64_t report = 0;
	uint8_t modifiers = 0;

	/* convert ADC values to a simple bitmask */
	for (r = 0; r < KB_ROWS; r++) {
		new_state[r] = 0;
		for (c = 0; c < KB_COLS; c++)
			if (matrix_state[r][c] < KEY_PRESS_ADC_THRESHOLD) {
				uint8_t id = mappings[r][c].usb_id;
				new_state[r] |= 1 << c;
				if (id >= MODIFIER_KEY_MIN)
					modifiers |=
						1 << (id - MODIFIER_KEY_MIN);
				else
					report = (report << 8) | id;
			}
	}

	/* Ignore if so many keys are pressed that we're ghosting */
	if (has_ghosting(new_state))
		return 0;

	/* Check for changes between previous scan and this one */
	for (r = 0; r < KB_ROWS; r++) {
		int diff = new_state[r] ^ prev_state[r];

		if (!diff)
			continue;

		for (c = 0; c < KB_COLS; c++)
			if (diff & (1 << c)) {
				keyboard_state_changed(r, c,
					!!(new_state[r] & (1 << c)));
				any_change = 1;
			}

		prev_state[r] = new_state[r];
	}
	if (any_change)
		set_keyboard_report((report << 16) | modifiers);

	return any_change;
}

void fsr_scan_task(void)
{
	CPRINTF("Start scanning keyboard ...\n");

	while (1) {
		/* scan all the sensors in the matrix */
		read_matrix();
		/* find new key press or release and send the HID events */
		check_keys_changed(prev_state);

		usleep(SCAN_IDLE_DELAY_US);
	}
}

static int command_matrix(int argc, char **argv)
{
	int r, c;

	for (r = 0; r < KB_ROWS; r++) {
		for (c = 0; c < KB_COLS; c++)
			ccprintf("%5d ", matrix_state[r][c]);
		ccprintf("\n");
	}
	ccprintf("map: ");
	for (r = 0; r < KB_ROWS; r++)
		ccprintf("|%03x|", prev_state[r]);
	ccprintf("\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(matrix, command_matrix,
			NULL,
			"Print last keyboard matrix scan values",
			NULL);
