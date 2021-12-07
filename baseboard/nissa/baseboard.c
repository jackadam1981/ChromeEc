/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "hooks.h"
#include "adc.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

#define PP3300_S5_GOOD_ADC_CHANNEL	6
#define PP3300_S5_GOOD_ON_THR_ID	0
#define PP3300_S5_GOOD_OFF_THR_ID	1

/* PP3300_S5_GOOD voltages in milivolts */
#define PP3300_S5_GOOD_ON_ADC_VOLTAGE	2700
#define PP3300_S5_GOOD_OFF_ADC_VOLTAGE	600

/* Store away pp300_s5_good status before sysjumps */
#define BASEBOARD_SYSJUMP_TAG   0x4242 /* BB */
#define BASEBOARD_HOOK_VERSION  1

int pp3300_s5_good = 0;

static void pp3300_s5_good_preserve(void)
{
	system_add_jump_tag(BASEBOARD_SYSJUMP_TAG, BASEBOARD_HOOK_VERSION,
			    sizeof(pp3300_s5_good), &pp3300_s5_good);
}
DECLARE_HOOK(HOOK_SYSJUMP, pp3300_s5_good_preserve, HOOK_PRIO_DEFAULT);

static void baseboard_prepare_power_signals(void)
{
	const int *stored;
	int version, size;

	stored = (const int *)system_get_jump_tag(BASEBOARD_SYSJUMP_TAG,
						  &version, &size);
	if (stored && (version == BASEBOARD_HOOK_VERSION) &&
	   (size == sizeof(pp3300_s5_good)))
		/* Valid PP3300 status found, restore before CHIPSET init */
		pp3300_s5_good = *stored;
}

DECLARE_HOOK(HOOK_INIT, baseboard_prepare_power_signals, HOOK_PRIO_FIRST);

void pp3300_s5_good_on_cb(void)
{
	atomic_or(&pp3300_s5_good, 1);

	/* Disable this interrupt while it's asserted. */
	adc_disable_threshold_interrupt(PP3300_S5_GOOD_ON_THR_ID);
	/* Enable the voltage low interrupt. */
	adc_enable_threshold_interrupt(PP3300_S5_GOOD_OFF_THR_ID);

	/*
	 * TODO: Call power_signal_interrupt() with a fake GPIO in order for the
	 * chipset task to pick up the change in power sequencing signals.
	 */
}

/*
 * TODO: Implement hooks function to be called by power sequence,
 *       these functions involve exposing state of pp3300_s5_good.
 */

void pp3300_s5_good_off_cb(void)
{
	atomic_clear_bits(&pp3300_s5_good, 1);

	/* Disable this interrupt while it's asserted. */
	adc_disable_threshold_interrupt(PP3300_S5_GOOD_OFF_THR_ID);
	/* Enable the voltage high interrupt. */
	adc_enable_threshold_interrupt(PP3300_S5_GOOD_ON_THR_ID);

	/*
	 * TODO: Call power_signal_interrupt() with a fake GPIO in order for the
	 * chipset task to pick up the change in power sequencing signals.
	 */
}

static int baseboard_init(const struct device *device)
{
	struct adc_threshold_cfg cfg;
	ARG_UNUSED(device);

	cfg.channel_id = PP3300_S5_GOOD_ADC_CHANNEL;
	cfg.threshold_id = PP3300_S5_GOOD_ON_THR_ID;
	cfg.value = PP3300_S5_GOOD_ON_ADC_VOLTAGE;
	cfg.mode = ADC_THR_MODE_RISE;
	cfg.adc_threshold_cb = pp3300_s5_good_on_cb;
	adc_config_threshold_interrupt(&cfg);
	adc_enable_threshold_interrupt(PP3300_S5_GOOD_ON_THR_ID);

	cfg.threshold_id = PP3300_S5_GOOD_OFF_THR_ID;
	cfg.value = PP3300_S5_GOOD_OFF_ADC_VOLTAGE;
	cfg.mode = ADC_THR_MODE_FALL;
	cfg.adc_threshold_cb = pp3300_s5_good_off_cb;
	adc_config_threshold_interrupt(&cfg);
	adc_enable_threshold_interrupt(PP3300_S5_GOOD_OFF_THR_ID);

	return 0;
}
SYS_INIT(baseboard_init, POST_KERNEL, 52);
