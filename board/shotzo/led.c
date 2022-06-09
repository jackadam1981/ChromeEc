/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Shotzo specific LED settings. */

#include "cbi_fw_config.h"
#include "charge_state.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "led_common.h"

#define POWER_LED_ON 0
#define POWER_LED_OFF 1

const enum ec_led_id supported_led_ids[] = {EC_LED_ID_POWER_LED};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_WHITE,
	LED_COLOR_COUNT  /* Number of colors, not a color itself */
};

static int led_set_color_power(enum led_color color)
{
	switch (color) {
	case LED_OFF:
		gpio_set_level(GPIO_PWR_LED_WHITE_L, POWER_LED_OFF);
		break;
	case LED_WHITE:
		gpio_set_level(GPIO_PWR_LED_WHITE_L, POWER_LED_ON);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	switch (led_id) {
	case EC_LED_ID_POWER_LED:
		brightness_range[EC_LED_COLOR_WHITE] = 1;
		break;
	default:
		break;
	}
}

static int led_set_color(enum ec_led_id led_id, enum led_color color)
{
	int rv;

	switch (led_id) {
	case EC_LED_ID_POWER_LED:
		rv = led_set_color_power(color);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return rv;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (brightness[EC_LED_COLOR_WHITE] != 0)
		led_set_color(led_id, LED_WHITE);
	else
		led_set_color(led_id, LED_OFF);

	return EC_SUCCESS;
}

static void led_set_power(void)
{
	static int power_tick;

	power_tick++;

	if (chipset_in_state(CHIPSET_STATE_ON))
		led_set_color_power(LED_WHITE);
	else if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND))
		led_set_color_power(
			(power_tick & 0x2) ? LED_WHITE : LED_OFF);
	else
		led_set_color_power(LED_OFF);
}

/* Called by hook task every TICK */
static void led_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED))
		led_set_power();
}
DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);
