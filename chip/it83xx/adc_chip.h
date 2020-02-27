/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IT83xx ADC module for Chrome EC */

#ifndef __CROS_EC_ADC_CHIP_H
#define __CROS_EC_ADC_CHIP_H

#include "common.h"

/*
 * Maximum time we allow for an ADC conversion.
 * NOTE:
 * This setting must be less than "SLEEP_SET_HTIMER_DELAY_USEC" in clock.c
 * or adding a sleep mask to prevent going in to deep sleep while ADC
 * converting.
 */
#define ADC_TIMEOUT_US 248

/* Minimum and maximum values returned by adc_read_channel(). */
#define ADC_READ_MIN 0
#define ADC_READ_MAX 1023
#define ADC_MAX_MVOLT 3000

/* Definitions for voltage comparator */
#define V_CMP_READ_MAX            1023
#define GREATER_THRESHOLD         BIT(0)
#define LESS_EQUAL_THRESHOLD      BIT(1)
#define EDGE_TRIGGER              BIT(2)
#define LEVEL_TRIGGER             BIT(3)

/* List of ADC channels. */
enum chip_adc_channel {
	CHIP_ADC_CH0 = 0,
	CHIP_ADC_CH1,
	CHIP_ADC_CH2,
	CHIP_ADC_CH3,
	CHIP_ADC_CH4,
	CHIP_ADC_CH5,
	CHIP_ADC_CH6,
	CHIP_ADC_CH7,
	CHIP_ADC_CH13,
	CHIP_ADC_CH14,
	CHIP_ADC_CH15,
	CHIP_ADC_CH16,
	CHIP_ADC_COUNT,
};

/* List of voltage comparator channels. */
enum chip_vcmp_channel {
	CHIP_VCMP_CH0 = 0,
	CHIP_VCMP_CH1,
	CHIP_VCMP_CH2,
	CHIP_VCMP_CH3,
	CHIP_VCMP_CH4,
	CHIP_VCMP_CH5,
	CHIP_VCMP_COUNT,
};

/* Data structure to define ADC channel control registers. */
struct adc_ctrl_t {
	volatile uint8_t *adc_ctrl;
	volatile uint8_t *adc_datm;
	volatile uint8_t *adc_datl;
	volatile uint8_t *adc_pin_ctrl;
};

/* Data structure to define ADC channels. */
struct adc_t {
	const char *name;
	int factor_mul;
	int factor_div;
	int shift;
	enum chip_adc_channel channel;
};

/* Data structure to define voltage comparator channel control registers. */
struct vcmp_ctrl_t {
	volatile uint8_t *vcmp_ctrl;
	volatile uint8_t *vcmp_datm;
	volatile uint8_t *vcmp_datl;
	/*
	 * When voltage comparator trigger IRQ151 and also can output H/L
	 * signal via this GPIO.
	 */
	volatile uint8_t *vcmp_output_pin_ctrl;
};

/* Data structure to define voltage comparator channels. */
struct vcmp_t {
	const char *name;
	int threshold;
	char condition;
	int resolution;
	enum chip_vcmp_channel vcmp_channel;
	/*
	 * Select which ADC channel output voltage into comparator and we
	 * should set the ADC channel pin in alternate mode via adc_channels[].
	 */
	enum chip_adc_channel adc_channel;
};

/*
 * Boards must provide this list of ADC channel definitions. This must match
 * the enum adc_channel list provided by the board.
 */
extern const struct adc_t adc_channels[];

/*
 * Boards must provide this list of voltage comparator channel definitions.
 * This must match the enum chip_vcmp_channel list provided by the board.
 */
extern struct vcmp_t vcmp_channels[];

#endif /* __CROS_EC_ADC_CHIP_H */
