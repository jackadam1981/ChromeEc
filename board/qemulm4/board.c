/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* LM4 QEMU emulator board-specific configuration */

#include "adc.h"
#include "board.h"
#include "gpio.h"
#include "lm4_adc.h"
#include "registers.h"
#include "util.h"

/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[ADC_CH_COUNT] = {
	/* EC internal temperature is calculated by
	 * 273 + (295 - 450 * ADC_VALUE / ADC_READ_MAX) / 2
	 * = -225 * ADC_VALUE / ADC_READ_MAX + 420.5
	 */
	{"ECTemp", LM4_ADC_SEQ0, -225, ADC_READ_MAX, 420,
	 LM4_AIN_NONE, 0x0e /* TS0 | IE0 | END0 */},
};


/* GPIO signal list.  Must match order from enum gpio_signal. */
const struct gpio_info gpio_list[GPIO_COUNT] = {
	{"RECOVERYn",   LM4_GPIO_D, (1<<1), GPIO_PULL_UP, NULL},
	GPIO_SIGNAL_NOT_IMPLEMENTED("WRITE_PROTECT"),
};


void configure_board(void)
{
}
