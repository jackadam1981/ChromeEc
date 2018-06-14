/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Power and battery LED control for Phaser
 */

#include "ec_commands.h"
#include "gpio.h"
#include "led_common.h"
#include "led_states.h"

#define LED_OFF_LVL	1
#define LED_ON_LVL	0

/* TODO: fill in phaser power LED (b/110084784)*/
/* TODO: fill in phaser battery LED (b/110086152)*/

const struct led_descriptor
			led_bat_state_table[LED_NUM_STATES][LED_NUM_PHASES] = {
};

const enum ec_led_id supported_led_ids[] = { };

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

void led_set_color_battery(enum ec_led_colors color)
{
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	return EC_SUCCESS;
}

