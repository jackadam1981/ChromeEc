/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Power and battery LED control for Malefor
 */

#include "common.h"
#include "led_onoff_states.h"
#include "led_common.h"
#include "gpio.h"
#include "timer.h"
#include "task.h"
#include "stdbool.h"
#include "i2c.h"
#include "hooks.h"
#include "charge_state.h"
#include "lid_switch.h"
#include "extpower.h"
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)

#define LED_OFF_LVL	1
#define LED_ON_LVL	0

const int led_charge_lvl_1 = 5;

const int led_charge_lvl_2 = 97;

struct led_descriptor led_bat_state_table[LED_NUM_STATES][LED_NUM_PHASES] = {
	[STATE_CHARGING_LVL_1]	     = {{EC_LED_COLOR_RED, LED_INDEFINITE} },
	[STATE_CHARGING_LVL_2]	     = {{EC_LED_COLOR_AMBER, LED_INDEFINITE} },
	[STATE_CHARGING_FULL_CHARGE] = {{EC_LED_COLOR_GREEN, LED_INDEFINITE} },
	[STATE_DISCHARGE_S0]	     = {{LED_OFF,            LED_INDEFINITE} },
	[STATE_DISCHARGE_S3]	     = {{LED_OFF,            LED_INDEFINITE} },
	[STATE_DISCHARGE_S5]         = {{LED_OFF,            LED_INDEFINITE} },
	[STATE_BATTERY_ERROR]        = {{EC_LED_COLOR_RED,   1 * LED_ONE_SEC},
					{LED_OFF,	     1 * LED_ONE_SEC} },
	[STATE_FACTORY_TEST]         = {{EC_LED_COLOR_RED,   2 * LED_ONE_SEC},
					{EC_LED_COLOR_GREEN, 2 * LED_ONE_SEC} },
};

const struct led_descriptor
		led_pwr_state_table[PWR_LED_NUM_STATES][LED_NUM_PHASES] = {
	[PWR_LED_STATE_ON]            = {{EC_LED_COLOR_WHITE, LED_INDEFINITE} },
	[PWR_LED_STATE_SUSPEND_AC]    = {{EC_LED_COLOR_WHITE, 1 * LED_ONE_SEC},
					 {LED_OFF,         3 * LED_ONE_SEC} },
	[PWR_LED_STATE_SUSPEND_NO_AC] = {{EC_LED_COLOR_WHITE, 1 * LED_ONE_SEC},
					 {LED_OFF,         3 * LED_ONE_SEC} },
	[PWR_LED_STATE_OFF]           = {{LED_OFF,            LED_INDEFINITE} },
};

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_BATTERY_LED,
	EC_LED_ID_POWER_LED
};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

void led_set_color_power(enum ec_led_colors color)
{
	if (color == EC_LED_COLOR_WHITE)
		gpio_set_level(GPIO_LED_3_L, LED_ON_LVL);
	else
		/* LED_OFF and unsupported colors */
		gpio_set_level(GPIO_LED_3_L, LED_OFF_LVL);
}

void led_set_color_battery(enum ec_led_colors color)
{
	switch (color) {
	case EC_LED_COLOR_AMBER:
		gpio_set_level(GPIO_LED_1_L, LED_ON_LVL);
		gpio_set_level(GPIO_LED_2_L, LED_ON_LVL);
		break;
	case EC_LED_COLOR_RED:
		gpio_set_level(GPIO_LED_1_L, LED_OFF_LVL);
		gpio_set_level(GPIO_LED_2_L, LED_ON_LVL);
		break;
	case EC_LED_COLOR_GREEN:
		gpio_set_level(GPIO_LED_1_L, LED_ON_LVL);
		gpio_set_level(GPIO_LED_2_L, LED_OFF_LVL);
		break;
	default: /* LED_OFF and other unsupported colors */
		gpio_set_level(GPIO_LED_1_L, LED_OFF_LVL);
		gpio_set_level(GPIO_LED_2_L, LED_OFF_LVL);
		break;
	}
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		brightness_range[EC_LED_COLOR_RED] = 1;
		brightness_range[EC_LED_COLOR_AMBER] = 1;
		brightness_range[EC_LED_COLOR_GREEN] = 1;
	} else if (led_id == EC_LED_ID_POWER_LED) {
		brightness_range[EC_LED_COLOR_WHITE] = 1;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		if (brightness[EC_LED_COLOR_RED] != 0)
			led_set_color_battery(EC_LED_COLOR_RED);
		else if (brightness[EC_LED_COLOR_AMBER] != 0)
			led_set_color_battery(EC_LED_COLOR_AMBER);
		else if (brightness[EC_LED_COLOR_GREEN] != 0)
			led_set_color_battery(EC_LED_COLOR_GREEN);
		else
			led_set_color_battery(LED_OFF);
	} else if (led_id == EC_LED_ID_POWER_LED) {
		if (brightness[EC_LED_COLOR_WHITE] != 0)
			led_set_color_power(EC_LED_COLOR_WHITE);
		else
			led_set_color_power(LED_OFF);
	}

	return EC_SUCCESS;
}

static const uint16_t lightbar_i2c_addr = 0x68;
static void controller_write(uint8_t reg, uint8_t val)
{
	uint8_t buf[2];

	buf[0] = reg;
	buf[1] = val;

	i2c_xfer_unlocked(I2C_PORT_LIGHTBAR, lightbar_i2c_addr,
			buf, 2, 0, 0,
			I2C_XFER_SINGLE);
}

enum lightbar_states {
	LB_STATE_OFF,
	LB_STATE_LID_CLOSE,
	LB_STATE_AC_ONLY,
	LB_STATE_AC_BAT_LOW,
	LB_STATE_AC_BAT_20,
	LB_STATE_AC_BAT_40,
	LB_STATE_AC_BAT_60,
	LB_STATE_AC_BAT_80,
	LB_STATE_AC_BAT_100,
	LB_STATE_BAT_LOW,
	LB_STATE_BAT_ONLY,
	LB_NUM_STATES
};

/*
 * All lightbar states should have one phase defined,
 * and an additional phase can be defined for blinking
 */
enum lightbar_phase {
	LIGHTBAR_PHASE_0 = 0,
	LIGHTBAR_PHASE_1 = 1,
	LIGHTBAR_NUM_PHASES
};

enum ec_lightbar_colors {
	BAR_OFF             = 0x00,
	BAR_COLOR_ORG_2     = 0x01,
	BAR_COLOR_ORG_4     = 0x02,
	BAR_COLOR_ORG_6     = 0x03,
	BAR_COLOR_ORG_8     = 0x04,
	BAR_COLOR_ORG_FULL  = 0x05,
	BAR_COLOR_GREEN     = 0x06,
	LIGHTBAR_COLOR_TOTAL
};

struct lightbar_descriptor {
	enum ec_lightbar_colors color;
	uint8_t time;
};

#define BAR_INFINITE      UINT8_MAX
#define LIGHTBAR_ONE_SEC  (1000 / HOOK_TICK_INTERVAL_MS)
const struct lightbar_descriptor
	lb_table[LB_NUM_STATES][LIGHTBAR_NUM_PHASES] = {
	[LB_STATE_OFF]         = {{BAR_OFF, BAR_INFINITE} },
	[LB_STATE_LID_CLOSE]   = {{BAR_OFF, BAR_INFINITE} },
	[LB_STATE_AC_ONLY]     = {{BAR_OFF, BAR_INFINITE} },
	[LB_STATE_AC_BAT_LOW]  = {{BAR_COLOR_ORG_2, BAR_INFINITE} },
	[LB_STATE_AC_BAT_20]   = {{BAR_COLOR_ORG_4, BAR_INFINITE} },
	[LB_STATE_AC_BAT_40]   = {{BAR_COLOR_ORG_6, BAR_INFINITE} },
	[LB_STATE_AC_BAT_60]   = {{BAR_COLOR_ORG_8, BAR_INFINITE} },
	[LB_STATE_AC_BAT_80]   = {{BAR_COLOR_ORG_FULL, BAR_INFINITE} },
	[LB_STATE_AC_BAT_100] = {{BAR_COLOR_GREEN, BAR_INFINITE} },
	[LB_STATE_BAT_LOW]     = {{BAR_OFF, 5*LIGHTBAR_ONE_SEC},
				{BAR_COLOR_ORG_FULL, LIGHTBAR_ONE_SEC} },
	[LB_STATE_BAT_ONLY]    = {{BAR_OFF, BAR_INFINITE} },
};

static void lightbar_init(void)
{
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(0x02, 0x00);
		controller_write(0x03, 0x00);
		controller_write(0x04, 0x00);
		controller_write(0x05, 0x00);
		controller_write(0x06, 0x00);
		controller_write(0x07, 0x00);
		controller_write(0x08, 0x00);

		i2c_lock(I2C_PORT_LIGHTBAR, 0);
}

/*	From EE's information, lindar only support two colors lightbar,
 *  Orange (Amber) and Green. And they connect KTD20xx's red color
 *  channel to orange color led, and green color
 *  channel to green color led.
 *  Blue color channel is unused.
 *
 *	Reg0x02: Control Configuration
 *	         BIT7:6 is EN_MODE[1:0]
 *	         00 = global off, 01 = Night mode,
 *           10 = Normal mode, 11 = reset as default
 *
 *	Reg0x03: RED0 color's current setting
 *	Reg0x04: GREEN0 color's current setting
 *	Reg0x05: BLUE0 color's current setting
 *	Reg0x06: RED1 color's current setting
 *	Reg0x07: GREEN1 color's current setting
 *	Reg0x08: BLUE1 color's current setting
 *	Reg0x09: ISELA12 Selection Configuration
 *	Reg0x0A: ISELA34 Selection Configuration
 *	Reg0x0B: ISELB12 Selection Configuration
 *	Reg0x0C: ISELB34 Selection Configuration
 *	Reg0x0D: ISELC12 Selection Configuration
 */
static void lightbar_set_color(enum ec_lightbar_colors color)
{
	switch (color) {
	case BAR_COLOR_ORG_2:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(0x03, 0x02);
		controller_write(0x04, 0x00);
		controller_write(0x05, 0x00);
		controller_write(0x06, 0x00);
		controller_write(0x07, 0x00);
		controller_write(0x08, 0x00);

		controller_write(0x09, 0x88);
		controller_write(0x0A, 0x00);
		controller_write(0x0B, 0x00);
		controller_write(0x0C, 0x00);
		controller_write(0x0D, 0x00);
		controller_write(0x02, 0x80);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case BAR_COLOR_ORG_4:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(0x03, 0x02);
		controller_write(0x04, 0x00);
		controller_write(0x05, 0x00);
		controller_write(0x06, 0x00);
		controller_write(0x07, 0x00);
		controller_write(0x08, 0x00);

		controller_write(0x09, 0x88);
		controller_write(0x0A, 0x88);
		controller_write(0x0B, 0x00);
		controller_write(0x0C, 0x00);
		controller_write(0x0D, 0x00);
		controller_write(0x02, 0x80);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case BAR_COLOR_ORG_6:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(0x03, 0x02);
		controller_write(0x04, 0x00);
		controller_write(0x05, 0x00);
		controller_write(0x06, 0x00);
		controller_write(0x07, 0x00);
		controller_write(0x08, 0x00);

		controller_write(0x09, 0x88);
		controller_write(0x0A, 0x88);
		controller_write(0x0B, 0x88);
		controller_write(0x0C, 0x00);
		controller_write(0x0D, 0x00);
		controller_write(0x02, 0x80);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case BAR_COLOR_ORG_8:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(0x03, 0x02);
		controller_write(0x04, 0x00);
		controller_write(0x05, 0x00);
		controller_write(0x06, 0x00);
		controller_write(0x07, 0x00);
		controller_write(0x08, 0x00);

		controller_write(0x09, 0x88);
		controller_write(0x0A, 0x88);
		controller_write(0x0B, 0x88);
		controller_write(0x0C, 0x88);
		controller_write(0x0D, 0x00);
		controller_write(0x02, 0x80);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case BAR_COLOR_ORG_FULL:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(0x03, 0x02);
		controller_write(0x04, 0x00);
		controller_write(0x05, 0x00);
		controller_write(0x06, 0x00);
		controller_write(0x07, 0x00);
		controller_write(0x08, 0x00);

		controller_write(0x09, 0x88);
		controller_write(0x0A, 0x88);
		controller_write(0x0B, 0x88);
		controller_write(0x0C, 0x88);
		controller_write(0x0D, 0x88);
		controller_write(0x02, 0x80);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case BAR_COLOR_GREEN:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(0x03, 0x00);
		controller_write(0x04, 0x02);
		controller_write(0x05, 0x00);
		controller_write(0x06, 0x00);
		controller_write(0x07, 0x00);
		controller_write(0x08, 0x00);

		controller_write(0x09, 0x88);
		controller_write(0x0A, 0x88);
		controller_write(0x0B, 0x88);
		controller_write(0x0C, 0x88);
		controller_write(0x0D, 0x88);
		controller_write(0x02, 0x80);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case BAR_OFF:
	default: /* BAR_OFF and other unsupported colors */
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(0x02, 0x00);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	}
}

const int lightbar_bat_low = 20;

static enum lightbar_states lightbar_get_state(void)
{
	enum lightbar_states new_state = LB_NUM_STATES;

	if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND)) {
		if (!lid_is_open())
			new_state = LB_STATE_LID_CLOSE;
		else {
			if (extpower_is_present()) {
				if (battery_is_present()) {
					if (charge_get_percent() < 20)
						new_state = LB_STATE_AC_BAT_LOW;
					else if (charge_get_percent() < 40)
						new_state = LB_STATE_AC_BAT_20;
					else if (charge_get_percent() < 60)
						new_state = LB_STATE_AC_BAT_40;
					else if (charge_get_percent() < 80)
						new_state = LB_STATE_AC_BAT_60;
					else if (charge_get_percent() < 97)
						new_state = LB_STATE_AC_BAT_80;
					else
						new_state = LB_STATE_AC_BAT_100;
				} else
					new_state = LB_STATE_AC_ONLY;
			} else {
				if (charge_get_percent() < lightbar_bat_low)
					new_state = LB_STATE_BAT_LOW;
				else
					new_state = LB_STATE_BAT_ONLY;
			}
		}
	} else
		new_state = LB_STATE_OFF;

	return new_state;
}

static void lightbar_update(void)
{
	static uint8_t ticks, period;
	static enum lightbar_states lb_cur_state = LB_NUM_STATES;
	enum lightbar_states desired_state;
	int phase;

	desired_state = lightbar_get_state();
	if (desired_state != lb_cur_state &&
	    desired_state < LB_NUM_STATES) {
		/* State is changing */
		lb_cur_state = desired_state;
		/* Reset ticks and period when state changes */
		ticks = 0;

		period = lb_table[lb_cur_state][LIGHTBAR_PHASE_0].time +
			lb_table[lb_cur_state][LIGHTBAR_PHASE_1].time;
	}

	/* If this state is undefined, turn lightbar off */
	if (period == 0) {
		CPRINTS("Undefined lightbar behavior for lightbar state %d,"
			"turning off lightbar", lb_cur_state);
		lightbar_set_color(BAR_OFF);
		return;
	}

	/*
	 * Determine which phase of the state table to use. The phase is
	 * determined if it falls within first phase time duration.
	 */
	phase = ticks < lb_table[lb_cur_state][LIGHTBAR_PHASE_0].time ?
									0 : 1;
	ticks = (ticks + 1) % period;

	/* Set the color for the given state and phase */
	lightbar_set_color(lb_table[lb_cur_state][phase].color);

}
DECLARE_HOOK(HOOK_INIT, lightbar_init, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_TICK, lightbar_update, HOOK_PRIO_DEFAULT);
