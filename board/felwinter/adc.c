/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "chipset.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "power.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "throttle_ap.h"

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

void pp1050_pgood_high(void)
{
	gpio_set_level(GPIO_EC_PROCHOT_IN_L, 1);
	/* Disable this interrupt while it's asserted. */
	npcx_adc_thresh_int_enable(NPCX_ADC_THRESH1, 0);
	/* Enable the voltage low interrupt. */
	npcx_adc_thresh_int_enable(NPCX_ADC_THRESH2, 1);

	throttle_ap_prochot_input_interrupt(GPIO_EC_PROCHOT_IN_L);
}

void pp1050_pgood_low(void)
{
	gpio_set_level(GPIO_EC_PROCHOT_IN_L, 0);
	/* Disable this interrupt while it's asserted. */
	npcx_adc_thresh_int_enable(NPCX_ADC_THRESH2, 0);
	/* Enable the voltage high interrupt. */
	npcx_adc_thresh_int_enable(NPCX_ADC_THRESH1, 1);

	throttle_ap_prochot_input_interrupt(GPIO_EC_PROCHOT_IN_L);
}

const struct npcx_adc_thresh_t adc_pp1050_pgood_high = {
	.adc_ch = ADC_PROCHOT_IN_L,
	.adc_thresh_cb = pp1050_pgood_high,
	.thresh_assert = 900,
};

const struct npcx_adc_thresh_t adc_pp1050_pgood_low = {
	.adc_ch = ADC_PROCHOT_IN_L,
	.adc_thresh_cb = pp1050_pgood_low,
	.lower_or_higher = 1,
	.thresh_assert = 600,
};

static void set_up_adc_irqs(void)
{
	/* Set interrupt thresholds for the ADC. */
	CPRINTS("%s", __func__);
	npcx_adc_register_thresh_irq(NPCX_ADC_THRESH1,
				     &adc_pp1050_pgood_high);
	npcx_adc_register_thresh_irq(NPCX_ADC_THRESH2, &adc_pp1050_pgood_low);
	npcx_set_adc_repetitive(adc_channels[ADC_PROCHOT_IN_L].input_ch, 1);
	npcx_adc_thresh_int_enable(NPCX_ADC_THRESH1, 1);
	npcx_adc_thresh_int_enable(NPCX_ADC_THRESH2, 1);
}
DECLARE_HOOK(HOOK_INIT, set_up_adc_irqs, HOOK_PRIO_INIT_ADC+1);
