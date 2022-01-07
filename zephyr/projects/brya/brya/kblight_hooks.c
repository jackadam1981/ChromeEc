/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cbi.h"
#include "gpio.h"
#include "hooks.h"

/* Enable/Disable keyboard backlight gpio*/
static inline void kbd_backlite_enable(bool enable)
{
	enum gpio_signal kblight_gpio = GPIO_UNIMPLEMENTED;
	int value = -1;

	if (get_board_id() == 1) {
		kblight_gpio = GPIO_ID_1_EC_KB_BL_EN;
		value = enable ? 1 : 0;
	} else {
		kblight_gpio = GPIO_EC_KB_BL_EN_L;
		value = enable ? 0 : 1;
	}
	gpio_set_level(kblight_gpio, value);
}

/* Called on AP S3 -> S0 transition */
static void board_chipset_resume(void)
{
	/* Allow keyboard backlight to be enabled */

	kbd_backlite_enable(true);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_chipset_resume, HOOK_PRIO_DEFAULT);

/* Called on AP S0 -> S3 transition */
static void board_chipset_suspend(void)
{
	/* Turn off the keyboard backlight if it's on. */

	kbd_backlite_enable(false);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_chipset_suspend, HOOK_PRIO_DEFAULT);

/*
 * Explicitly apply the board ID 1 *gpio.inc settings to pins that
 * were reassigned on current boards.
 */

static void set_board_id_1_gpios(void)
{
	if (get_board_id() != 1)
		return;

	gpio_set_flags(GPIO_ID_1_EC_KB_BL_EN, GPIO_OUT_LOW);
}
DECLARE_HOOK(HOOK_INIT, set_board_id_1_gpios, HOOK_PRIO_FIRST);
