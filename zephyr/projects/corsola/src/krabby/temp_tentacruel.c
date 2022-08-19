/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger.h"
#include "charge_state.h"
#include "common.h"
#include "config.h"
#include "console.h"
#include "driver/charger/rt9490.h"
#include "hooks.h"
#include "temp_sensor/temp_sensor.h"
#include "thermal.h"
#include "util.h"

#define NUM_CURRENT_LEVELS ARRAY_SIZE(current_table)
#define TEMP_THRESHOLD 55
#define TEMP_BUFF_SIZE 60

/* Calcute current average temperature */
static int Average_tempature(void)
{
	static int temp_history_buffer[TEMP_BUFF_SIZE];
	static int buff_ptr;
	static int temp_sum;
	static int Past_Temp;
	static int Avg_Temp;
	int Cur_Temp, t;

	temp_sensor_read(TEMP_SENSOR_CHARGER, &t);
	Cur_Temp = K_TO_C(t);
	Past_Temp = temp_history_buffer[buff_ptr];
	temp_history_buffer[buff_ptr] = Cur_Temp;
	temp_sum = temp_sum + temp_history_buffer[buff_ptr] - Past_Temp;
	buff_ptr++;

	if (buff_ptr >= TEMP_BUFF_SIZE) {
		buff_ptr = 0;
	}

	Avg_Temp = temp_sum / TEMP_BUFF_SIZE;

	return Avg_Temp;
}

static int current_level;

static uint16_t current_table[] = {
	3600,
	3000,
	2400,
	1800,
};

/* Called by hook task every hook second (1 sec) */
static void current_update(void)
{
	int temp;
	static int Uptime;
	static int Dntime;

	temp = Average_tempature();
	if (temp >= TEMP_THRESHOLD) {
		Dntime = 0;
		if (Uptime < 5)
			Uptime++;
		else {
			Uptime = 0;
			current_level++;
		}
	} else if (current_level != 0 && temp < TEMP_THRESHOLD) {
		Uptime = 0;
		if (Dntime < 5)
			Dntime++;
		else {
			Dntime = 0;
			current_level--;
		}
	} else {
		Uptime = 0;
		Dntime = 0;
	}

	if (current_level < 0)
		current_level = 0;
	else if (current_level > NUM_CURRENT_LEVELS)
		current_level = NUM_CURRENT_LEVELS;
}
DECLARE_HOOK(HOOK_SECOND, current_update, HOOK_PRIO_DEFAULT);

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
