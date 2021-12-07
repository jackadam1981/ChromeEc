/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <drivers/adc_cmp.h>
#include "gpio.h"
#include "hooks.h"
#include "adc.h"
#include "power.h"
#include "power/icelake.h"
#include "power/intel_x86.h"

#define pp330_on_dev DEVICE_DT_GET(DT_NODELABEL(cmp_pp3300_on))
#define pp330_off_dev DEVICE_DT_GET(DT_NODELABEL(cmp_pp3300_off))

static atomic_t pp3300_s5_pgood;

/* Store away pp300_s5_good status before sysjumps */
#define BASEBOARD_SYSJUMP_TAG   0x4242 /* BB */
#define BASEBOARD_HOOK_VERSION  1

static void pp3300_s5_pgood_preserve(void)
{
	system_add_jump_tag(BASEBOARD_SYSJUMP_TAG, BASEBOARD_HOOK_VERSION,
			    sizeof(pp3300_s5_pgood), &pp3300_s5_pgood);
}
DECLARE_HOOK(HOOK_SYSJUMP, pp3300_s5_pgood_preserve, HOOK_PRIO_DEFAULT);

static void baseboard_prepare_power_signals(void)
{
	const int *stored;
	int version, size;

	stored = (const int *)system_get_jump_tag(BASEBOARD_SYSJUMP_TAG,
						  &version, &size);
	if (stored && (version == BASEBOARD_HOOK_VERSION) &&
	   (size == sizeof(pp3300_s5_pgood)))
		/* Valid PP3300 status found, restore before CHIPSET init */
		pp3300_s5_pgood = *stored;
}
DECLARE_HOOK(HOOK_INIT, baseboard_prepare_power_signals, HOOK_PRIO_FIRST);

static void pp3300_s5_pgood_cb(const struct device *dev, void *data)
{
	if (dev == pp330_on_dev) {
		atomic_or(&pp3300_s5_pgood, 1);

		/* Disable this interrupt while it's asserted. */
		threshold_comparator_enable(pp330_on_dev, false);

		/* Enable the voltage low interrupt. */
		threshold_comparator_enable(pp330_off_dev, true);
	} else if (dev == pp330_off_dev) {
		atomic_clear_bits(&pp3300_s5_pgood, 1);

		/* Disable this interrupt while it's asserted. */
		threshold_comparator_enable(pp330_off_dev, false);

		/* Enable the voltage high interrupt. */
		threshold_comparator_enable(pp330_on_dev, true);
	}
	/*
	 * Call power_signal_interrupt() with a fake GPIO in order for the
	 * chipset task to pick up the change in power sequencing signals.
	 */
	power_signal_interrupt(GPIO_PG_EC_DSW_PWROK);
}

static int baseboard_init(const struct device *device)
{
	struct threshold_comparator_config t_config;

	t_config.comparison = THRESHOLD_GREATER;
	t_config.raw_threshold = 900;
	t_config.callback = pp3300_s5_pgood_cb;
	t_config.callback_data = NULL;

	threshold_comparator_setup(pp330_on_dev, &t_config);
	threshold_comparator_enable(pp330_on_dev, true);

	t_config.comparison = THRESHOLD_LESS_OR_EQUAL;
	t_config.raw_threshold = 200;
	threshold_comparator_setup(pp330_off_dev, &t_config);

	return 0;
}
SYS_INIT(baseboard_init, POST_KERNEL, 52);
