/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Modular keyboard console commands for Chrome EC */

/* All includes should be nested within this h file */
#include "keyboard_scan_console_cmd.h"

/*****************************************************************************/
/* Console commands */
static int command_keyboard_press(int argc, char **argv)
{
	if (argc == 1) {
		int i;

		ccputs("Simulated keys:\n");
		for (i = 0; i < get_num_col(); ++i) {
			int col = get_simulated_col(i);

			while (col) {
				const int row = __fls(col);

				col &= ~BIT(row);
				ccprintf("\t%d %d\n", i, row);
			}
		}

	} else if (argc == 3 || argc == 4) {
		int r, c, p;
		char *e;

		c = strtoi(argv[1], &e, 0);
		if (*e || c < 0 || c >= keyboard_cols)
			return EC_ERROR_PARAM1;

		r = strtoi(argv[2], &e, 0);
		if (*e || r < 0 || r >= KEYBOARD_ROWS)
			return EC_ERROR_PARAM2;

		if (argc == 3) {
			/* Simulate a press and release */
			simulate_key(r, c, true);
			simulate_key(r, c, false);
		} else {
			p = strtoi(argv[3], &e, 0);
			if (*e || p < 0 || p > 1)
				return EC_ERROR_PARAM3;

			simulate_key(r, c, !!p);
		}
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(kbpress, command_keyboard_press,
			"[col row [0 | 1]]",
			"Simulate keypress");
