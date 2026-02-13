/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "cros_cbi.h"
#include "gpio.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_config.h"
#include "keyboard_customization.h"
#include "keyboard_protocol.h"
#include "keyboard_raw.h"

#include <zephyr/drivers/gpio.h>

__override int board_alt(const uint8_t *state)
{
	uint32_t val = 0;
	cros_cbi_get_fw_config(FW_KEYBOARD, &val);

	/* US */
	if (val == 0) {
		if (state[KEYBOARD_COL_RIGHT_ALT] !=
			    KEYBOARD_ROW_TO_MASK(KEYBOARD_ROW_RIGHT_ALT) &&
		    state[KEYBOARD_COL_LEFT_ALT] !=
			    KEYBOARD_ROW_TO_MASK(KEYBOARD_ROW_LEFT_ALT))
			return 0;
	}
	/* JP */
	else if (val == 1) {
		if (state[KEYBOARD_COL_RIGHT_ALT_JP] !=
			    KEYBOARD_ROW_TO_MASK(KEYBOARD_ROW_RIGHT_ALT_JP) &&
		    state[KEYBOARD_COL_LEFT_ALT] !=
			    KEYBOARD_ROW_TO_MASK(KEYBOARD_ROW_LEFT_ALT))
			return 0;
	}
	return 0;
}
