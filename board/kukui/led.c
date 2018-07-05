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

enum led_color {
	LED_OFF = 0,
	LED_GREEN,
	LED_RED,
	LED_COLOR_COUNT  /* Number of colors, not a color itself */
};

static void bat_led_set_color(enum led_color color)
{
	switch (color) {
	case LED_OFF:
		rt946x_update_bits(MT6370_REG_RGBEN,
				   MT6370_MASK_RGB_ISNK_ALL_EN, 0);
		break;
	case LED_GREEN:
		rt946x_update_bits(MT6370_REG_RGBEN,
				   MT6370_MASK_RGB_ISNK_ALL_EN,
				   1 << MT6370_SHIFT_RGB_ISNK1DIM);
		break;
	case LED_RED:
		rt946x_update_bits(MT6370_REG_RGBEN,
				   MT6370_MASK_RGB_ISNK_ALL_EN,
				   1 << MT6370_SHIFT_RGB_ISNK2DIM);
		break;
	default:
		break;
	}
}

static void kukui_led_set_battery(void)
{
	static int battery_second;
	uint32_t chflags = charge_get_flags();

	battery_second++;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		/* Always indicate when charging, even in suspend. */
		bat_led_set_color(LED_RED);
		break;
	case PWR_STATE_DISCHARGE:
		if (charge_get_percent() <= 10)
			bat_led_set_color(
			   (battery_second & 0x4) ? LED_GREEN : LED_OFF);
		else
			bat_led_set_color(LED_OFF);
		break;
	case PWR_STATE_ERROR:
		bat_led_set_color((battery_second & 0x2) ?
				LED_GREEN : LED_OFF);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		bat_led_set_color(LED_GREEN);
		break;
	case PWR_STATE_IDLE: /* External power connected in IDLE. */
		if (chflags & CHARGE_FLAG_FORCE_IDLE)
			bat_led_set_color((battery_second & 0x4) ?
					LED_RED : LED_OFF);
		else
			bat_led_set_color(LED_GREEN);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		brightness_range[EC_LED_COLOR_GREEN] = 7;
		brightness_range[EC_LED_COLOR_RED] = 7;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		rt946x_update_bits(MT6370_REG_RGB1ISNK,
				   MT6370_MASK_RGBISNK_CURSEL,
				   brightness[EC_LED_COLOR_GREEN]
					   << MT6370_SHIFT_RGBISNK_CURSEL);
		rt946x_update_bits(MT6370_REG_RGB2ISNK,
				   MT6370_MASK_RGBISNK_CURSEL,
				   brightness[EC_LED_COLOR_RED]
					   << MT6370_SHIFT_RGBISNK_CURSEL);
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
