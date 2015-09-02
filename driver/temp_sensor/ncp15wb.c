/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NCP15WB thermistor module for Chrome EC */

#include "common.h"
#include "console.h"
#include "ncp15wb.h"
#include "gpio.h"
#include "adc.h"
#include "temp_sensor.h"
#include "hooks.h"
#include "util.h"

/* List of active channels, ordered by pointer register */
static enum ec_own_adc_channel
	active_channels[OWN_ADC_CHANNEL_COUNT];

static int temp_val[OWN_ADC_CHANNEL_COUNT + 1];

/*
 * ADC-to-temp conversion assumes recommended thermistor / resistor
 * configuration specified in datasheet (NCP15WB* / 24.9K).
 * For 50C through 100C, use linear interpolation from discreet points
 * in table below. For temps < 50C, use a simplified linear function.
 */
#define ADC_DISCREET_RANGE_START_TEMP	50
/* 10 bit ADC result corresponding to START_TEMP */
#define ADC_DISCREET_RANGE_START_RESULT	407

#define ADC_DISCREET_RANGE_LIMIT_TEMP	100
/* 10 bit ADC result corresponding to LIMIT_TEMP */
#define ADC_DISCREET_RANGE_LIMIT_RESULT	107

/* Table entries in steppings of 5C */
#define ADC_DISCREET_RANGE_STEP		5

/* Discreet range ADC results (9 bit) per temperature, in 5 degree steps */
static const uint8_t adc_result[] = {
	203,	/* 50 C */
	178,	/* 55 C */
	157,	/* 60 C */
	138,	/* 65 C */
	121,	/* 70 C */
	106,	/* 75 C */
	93,	/* 80 C */
	81,	/* 85 C */
	70,	/* 90 C */
	61,	/* 95 C */
	53,	/* 100 C */
};

/*
 * From 20C (reasonable lower limit of temperatures we care about accuracy)
 * to 50C, the temperature curve is roughly linear, so we don't need to include
 * data points in our table.
 */
#define adc_to_temp(result) (ADC_DISCREET_RANGE_START_TEMP - \
	(((result) - ADC_DISCREET_RANGE_START_RESULT) * 3 + 16) / 32)

/**
 * Determine whether the sensor is powered.
 *
 * @return non-zero the ncp15wb sensor is powered.
 */
static int has_power(void)
{
#ifdef CONFIG_TEMP_SENSOR_POWER_GPIO
	return gpio_get_level(CONFIG_TEMP_SENSOR_POWER_GPIO);
#else
	return 1;
#endif
}

#if CONFIG_TEMP_SENSOR_MEC1322_OWN
static void ncp15wb_init(void)
{
	int i;
	int active_channel_count = 0;

	/* Mark active channels from the board temp sensor table */
	for (i = 0; i < TEMP_SENSOR_COUNT; ++i)
		if (temp_sensors[i].read == ncp15wb_get_val)
			active_channels[active_channel_count++] =
				temp_sensors[i].idx;

	/* Make sure we don't have too many active channels. */
	ASSERT(active_channel_count <= ARRAY_SIZE(active_channels));

	/* Mark the first unused channel so we know where to stop searching */
	if (active_channel_count != ARRAY_SIZE(active_channels))
		active_channels[active_channel_count] =
			OWN_ADC_CHANNEL_NONE;
}
DECLARE_HOOK(HOOK_INIT, ncp15wb_init, HOOK_PRIO_DEFAULT);
#endif

/* Convert ADC result to temperature in celsius */
int ncp15wb_calculate_temp(uint16_t adc)
{
	int temp;
	int head, tail, mid;
	uint8_t delta, step;

	/* Is ADC result in linear range? */
	if (adc >= ADC_DISCREET_RANGE_START_RESULT) {
		temp = adc_to_temp(adc);
	}
	/* Hotter than our discreet range limit? */
	else if (adc <= ADC_DISCREET_RANGE_LIMIT_RESULT) {
		temp = ADC_DISCREET_RANGE_LIMIT_TEMP;
	}
	/* We're in the discreet range */
	else {
		/* Table uses 9 bit ADC values */
		adc /= 2;

		/* Binary search to find proper table entry */
		head = 0;
		tail = ARRAY_SIZE(adc_result) - 1;
		while (head != tail) {
			mid = (head + tail) / 2;
			if (adc_result[mid] >= adc &&
			    adc_result[mid+1] < adc)
				break;
			if (adc_result[mid] > adc)
				head = mid + 1;
			else
				tail = mid;
		}

		/* Now fit between table entries using linear interpolation. */
		if (head != tail) {
			delta = adc_result[mid] - adc_result[mid + 1];
			step = ((adc_result[mid] - adc) *
				ADC_DISCREET_RANGE_STEP + delta / 2) / delta;
		} else {
			/* Edge case where adc = max */
			mid = head;
			step = 0;
		}

		temp = ADC_DISCREET_RANGE_START_TEMP +
		       ADC_DISCREET_RANGE_STEP * mid + step;
	}

	return temp;
}

int ncp15wb_get_val(int idx, int *temp_ptr)
{
	if (!has_power())
		return EC_ERROR_NOT_POWERED;

	if(temp_val[idx] >0)
		*temp_ptr = temp_val[idx];
	else
		return EC_ERROR_UNKNOWN;

	return EC_SUCCESS;
}

#if CONFIG_TEMP_SENSOR_MEC1322_OWN
/* Get temperature from requested sensor */
static int get_temp(int idx, int *temp_ptr)
{
	uint16_t adc;
	int temp_raw = 0;

	/* Read 10-bit ADC result */
	temp_raw = adc_read_channel(idx);

	if (temp_raw == ADC_READ_ERROR)
		return EC_ERROR_UNKNOWN;

	adc = temp_raw;
	*temp_ptr = ncp15wb_calculate_temp(adc);

	return EC_SUCCESS;
}

static void ncp15wb_temp_sensor_poll(void)
{
	int temp_c;
	int active_channel_count = 0;

	if (!has_power())
		return;

	while(active_channels[active_channel_count] != OWN_ADC_CHANNEL_NONE) {
		if (get_temp(active_channel_count, &temp_c) == EC_SUCCESS)
			temp_val[active_channel_count] = C_TO_K(temp_c);
		++active_channel_count;
	}
}
DECLARE_HOOK(HOOK_SECOND, ncp15wb_temp_sensor_poll, \
											HOOK_PRIO_TEMP_SENSOR);
#endif

static int print_status(void)
{
	int i,value;
	int ch = 0;

	while(active_channels[ch] != OWN_ADC_CHANNEL_NONE) {
		for (i = 0; i < TEMP_SENSOR_COUNT; ++i)
			if(temp_sensors[i].read == ncp15wb_get_val &&
				active_channels[ch] == temp_sensors[i].idx)
				break;
		ccprintf("%s : \n", temp_sensors[i].name);
		ccprintf("	Raw Data :\t %3d\n", value = adc_read_channel(ch));
		ccprintf("	Temperature :\t %53C\n", ncp15wb_calculate_temp((uint16_t) value));
		++ch;
	}
	ccprintf("\n");

	return EC_SUCCESS;
}

static int command_ncp15wb(int argc, char **argv)
{
	if (!has_power()) {
		ccprintf("ERROR: Temp sensor not powered.\n");
		return EC_ERROR_NOT_POWERED;
	}

	/* If no args just print status */
	if (argc == 1)
		return print_status();
	else
		return EC_ERROR_PARAM_COUNT;
}
DECLARE_CONSOLE_COMMAND(ncp15wb, command_ncp15wb,
	NULL,
	"Print ncp15wb temp sensor return value.(Celius)",
	NULL);
