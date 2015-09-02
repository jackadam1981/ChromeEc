/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NCP15WB thermistor module for Chrome EC */

#include "common.h"
#include "console.h"
#include "ncp15wb.h"
#include "ec_adc.h"
#include "gpio.h"
#include "adc.h"
#include "temp_sensor.h"
#include "hooks.h"
#include "util.h"

/* List of active channels, ordered by pointer register */
static enum adc_channel active_channels[ADC_CH_COUNT];

static int temp_val[ADC_CH_COUNT];

static void thermistor_init(void)
{
	int i;
	int active_channel_count = 0;

	/* Mark active channels from the board temp sensor table */
	for (i = 0; i < TEMP_SENSOR_COUNT; ++i)
		if (temp_sensors[i].read == thermistor_get_val)
			active_channels[active_channel_count++] =
				temp_sensors[i].idx;

	/* Make sure we don't have too many active channels. */
	ASSERT(active_channel_count <= ARRAY_SIZE(active_channels));

	/* Mark the first unused channel so we know where to stop searching */
	if (active_channel_count != ARRAY_SIZE(active_channels))
		active_channels[active_channel_count] = ADC_CH_COUNT;
}
DECLARE_HOOK(HOOK_INIT, thermistor_init, HOOK_PRIO_DEFAULT);

int thermistor_get_val(int idx, int *temp_ptr)
{
	if(temp_val[idx] >0)
		*temp_ptr = temp_val[idx];
	else
		return EC_ERROR_UNKNOWN;

	return EC_SUCCESS;
}

/* Get temperature from requested sensor */
static int get_temp(int idx, int *temp_ptr)
{
	uint16_t adc;
	int temp_raw = 0;

	/* Read 10-bit ADC result */
	temp_raw = adc_read_channel(idx);

	if (temp_raw == ADC_READ_ERROR)
		return EC_ERROR_UNKNOWN;

	/* TODO : 
	 * 
	 * 
	 * Need modification here if the result is not 10-bit
	 */
	adc = temp_raw;
	*temp_ptr = ncp15wb_calculate_temp(adc);

	return EC_SUCCESS;
}

static void thermistor_poll(void)
{
	int temp_c;
	int count = 0;

	while(active_channels[count] != ADC_CH_COUNT) {
		if (get_temp(count, &temp_c) == EC_SUCCESS)
			temp_val[count] = C_TO_K(temp_c);
		++count;
	}
}
DECLARE_HOOK(HOOK_SECOND, thermistor_poll, \
								HOOK_PRIO_TEMP_SENSOR);

static int print_status(void)
{
	int i,value;
	int ch = 0;

	while(active_channels[ch] != ADC_CH_COUNT) {
		for (i = 0; i < TEMP_SENSOR_COUNT; ++i)
			if(temp_sensors[i].read == thermistor_get_val &&
				active_channels[ch] == temp_sensors[i].idx)
				break;
		ccprintf("%s : \n", temp_sensors[i].name);
		ccprintf("	Raw Data :\t %3d\n", value = adc_read_channel(ch));
		ccprintf("	Temperature :\t %3d\n", ncp15wb_calculate_temp((uint16_t) value));
		++ch;
	}
	ccprintf("\n");

	return EC_SUCCESS;
}

static int command_thermistor(int argc, char **argv)
{
	/* If no args just print status */
	if (argc == 1)
		return print_status();
	else
		return EC_ERROR_PARAM_COUNT;
}
DECLARE_CONSOLE_COMMAND(thermistor, command_thermistor,
	NULL,
	"Print ncp15wb temp sensor return value.(Celius)",
	NULL);
