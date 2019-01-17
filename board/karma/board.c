/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "oz554.h"

enum panel_id_list {
	PANEL_LM_SSE2 = 0,
	PANEL_LM_SSK1,
	PANEL_COUNT,
	PANEL_UNKNOWN,
};

static int get_panel_id(void)
{
	int pin_status = 0;

	if (gpio_get_level(GPIO_PANEL_ID_0))
		pin_status |= 0x01;
	if (gpio_get_level(GPIO_PANEL_ID_1))
		pin_status |= 0x02;
	if (gpio_get_level(GPIO_PANEL_ID_2))
		pin_status |= 0x04;

	CPRINTS("Panel pin_status 0x%X", pin_status);

	switch (pin_status) {
	case 0x04:
		return PANEL_LM_SSE2;
	case 0x05:
		return PANEL_LM_SSK1;
	default:
		return PANEL_UNKNOWN;
	}
}

static void board_init_oz554(void)
{
	int panel_id = get_panel_id();

	CPRINTS("Panel id %d", panel_id);

	if (panel_id == PANEL_LM_SSK1)
		/* Reigster 0x02: Setting LED current: 55(mA) */
		change_oz554_setting(2, 0x55);

	gpio_enable_interrupt(GPIO_PANEL_BACKLIGHT_EN);
}
DECLARE_HOOK(HOOK_INIT, board_init_oz554, HOOK_PRIO_DEFAULT);
