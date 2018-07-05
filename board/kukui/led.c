/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery LED control for Kukui board.
 *TODO(b:80160408): Implement mt6370 led driver.
 */

#include "battery.h"
#include "charge_state.h"
#include "driver/charger/rt946x.h"
#include "hooks.h"
#include "led_common.h"

const enum ec_led_id supported_led_ids[] = { EC_LED_ID_BATTERY_LED };

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

static void kukui_led_set_battery(void)
{
	static int battery_second;
	uint32_t chflags = charge_get_flags();

	battery_second++;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		mt6370_led_set_color(LED_BLUE);
		break;
	case PWR_STATE_DISCHARGE:
		if (charge_get_percent() < 3)
			mt6370_led_set_color((battery_second & 1)
					? LED_OFF : LED_BLUE);
		else if (charge_get_percent() < 10)
			mt6370_led_set_color((battery_second & 3)
					? LED_OFF : LED_BLUE);
		else if (charge_get_percent() >= BATTERY_LEVEL_NEAR_FULL &&
		    (chflags & CHARGE_FLAG_EXTERNAL_POWER))
			mt6370_led_set_color(LED_GREEN);
		else
			mt6370_led_set_color(LED_OFF);
		break;
	case PWR_STATE_ERROR:
		mt6370_led_set_color(LED_RED);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		mt6370_led_set_color(LED_GREEN);
		break;
	case PWR_STATE_IDLE: /* External power connected in IDLE. */
		if (chflags & CHARGE_FLAG_FORCE_IDLE)
			mt6370_led_set_color(
				(battery_second & 0x2) ? LED_GREEN : LED_BLUE);
		else
			mt6370_led_set_color(LED_GREEN);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		brightness_range[EC_LED_COLOR_RED] = 7;
		brightness_range[EC_LED_COLOR_GREEN] = 7;
		brightness_range[EC_LED_COLOR_BLUE] = 7;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		mt6370_led_set_brightness(brightness);
		return EC_SUCCESS;
	}
	return EC_ERROR_INVAL;
}

/* Called by hook task every 1 sec */
static void led_second(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		kukui_led_set_battery();
}
DECLARE_HOOK(HOOK_SECOND, led_second, HOOK_PRIO_DEFAULT);
