/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "charge_state.h"
#include "common.h"
#include "driver/charger/rt9490.h"
#include "hooks.h"
#include "temp_sensor/temp_sensor.h"
#define NUM_CURRENT_LEVELS ARRAY_SIZE(current_table)
#define TEMP_THRESHOLD 200 /* TODO: need to reback */
#define TEMP_BUFF_SIZE 60
#define KEEP_TIME 5
BUILD_ASSERT(IS_ENABLED(CONFIG_BOARD_VELUZA) || IS_ENABLED(CONFIG_TEST));
/* calculate current average temperature */

enum temperature_sensor_type {
	I2C_CHARGER = 0,
	ADC_5V,
	ADC_AMB,
	NUM_THERMISTOR_TYPES
};

static int temp_history_buffer[NUM_THERMISTOR_TYPES][TEMP_BUFF_SIZE];
static int buff_ptr[NUM_THERMISTOR_TYPES];
static int temp_sum[NUM_THERMISTOR_TYPES];
static int avg_temp[NUM_THERMISTOR_TYPES];

static void average_tempature(void)
{
	static int past_temp;
	int cur_temp, t;

	for (enum temperature_sensor_type type = 0; type < NUM_THERMISTOR_TYPES;
	     type++) {
		switch (type) {
		case I2C_CHARGER:
			temp_sensor_read(
				TEMP_SENSOR_ID(DT_NODELABEL(temp_charger)), &t);
			break;
		case ADC_5V:
			temp_sensor_read(
				TEMP_SENSOR_ID(DT_NODELABEL(temp_adc5v)), &t);
			break;
		case ADC_AMB:
			temp_sensor_read(
				TEMP_SENSOR_ID(DT_NODELABEL(temp_adcambient)),
				&t);
			break;
		default:
			break;
		}
		cur_temp = K_TO_C(t);
		past_temp = temp_history_buffer[type][buff_ptr[type]];
		temp_history_buffer[type][buff_ptr[type]] = cur_temp;
		temp_sum[type] = temp_sum[type] +
				 temp_history_buffer[type][buff_ptr[type]] -
				 past_temp;
		buff_ptr[type]++;
		if (buff_ptr[type] >= TEMP_BUFF_SIZE) {
			buff_ptr[type] = 0;
		}

		/* Calculate per minute temperature.
		 * It's expected low temperature when the first 60 seconds.
		 */
		avg_temp[type] = temp_sum[type] / TEMP_BUFF_SIZE;
	}
}
DECLARE_HOOK(HOOK_SECOND, average_tempature, HOOK_PRIO_DEFAULT);

static int current_level;
/* Limit charging current table : 3600/3000/2400/1800
 * note this should be in descending order.
 */
static uint16_t current_table[] = {
	3600,
	3000,
	2400,
	1600,
};
/* Called by hook task every hook second (1 sec) */
static void current_update(void)
{
	static uint8_t uptime;
	static uint8_t dntime;

#ifndef CONFIG_TEST
	if (led_pwr_get_state() == LED_PWRS_DISCHARGE) {
		current_level = 0;
		uptime = 0;
		dntime = 0;
		return;
	}
#endif
	if (avg_temp[I2C_CHARGER] >= TEMP_THRESHOLD) {
		dntime = 0;
		if (uptime < KEEP_TIME) {
			uptime++;
		} else {
			uptime = 0;
			current_level++;
		}
	} else if (current_level != 0 &&
		   avg_temp[I2C_CHARGER] < TEMP_THRESHOLD) {
		uptime = 0;
		if (dntime < KEEP_TIME) {
			dntime++;
		} else {
			dntime = 0;
			current_level--;
		}
	} else {
		uptime = 0;
		dntime = 0;
	}
	if (current_level > NUM_CURRENT_LEVELS) {
		current_level = NUM_CURRENT_LEVELS;
	}
}
DECLARE_HOOK(HOOK_SECOND, current_update, HOOK_PRIO_DEFAULT);

#define AMBIENT_TEMP_THRESHOLD 55

struct safety_struct {
	int trigger_temp;
	int release_temp;
};

static const struct safety_struct safety_table[] = {
	{ 60, 59 },
	{ 63, 62 },
};
#define SAFETY_LEVELS ARRAY_SIZE(safety_table)

static void safety_protection(void)
{
	static uint8_t uptime;
	static uint8_t dntime;
	static int delay_time;
	static int safety_level;
	static int pre_safety_level;

	if (!chipset_in_state(CHIPSET_STATE_ON) ||
	    (avg_temp[ADC_AMB] >= AMBIENT_TEMP_THRESHOLD)) {
		/* If the ambient temp is over threshold then skip the process.
		 */
		safety_level = 0;
		delay_time = 0;
		uptime = 0;
		dntime = 0;
	} else {
		/* TODO: I2C_CHARGER need to change to ADC_5V */

		if (delay_time) {
			delay_time--;
		} else if (safety_level != 0 &&
			   avg_temp[I2C_CHARGER] <=
				   safety_table[safety_level - 1].release_temp) {
			/* Increase level */
			uptime = 0;
			if (dntime < KEEP_TIME) {
				dntime++;
			} else {
				dntime = 0;
				safety_level--;
				ccprints("safety_level = %d. avg_temp=%d",
					 safety_level, avg_temp[I2C_CHARGER]);
			}
		} else if ((safety_level + 1) <= SAFETY_LEVELS &&
			   avg_temp[I2C_CHARGER] >=
				   safety_table[safety_level].trigger_temp) {
			/* Decrease level */
			dntime = 0;
			if (uptime < KEEP_TIME) {
				uptime++;
			} else {
				uptime = 0;
				safety_level++;
				ccprints("safety_level = %d. avg_temp=%d",
					 safety_level, avg_temp[I2C_CHARGER]);
			}
		} else {
			uptime = 0;
			dntime = 0;
		}
	}

	if (pre_safety_level != safety_level) {
		switch (safety_level) {
		case 0:
			host_set_single_event(EC_HOST_EVENT_THROTTLE_STOP);
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(en_pp5000_usb_a0_vbus),
				1);
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(en_pp5000_usb_a1_vbus_x),
				1);
			break;
		case 1:
			if (pre_safety_level != 2) {
				host_set_single_event(
					EC_HOST_EVENT_THROTTLE_START);
				delay_time = 60;
			}
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(en_pp5000_usb_a0_vbus),
				1);
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(en_pp5000_usb_a1_vbus_x),
				1);
			break;
		case 2:
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(en_pp5000_usb_a0_vbus),
				0);
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(en_pp5000_usb_a1_vbus_x),
				0);
			break;
		}
	}

	pre_safety_level = safety_level;
}
DECLARE_HOOK(HOOK_SECOND, safety_protection, HOOK_PRIO_DEFAULT);

int charger_profile_override(struct charge_state_data *curr)
{
	/*
	 * Precharge must be executed when communication is failed on
	 * dead battery.
	 */
	if (!(curr->batt.flags & BATT_FLAG_RESPONSIVE))
		return 0;
	if (current_level != 0) {
		if (curr->requested_current > current_table[current_level - 1])
			curr->requested_current =
				current_table[current_level - 1];
	}
	return 0;
}
enum ec_status charger_profile_override_get_param(uint32_t param,
						  uint32_t *value)
{
	return EC_RES_INVALID_PARAM;
}
enum ec_status charger_profile_override_set_param(uint32_t param,
						  uint32_t value)
{
	return EC_RES_INVALID_PARAM;
}
