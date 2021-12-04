/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "keyboard_scan.h"
#include "rgb_keyboard.h"
#include "spi.h"
#include "timer.h"

/* Keyboard scan setting */
__override struct keyboard_scan_config keyscan_config = {
	/* Increase from 50 us, because KSO_02 passes through the H1. */
	.output_settle_us = 80,
	/* Other values should be the same as the default configuration. */
	.debounce_down_us = 9 * MSEC,
	.debounce_up_us = 30 * MSEC,
	.scan_period_us = 3 * MSEC,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = 100 * MSEC,
	.actual_key_mask = {
		0x14, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xfe, 0x55, 0xfa, 0xca  /* full set */
	},
};

static const struct ec_response_keybd_config keybd1 = {
	.num_top_row_keys = 13,
	.action_keys = {
		TK_BACK,		/* T1 */
		TK_REFRESH,		/* T2 */
		TK_FULLSCREEN,		/* T3 */
		TK_OVERVIEW,		/* T4 */
		TK_SNAPSHOT,		/* T5 */
		TK_BRIGHTNESS_DOWN,	/* T6 */
		TK_BRIGHTNESS_UP,	/* T7 */
		TK_KBD_BKLIGHT_TOGGLE,	/* T8 */
		TK_PLAY_PAUSE,		/* T9 */
		TK_MICMUTE,		/* T10 */
		TK_VOL_MUTE,		/* T11 */
		TK_VOL_DOWN,		/* T12 */
		TK_VOL_UP,		/* T13 */
	},
	.capabilities = KEYBD_CAP_SCRNLOCK_KEY,
};

extern struct rgbkbd_drv is31fl3743b_drv;

struct rgbkbd rgbkbds[] = {
	[0] = {
		.cfg = &(const struct rgbkbd_cfg) {
			.drv = &is31fl3743b_drv,
			.col_len = 11,
			.row_len = 6,
			.spi = SPI_RGB0_DEVICE_ID,
		},
	},
	[1] = {
		.cfg = &(const struct rgbkbd_cfg) {
			.drv = &is31fl3743b_drv,
			.col_len = 11,
			.row_len = 6,
			.spi = SPI_RGB1_DEVICE_ID,
		},
	},
};
const uint8_t rgbkbd_count = ARRAY_SIZE(rgbkbds);

__override const struct ec_response_keybd_config *
board_vivaldi_keybd_config(void)
{
	return &keybd1;
}

void board_rgb_keyboard_init(void)
{
	/* Enable SPI for RGB matrix. */
	gpio_config_module(MODULE_SPI_CONTROLLER, 1);
	spi_enable(&spi_devices[SPI_RGB0_DEVICE_ID], 1);
	spi_enable(&spi_devices[SPI_RGB1_DEVICE_ID], 1);
}
DECLARE_HOOK(HOOK_INIT, board_rgb_keyboard_init, HOOK_PRIO_INIT_SPI - 1);
