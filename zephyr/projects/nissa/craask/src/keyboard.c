/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"
#include "gpio.h"
#include "soc_gpio.h"

static const struct ec_response_keybd_config craask_kb = {
	.num_top_row_keys = 10,
	.action_keys = {
		TK_BACK,		/* T1 */
		TK_REFRESH,		/* T2 */
		TK_FULLSCREEN,		/* T3 */
		TK_OVERVIEW,		/* T4 */
		TK_SNAPSHOT,		/* T5 */
		TK_BRIGHTNESS_DOWN,	/* T6 */
		TK_BRIGHTNESS_UP,	/* T7 */
		TK_VOL_MUTE,		/* T8 */
		TK_VOL_DOWN,		/* T9 */
		TK_VOL_UP,		/* T10 */
	},
	.capabilities = KEYBD_CAP_SCRNLOCK_KEY,
};

__override const struct ec_response_keybd_config *
board_vivaldi_keybd_config(void)
{
	return &craask_kb;
}

#define KBD_KS02_NODE DT_ALIAS(gpio_kbd_kso2)

/*
 * We have total 30 pins for keyboard connecter {-1, -1} mean
 * the N/A pin that don't consider it and reserve index 0 area
 * that we don't have pin 0.
 */
const int keyboard_factory_scan_pins[][2] = {
	{ -1, -1 }, { 0, 5 },	{ 1, 1 }, { 1, 0 },   { 0, 6 },	  { 0, 7 },
	{ -1, -1 }, { -1, -1 }, { 1, 4 }, { 1, 3 },   { -1, -1 }, { 1, 6 },
	{ 1, 7 },   { 3, 1 },	{ 2, 0 }, { 1, 5 },   { 2, 6 },	  { 2, 7 },
	{ 2, 1 },   { 2, 4 },	{ 2, 5 }, { 1, 2 },   { 2, 3 },	  { 2, 2 },
	{ 3, 0 },   { -1, -1 }, { 0, 4 }, { -1, -1 }, { 8, 2 },	  { -1, -1 },
	{ -1, -1 },
};
const int keyboard_factory_scan_pins_used =
	ARRAY_SIZE(keyboard_factory_scan_pins);

/**
 * Run keyboard factory testing, scan out KSO/KSI if any shorted.
 */
__override int keyboard_factory_test_scan(void)
{
	int i, j;
	int flags;
	uint16_t shorted = 0;

	/* Disable keyboard scan while testing */
	keyboard_scan_enable(0, KB_SCAN_DISABLE_LID_CLOSED);
	gpio_pin_get_config_dt(GPIO_DT_FROM_NODE(KBD_KS02_NODE), &flags);

	/* Set all of KSO/KSI pins to internal pull-up and input */
	keybaord_raw_config_alt(0);
	for (i = 0; i < keyboard_factory_scan_pins_used; ++i) {
		const struct device *dev =
			npcx_get_gpio_dev(keyboard_factory_scan_pins[i][0]);
		int pin = keyboard_factory_scan_pins[i][1];

		if (keyboard_factory_scan_pins[i][0] < 0)
			continue;

		gpio_pin_configure(dev, pin, GPIO_INPUT | GPIO_PULL_UP);
	}

	for (i = 0; i < keyboard_factory_scan_pins_used; ++i) {
		const struct device *dev =
			npcx_get_gpio_dev(keyboard_factory_scan_pins[i][0]);
		int pin = keyboard_factory_scan_pins[i][1];

		if (keyboard_factory_scan_pins[i][0] < 0)
			continue;

		gpio_pin_configure(dev, pin, GPIO_OUT_LOW);

		for (j = 0; j < keyboard_factory_scan_pins_used; ++j) {
			if (keyboard_factory_scan_pins[j][0] < 0 || i == j)
				continue;

			if (keyboard_raw_is_input_low(
				    keyboard_factory_scan_pins[j][0],
				    keyboard_factory_scan_pins[j][1])) {
				shorted = i << 8 | j;
				goto done;
			}
		}
		gpio_pin_configure(dev, pin, GPIO_INPUT | GPIO_PULL_UP);
	}
done:
	keybaord_raw_config_alt(1);
	gpio_pin_configure_dt(GPIO_DT_FROM_NODE(KBD_KS02_NODE), flags);
	keyboard_scan_enable(1, KB_SCAN_DISABLE_LID_CLOSED);

	return shorted;
}
