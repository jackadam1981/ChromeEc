/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/input/input.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#include <dt-bindings/vivaldi_kbd.h>
#include <ec_commands.h>
#include <host_command.h>
#include <keyboard_8042_sharedlib.h>
#include <keyboard_scan.h>

static struct {
	uint16_t codes[KEYBOARD_ROWS][KEYBOARD_COLS_MAX];
	int call_count;
} set2_test;

void set_scancode_set2(uint8_t row, uint8_t col, uint16_t val)
{
	set2_test.codes[row][col] = val;
	set2_test.call_count++;
}

static struct {
	uint16_t row;
	uint16_t col;
	int call_count;
} vol_up_key;

void set_vol_up_key(uint8_t row, uint8_t col)
{
	vol_up_key.row = row;
	vol_up_key.col = col;
	vol_up_key.call_count++;
}

struct keyboard_scan_config keyscan_config;

ZTEST(vivaldi_kbd, test_matching_codes)
{
	/* Ensure that devicetree binding codes are in sync with the common ones
	 */
	zassert_equal(TK_ABSENT, VIVALDI_TK_ABSENT);
	zassert_equal(TK_BACK, VIVALDI_TK_BACK);
	zassert_equal(TK_FORWARD, VIVALDI_TK_FORWARD);
	zassert_equal(TK_REFRESH, VIVALDI_TK_REFRESH);
	zassert_equal(TK_FULLSCREEN, VIVALDI_TK_FULLSCREEN);
	zassert_equal(TK_OVERVIEW, VIVALDI_TK_OVERVIEW);
	zassert_equal(TK_BRIGHTNESS_DOWN, VIVALDI_TK_BRIGHTNESS_DOWN);
	zassert_equal(TK_BRIGHTNESS_UP, VIVALDI_TK_BRIGHTNESS_UP);
	zassert_equal(TK_VOL_MUTE, VIVALDI_TK_VOL_MUTE);
	zassert_equal(TK_VOL_DOWN, VIVALDI_TK_VOL_DOWN);
	zassert_equal(TK_VOL_UP, VIVALDI_TK_VOL_UP);
	zassert_equal(TK_SNAPSHOT, VIVALDI_TK_SNAPSHOT);
	zassert_equal(TK_PRIVACY_SCRN_TOGGLE, VIVALDI_TK_PRIVACY_SCRN_TOGGLE);
	zassert_equal(TK_KBD_BKLIGHT_DOWN, VIVALDI_TK_KBD_BKLIGHT_DOWN);
	zassert_equal(TK_KBD_BKLIGHT_UP, VIVALDI_TK_KBD_BKLIGHT_UP);
	zassert_equal(TK_PLAY_PAUSE, VIVALDI_TK_PLAY_PAUSE);
	zassert_equal(TK_NEXT_TRACK, VIVALDI_TK_NEXT_TRACK);
	zassert_equal(TK_PREV_TRACK, VIVALDI_TK_PREV_TRACK);
	zassert_equal(TK_KBD_BKLIGHT_TOGGLE, VIVALDI_TK_KBD_BKLIGHT_TOGGLE);
	zassert_equal(TK_MICMUTE, VIVALDI_TK_MICMUTE);
	zassert_equal(TK_MENU, VIVALDI_TK_MENU);

	zassert_equal(KEYBD_CAP_FUNCTION_KEYS, VIVALDI_KEYBD_CAP_FUNCTION_KEYS);
	zassert_equal(KEYBD_CAP_NUMERIC_KEYPAD,
		      VIVALDI_KEYBD_CAP_NUMERIC_KEYPAD);
	zassert_equal(KEYBD_CAP_SCRNLOCK_KEY, VIVALDI_KEYBD_CAP_SCRNLOCK_KEY);
}

ZTEST(vivaldi_kbd, test_get_vivaldi_keybd_config)
{
	enum ec_status ret;
	struct ec_response_keybd_config resp;
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_RESPONSE(EC_CMD_GET_KEYBD_CONFIG, 0, resp);

	ret = host_command_process(&args);

	zassert_equal(ret, EC_RES_SUCCESS);
	zassert_equal(resp.num_top_row_keys, 10);
	zassert_equal(resp.action_keys[0], TK_BACK);
	zassert_equal(resp.action_keys[1], TK_FORWARD);
	zassert_equal(resp.action_keys[2], TK_REFRESH);
	zassert_equal(resp.action_keys[3], TK_FULLSCREEN);
	zassert_equal(resp.action_keys[4], TK_OVERVIEW);
	zassert_equal(resp.action_keys[5], TK_BRIGHTNESS_DOWN);
	zassert_equal(resp.action_keys[6], TK_BRIGHTNESS_UP);
	zassert_equal(resp.action_keys[7], TK_VOL_MUTE);
	zassert_equal(resp.action_keys[8], TK_VOL_DOWN);
	zassert_equal(resp.action_keys[9], TK_VOL_UP);
	zassert_equal(resp.capabilities, KEYBD_CAP_SCRNLOCK_KEY);
}

ZTEST(vivaldi_kbd, test_actual_key_mask)
{
	/* only 10 keys defined from the 10 vivaldi-codes entries */
	zassert_equal(keyscan_config.actual_key_mask[0], 0);
	zassert_equal(keyscan_config.actual_key_mask[1], 0);
	zassert_equal(keyscan_config.actual_key_mask[2],
		      BIT(0) | BIT(1) | BIT(2) | BIT(3));
	zassert_equal(keyscan_config.actual_key_mask[3], 0);
	zassert_equal(keyscan_config.actual_key_mask[4],
		      BIT(0) | BIT(1) | BIT(2) | BIT(3));
	zassert_equal(keyscan_config.actual_key_mask[5], 0);
	zassert_equal(keyscan_config.actual_key_mask[6], 0);
	zassert_equal(keyscan_config.actual_key_mask[7], 0);
	zassert_equal(keyscan_config.actual_key_mask[8], 0);
	zassert_equal(keyscan_config.actual_key_mask[9], BIT(1) | BIT(2));
	zassert_equal(keyscan_config.actual_key_mask[10], 0);
	zassert_equal(keyscan_config.actual_key_mask[11], 0);
}

ZTEST(vivaldi_kbd, test_set2_codes)
{
	zassert_equal(set2_test.codes[0][2], SCANCODE_BACK);
	zassert_equal(set2_test.codes[3][2], SCANCODE_FORWARD);
	zassert_equal(set2_test.codes[2][2], SCANCODE_REFRESH);
	zassert_equal(set2_test.codes[1][2], SCANCODE_FULLSCREEN);
	zassert_equal(set2_test.codes[3][4], SCANCODE_OVERVIEW);
	zassert_equal(set2_test.codes[2][4], SCANCODE_BRIGHTNESS_DOWN);
	zassert_equal(set2_test.codes[1][4], SCANCODE_BRIGHTNESS_UP);
	zassert_equal(set2_test.codes[2][9], SCANCODE_VOLUME_MUTE);
	zassert_equal(set2_test.codes[1][9], SCANCODE_VOLUME_DOWN);
	zassert_equal(set2_test.codes[0][4], SCANCODE_VOLUME_UP);
	zassert_equal(set2_test.call_count, 10);
}

ZTEST(vivaldi_kbd, test_vol_up_key)
{
	zassert_equal(vol_up_key.row, 0);
	zassert_equal(vol_up_key.col, 4);
	zassert_equal(vol_up_key.call_count, 1);
}

ZTEST_SUITE(vivaldi_kbd, NULL, NULL, NULL, NULL, NULL);
