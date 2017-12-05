/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
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

enum adc_hw_channel {
	ADC_HW_CH_0 = 0,
	ADC_HW_CH_1,
	ADC_HW_CH_2,
	ADC_HW_CH_3,
	ADC_HW_CH_4,
	ADC_HW_CH_5,
	ADC_HW_CH_6,
	ADC_HW_CH_7,
	ADC_HW_CH_13 = 13,
	ADC_HW_CH_14,
	ADC_HW_CH_15,
	ADC_HW_CH_16,
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
	int channel;
};

/*
 * Boards must provide this list of ADC channel definitions. This must match
 * the enum adc_channel list provided by the board.
 */
extern const struct adc_t adc_channels[];

#endif /* __CROS_EC_ADC_CHIP_H */
