/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* APL6012 temperature sensor module for Chrome EC */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "math_util.h"
#include "temp_sensor.h"
#include "temp_sensor/apl6012.h"
#include "temp_sensor/thermistor.h"
#include "util.h"

#ifdef CONFIG_ZEPHYR
#include "temp_sensor/temp_sensor.h"
#endif

static int temps[APL6012_IDX_COUNT];

static int raw_read8(int sensor, const int offset, int *data)
{
	return i2c_read8(apl6012_sensors[sensor].i2c_port,
			 apl6012_sensors[sensor].i2c_addr_flags, offset, data);
}

static int get_adc(int sensor, const int offset, int *adc)
{
	int rv;
	int adc_raw = 0;

	rv = raw_read8(sensor, offset, &adc_raw);
	if (rv != 0)
		return rv;

	*adc = adc_raw;
	return EC_SUCCESS;
}

int apl6012_get_val_k(int idx, int *temp_k_ptr)
{
	if (idx >= APL6012_IDX_COUNT)
		return EC_ERROR_INVAL;

	*temp_k_ptr = MILLI_KELVIN_TO_KELVIN(temps[idx]);
	return EC_SUCCESS;
}

#ifndef CONFIG_ZEPHYR
static void apl6012_sensor_poll(void)
{
	int index;
	int rv;
	uint16_t mv;
	int adc_val;
	int temp_c;

	for (index = 0; index < APL6012_IDX_COUNT; index++) {
		rv = get_adc(index, (APL6012_TD1 + index), &adc_val);
		if (rv != 0)
			continue;

		mv = adc_val * 10 + 1000;
		temp_c = thermistor_linear_interpolate(mv, &apl6012_thermistor_info[index]);
		temps[index] = CELSIUS_TO_MILLI_KELVIN(temp_c);
	}
}
DECLARE_HOOK(HOOK_SECOND, apl6012_sensor_poll, HOOK_PRIO_TEMP_SENSOR);
#else
static const struct temp_sensor_t *find_temp_sensor_by_idx(int idx)
{
	int i;

	for (i = 0; i < TEMP_SENSOR_COUNT; i++) {
		if (temp_sensors[i].idx == idx) {
			return &temp_sensors[i];
		}
	}

	return NULL;
}

void apl6012_update_temperature(int idx)
{
	int rv;
	uint16_t mv;
	int adc_val;
	int temp_c;
	const struct temp_sensor_t *sensor = find_temp_sensor_by_idx(idx);

	if (!sensor)
		return;

	rv = get_adc(idx, (APL6012_TD1 + idx), &adc_val);
	if (rv != 0)
		return;

	mv = adc_val * 10 + 1000;
	temp_c = thermistor_linear_interpolate(mv, sensor->zephyr_info->thermistor);
	temps[idx] = CELSIUS_TO_MILLI_KELVIN(temp_c);
}

static int apl6012_debug(int argc, const char **argv)
{
	int index;
	char *e;
	uint16_t mv;
	int temp_c;
	const struct temp_sensor_t *sensor;

	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;

	index = strtoi(argv[1], &e, 0);
	if ((*e) || (index < 0) || (index >= APL6012_IDX_COUNT))
		return EC_ERROR_PARAM1;

	sensor = find_temp_sensor_by_idx(index);
	mv = strtoi(argv[2], &e, 0);

	if ((*e) || (mv < 1000) || (mv > 3560))
		return EC_ERROR_PARAM2;

	temp_c = thermistor_linear_interpolate(mv, sensor->zephyr_info->thermistor);
	ccprintf("   CH %d, mV = %d\n", index+1, mv);
	ccprintf("   NTC name: %s\n", sensor->name);
	ccprintf("   temp_c = %d\n", temp_c);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(apl6012, apl6012_debug, "<index> <mV value>",
			"Set fake mV to get temperature of sensor apl6012.");
#endif /* CONFIG_ZEPHYR */
