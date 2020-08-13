/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Standard Battery LED and Power LED control for madoo
 */

#include "gpio.h"
#include "hooks.h"
#include "battery.h"
#include "charge_state.h"
#include "chipset.h"
#include "led_common.h"
#include "util.h"
#include "lid_switch.h"


#define BAT_LED_ON 0
#define BAT_LED_OFF 1

#define POWER_LED_ON 0
#define POWER_LED_OFF 1


#define LED_TICKS_PER_CYCLE 10
#define LED_ON_TICKS 5


const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_BATTERY_LED, EC_LED_ID_POWER_LED};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_AMBER,
	LED_WHITE,
	LED_COLOR_COUNT  /* Number of colors, not a color itself */
};

static int bat_led_set_color(enum led_color color)
{
	switch (color) {
	case LED_OFF:
		gpio_set_level(GPIO_CHG_LED_AMBER_L, BAT_LED_OFF);
		gpio_set_level(GPIO_CHG_LED_WHITE_L, BAT_LED_OFF);
		break;
	case LED_AMBER:
		gpio_set_level(GPIO_CHG_LED_AMBER_L, BAT_LED_ON);
		gpio_set_level(GPIO_CHG_LED_WHITE_L, BAT_LED_OFF);
		break;
	case LED_WHITE:
		gpio_set_level(GPIO_CHG_LED_WHITE_L, BAT_LED_ON);
		gpio_set_level(GPIO_CHG_LED_AMBER_L, BAT_LED_OFF);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

static int pwr_led_set_color(enum led_color color)
{
	switch (color) {
	case LED_OFF:
		gpio_set_level(GPIO_PWR_LED_WHITE_L, POWER_LED_OFF);
		break;
	case LED_WHITE:
		gpio_set_level(GPIO_PWR_LED_WHITE_L,
			       lid_is_open() ? POWER_LED_ON : POWER_LED_OFF);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	switch (led_id) {
	case EC_LED_ID_BATTERY_LED:
		brightness_range[EC_LED_COLOR_AMBER] = 1;
		brightness_range[EC_LED_COLOR_WHITE] = 1;
		break;
	case EC_LED_ID_POWER_LED:
		brightness_range[EC_LED_COLOR_WHITE] = 1;
		break;
	default:
		/* ignore */
		break;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	switch (led_id) {
	case EC_LED_ID_BATTERY_LED:
		gpio_set_level(GPIO_CHG_LED_AMBER_L,
			       (brightness[EC_LED_COLOR_AMBER] != 0) ?
					BAT_LED_ON : BAT_LED_OFF);
		gpio_set_level(GPIO_CHG_LED_WHITE_L,
			       (brightness[EC_LED_COLOR_WHITE] != 0) ?
					BAT_LED_ON : BAT_LED_OFF);
		break;
	case EC_LED_ID_POWER_LED:
		gpio_set_level(GPIO_PWR_LED_WHITE_L,
			       (brightness[EC_LED_COLOR_WHITE] != 0) ?
					POWER_LED_ON : POWER_LED_OFF);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

#ifdef HAS_TASK_CHIPSET
static void madoo_led_shutdown(void)
{
	pwr_led_set_color(LED_OFF);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, madoo_led_shutdown, HOOK_PRIO_DEFAULT);
#endif

static void madoo_led_set_power(void)
{
	static int power_second;

	power_second++;

	if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
		pwr_led_set_color(LED_OFF);
	else if (chipset_in_state(CHIPSET_STATE_ON))
		pwr_led_set_color(LED_WHITE);
	else if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND))
		pwr_led_set_color((power_second %
					LED_TICKS_PER_CYCLE < LED_ON_TICKS)
					? LED_WHITE : LED_OFF);
}

static void madoo_led_set_battery(void)
{
	static int battery_second;

	battery_second++;

	/* BAT LED behavior:
	 * Same as the chromeos spec
	 * Green/Amber for CHARGE_FLAG_FORCE_IDLE
	 */
	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		bat_led_set_color(LED_AMBER);
		break;
	case PWR_STATE_DISCHARGE:
		if (charge_get_percent() < 10)
			bat_led_set_color((battery_second %
					LED_TICKS_PER_CYCLE < LED_ON_TICKS)
					? LED_WHITE : LED_OFF);
		else
			bat_led_set_color(LED_OFF);
		break;
	case PWR_STATE_ERROR:
		/* Delay 100ms for 0.5s blink */
		if (battery_second & 0x2)
			msleep(100);

		bat_led_set_color((battery_second & 2)
					? LED_AMBER : LED_OFF);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
			bat_led_set_color(LED_WHITE);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

/* Called by hook task every 200 ms */
static void led_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED))
		madoo_led_set_power();
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		madoo_led_set_battery();

}
DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);
