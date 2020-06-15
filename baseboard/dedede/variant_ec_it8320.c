/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common code for VARIANT_DEDEDE_IT8320 configuration */

#include "adc_chip.h"
#include "atomic.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "power.h"
#include "registers.h"

#define CPRINTUSB(format, args...) cprints(CC_USBCHARGE, format, ## args)

/* ADC channels */
const struct adc_t adc_channels[] = {
	[ADC_VSNS_PP3300_A] = {
		"PP3300_A_PGOOD", CHIP_ADC_CH0, ADC_MAX_MVOLT, ADC_READ_MAX+1,
		0},
	[ADC_TEMP_SENSOR_1] = {
		"TEMP_SENSOR1", CHIP_ADC_CH2, ADC_MAX_MVOLT, ADC_READ_MAX+1, 0},

	[ADC_TEMP_SENSOR_2] = {
		"TEMP_SENSOR2", CHIP_ADC_CH3, ADC_MAX_MVOLT, ADC_READ_MAX+1, 0},

	[ADC_SUB_ANALOG] = {
		"SUB_ANALOG", CHIP_ADC_CH13, ADC_MAX_MVOLT, ADC_READ_MAX+1, 0},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

static void pp3300_a_pgood_low(void)
{
	atomic_clear(&pp3300_a_pgood, 1);

	/* Disable low interrupt while asserted */
	vcmp_enable(VCMP_SNS_PP3300_LOW, 0);

	/* Enable high interrupt */
	vcmp_enable(VCMP_SNS_PP3300_HIGH, 1);

	/*
	 * Call power_signal_interrupt() with a dummy GPIO in order for the
	 * chipset task to pick up the change in power sequencing signals.
	 */
	power_signal_interrupt(GPIO_PG_EC_DSW_PWROK);
}

static void pp3300_a_pgood_high(void)
{
	atomic_or(&pp3300_a_pgood, 1);

	/* Disable high interrupt while asserted */
	vcmp_enable(VCMP_SNS_PP3300_HIGH, 0);

	/* Enable low interrupt */
	vcmp_enable(VCMP_SNS_PP3300_LOW, 1);

	/*
	 * Call power_signal_interrupt() with a dummy GPIO in order for the
	 * chipset task to pick up the change in power sequencing signals.
	 */
	power_signal_interrupt(GPIO_PG_EC_DSW_PWROK);
}

const struct vcmp_t vcmp_list[] = {
	[VCMP_SNS_PP3300_LOW] = {
		.name = "VCMP_SNS_PP3300_LOW",
		.threshold = 600, /* mV */
		.flag = LESS_EQUAL_THRESHOLD,
		.vcmp_thresh_cb = &pp3300_a_pgood_low,
		.scan_period = VCMP_SCAN_PERIOD_600US,
		.adc_ch = CHIP_ADC_CH0,
	},
	[VCMP_SNS_PP3300_HIGH] = {
		.name = "VCMP_SNS_PP3300_HIGH",
		.threshold = 2700, /* mV */
		.flag = GREATER_THRESHOLD,
		.vcmp_thresh_cb = &pp3300_a_pgood_high,
		.scan_period = VCMP_SCAN_PERIOD_600US,
		.adc_ch = CHIP_ADC_CH0,
	},
};
BUILD_ASSERT(ARRAY_SIZE(vcmp_list) <= CHIP_VCMP_COUNT);
BUILD_ASSERT(ARRAY_SIZE(vcmp_list) == VCMP_COUNT);
