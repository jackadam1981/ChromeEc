/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_board_info.h"
#include "cros_cbi.h"
#include "ec_commands.h"
#include "gpio_it8xxx2.h"
#include "hooks.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_scan.h"

#include <zephyr/logging/log.h>

#include <drivers/vivaldi_kbd.h>

LOG_MODULE_REGISTER(nissa_keyboard, LOG_LEVEL_INF);

static bool key_bl = FW_KB_BL_NOT_PRESENT;
static bool key_numeric_pad = FW_KB_NUMERIC_PAD_ABSENT;

int8_t board_vivaldi_keybd_idx(void)
{
	if (key_bl == FW_KB_BL_NOT_PRESENT) {
		return DT_NODE_CHILD_IDX(DT_NODELABEL(kbd_config_1));
	} else {
		return DT_NODE_CHILD_IDX(DT_NODELABEL(kbd_config_0));
	}
}

/*
 * Keyboard function decided by FW config.
 */
test_export_static void keyboard_matrix_init(void)
{
	int ret;
	uint32_t val;

	ret = cros_cbi_get_fw_config(FW_KB_NUMERIC_PAD, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG_NUMERIC field %d",
			FW_KB_NUMERIC_PAD);
		return;
	}

	if (val == FW_KB_NUMERIC_PAD_PRESENT) {
		key_numeric_pad = FW_KB_NUMERIC_PAD_PRESENT;
	} else {
		key_numeric_pad = FW_KB_NUMERIC_PAD_ABSENT;
	}
}
DECLARE_HOOK(HOOK_INIT, keyboard_matrix_init, HOOK_PRIO_POST_FIRST);

/*
 * Keyboard function decided by FW config.
 */
test_export_static void kb_init(void)
{
	int ret;
	uint32_t val;

	ret = cros_cbi_get_fw_config(FW_KB_BL, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG_BL field %d", FW_KB_BL);
		return;
	}

	if (val == FW_KB_BL_PRESENT) {
		key_bl = FW_KB_BL_PRESENT;
	} else {
		key_bl = FW_KB_BL_NOT_PRESENT;
	}
}
DECLARE_HOOK(HOOK_INIT, kb_init, HOOK_PRIO_POST_FIRST);
