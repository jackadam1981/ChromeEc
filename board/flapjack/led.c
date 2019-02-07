/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery LED control for Flapjack board.
 */

#include "battery.h"
#include "charge_state.h"
#include "console.h"
#include "driver/charger/rt946x.h"
#include "hooks.h"
#include "led_common.h"
#include "util.h"

/* Define this to enable led command and debug LED */
#undef DEBUG_LED

const enum ec_led_id supported_led_ids[] = { EC_LED_ID_BATTERY_LED };

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

#define LED_OFF		MT6370_LED_ID_OFF
#define LED_RED		MT6370_LED_ID1
#define LED_GRN		MT6370_LED_ID2
#define LED_BLU		MT6370_LED_ID3

#define LED_MASK_OFF	0
#define LED_MASK_RED	MT6370_MASK_RGB_ISNK1DIM_EN
#define LED_MASK_GRN	MT6370_MASK_RGB_ISNK2DIM_EN
#define LED_MASK_BLU	MT6370_MASK_RGB_ISNK3DIM_EN

static enum charge_state chstate;
const static enum mt6370_led_dim_mode  dim = MT6370_LED_DIM_MODE_REGISTER;

static void led_set_battery(void)
{
	static enum charge_state prev = PWR_STATE_UNCHANGE;
	static int battery_second;

	battery_second++;
#ifndef DEBUG_LED
	chstate = charge_get_state();
#endif
	if (chstate == prev)
		return;
	prev = chstate;

	mt6370_led_set_color(LED_MASK_OFF);
	switch (chstate) {
	case PWR_STATE_CHARGE:
		mt6370_led_set_color(LED_MASK_RED|LED_MASK_GRN);
		mt6370_led_set_dim_mode(LED_RED, dim);
		mt6370_led_set_dim_mode(LED_GRN, dim);
		mt6370_led_set_brightness(LED_RED, 0xF5);
		mt6370_led_set_brightness(LED_GRN, 0xF1);
		break;
	case PWR_STATE_DISCHARGE:
		break;
	case PWR_STATE_ERROR:
		mt6370_led_set_color(LED_MASK_RED);
		mt6370_led_set_dim_mode(LED_RED, dim);
		mt6370_led_set_brightness(LED_RED, 0xFF);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		mt6370_led_set_color(LED_MASK_RED|LED_MASK_GRN|LED_MASK_BLU);
		mt6370_led_set_dim_mode(LED_RED, dim);
		mt6370_led_set_dim_mode(LED_GRN, dim);
		mt6370_led_set_dim_mode(LED_BLU, dim);
		mt6370_led_set_brightness(LED_RED, 0xFF);
		mt6370_led_set_brightness(LED_GRN, 0xFF);
		mt6370_led_set_brightness(LED_BLU, 0xFF);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	if (led_id != EC_LED_ID_BATTERY_LED)
		return;

	brightness_range[EC_LED_COLOR_GREEN] = MT6370_LED_BRIGHTNESS_MAX;
	brightness_range[EC_LED_COLOR_RED] = MT6370_LED_BRIGHTNESS_MAX;
	brightness_range[EC_LED_COLOR_BLUE] = MT6370_LED_BRIGHTNESS_MAX;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (led_id != EC_LED_ID_BATTERY_LED)
		return EC_ERROR_INVAL;

	mt6370_led_set_brightness(LED_GRN, brightness[EC_LED_COLOR_GREEN]);
	mt6370_led_set_brightness(LED_RED, brightness[EC_LED_COLOR_RED]);
	mt6370_led_set_brightness(LED_BLU, brightness[EC_LED_COLOR_BLUE]);
	return EC_SUCCESS;
}

/* Called by hook task every 1 sec */
static void led_second(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		led_set_battery();
}
DECLARE_HOOK(HOOK_SECOND, led_second, HOOK_PRIO_DEFAULT);

#ifdef DEBUG_LED
static int command_led(int argc, char **argv)
{
	char *e;
	chstate = strtoi(argv[1], &e, 10);
	if (*e)
		return EC_ERROR_PARAM1;
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(led, command_led, NULL, NULL);
#endif
