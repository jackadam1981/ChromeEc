/* Copyright 2021 The Chromium OS Authors. All rights reserved.
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

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

/* PP3300_A_PGOOD voltages in milivolts */
#define PP3300_A_PGOOD_ON_ADC_VOLTAGE	2700
#define PP3300_A_PGOOD_OFF_ADC_VOLTAGE	600

#define pp330_on_dev DEVICE_DT_GET(DT_NODELABEL(cmp_pp3300_on))
#define pp330_off_dev DEVICE_DT_GET(DT_NODELABEL(cmp_pp3300_off))

atomic_t pp3300_a_pgood = 0;

const struct intel_x86_pwrok_signal pwrok_signal_assert_list[] = {
	{
		.gpio = GPIO_PG_EC_ALL_SYS_PWRGD,
	},
	{
		.gpio = GPIO_VCCST_PWRGD_OD,
		.delay_ms = 2,
	},
	{
		.gpio = GPIO_EC_AP_PCH_PWROK_OD,
	},
	{
		.gpio = GPIO_EC_PCH_SYS_PWROK,
	},
};
const int pwrok_signal_assert_count = ARRAY_SIZE(pwrok_signal_assert_list);

const struct intel_x86_pwrok_signal pwrok_signal_deassert_list[] = {
	/* No delays needed during S0 exit */
	{
		.gpio = GPIO_VCCST_PWRGD_OD,
	},
	{
		.gpio = GPIO_EC_AP_PCH_PWROK_OD,
	},
	{
		.gpio = GPIO_EC_PCH_SYS_PWROK,
	},
	/* Turn off the VCCIN rail last */
	{
		.gpio = GPIO_PG_EC_ALL_SYS_PWRGD,
	},
};
const int pwrok_signal_deassert_count = ARRAY_SIZE(pwrok_signal_deassert_list);

__override void board_after_rsmrst(int rsmrst)
{
	/*
	 * b:148688874: If RSMRST# is de-asserted, enable the pull-up on
	 * PG_PP1050_ST_OD.  It won't be enabled prior to this signal going high
	 * because the load switch for PP1050_ST cannot pull the PG low.  Once
	 * it's asserted, disable the pull up so we don't inidicate that the
	 * power is good before the rail is actually ready.
	 */
	int flags = rsmrst ? GPIO_PULL_UP : 0;

	flags |= GPIO_INT_BOTH;

	gpio_set_flags(GPIO_PG_PP1050_ST_S_OD, flags);
}

/* Store away PP300_A good status before sysjumps */
#define BASEBOARD_SYSJUMP_TAG   0x4242 /* BB */
#define BASEBOARD_HOOK_VERSION  1

static void pp3300_a_pgood_preserve(void)
{
	system_add_jump_tag(BASEBOARD_SYSJUMP_TAG, BASEBOARD_HOOK_VERSION,
			    sizeof(pp3300_a_pgood), &pp3300_a_pgood);
}
DECLARE_HOOK(HOOK_SYSJUMP, pp3300_a_pgood_preserve, HOOK_PRIO_DEFAULT);

static void baseboard_prepare_power_signals(void)
{
	const int *stored;
	int version, size;

	stored = (const int *)system_get_jump_tag(BASEBOARD_SYSJUMP_TAG,
						  &version, &size);
	if (stored && (version == BASEBOARD_HOOK_VERSION) &&
	   (size == sizeof(pp3300_a_pgood)))
		/* Valid PP3300 status found, restore before CHIPSET init */
		pp3300_a_pgood = *stored;

	/* Restore pull-up on PG_PP1050_ST_OD */
	if (system_jumped_to_this_image() &&
	    gpio_get_level(GPIO_PG_EC_RSMRST_ODL))
		board_after_rsmrst(1);
}
DECLARE_HOOK(HOOK_INIT, baseboard_prepare_power_signals, HOOK_PRIO_FIRST);

int extpower_is_present(void)
{
	/* Empty function to satisfy compiler and avoid build failure */
	return 0;
}

__override int intel_x86_get_pg_ec_dsw_pwrok(void)
{
	/*
	 * The PP3300_A rail is an input to generate DPWROK.  Assuming that
	 * power is good if voltage is at least 80% of nominal level.  We cannot
	 * read the ADC values during an interrupt, therefore, this power good
	 * value is updated via ADC threshold interrupts.
	 */
	return pp3300_a_pgood;
}

__override int power_signal_get_level(enum gpio_signal signal)
{
	if (signal == GPIO_PG_EC_DSW_PWROK)
		return intel_x86_get_pg_ec_dsw_pwrok();

	if (signal == GPIO_PG_EC_ALL_SYS_PWRGD)
		return intel_x86_get_pg_ec_all_sys_pwrgd();

	if (IS_ENABLED(CONFIG_HOSTCMD_ESPI)) {
		/* Check signal is from GPIOs or VWs */
		if (espi_signal_is_vw(signal))
			return espi_vw_get_wire(signal);
	}
	return gpio_get_level(signal);
}

void baseboard_all_sys_pgood_interrupt(enum gpio_signal signal)
{
	/*
	 * We need to deassert ALL_SYS_PGOOD within 200us of SLP_S3_L asserting.
	 * that is why we do this here instead of waiting for the chipset
	 * driver to.
	 * Early protos do not pull VCCST_PWRGD below Vil in hardware logic,
	 * so we need to do the same for this signal.
	 * Pull EN_VCCIO_EXT to LOW, which ensures VCCST_PWRGD remains LOW during
	 * SLP_S3_L assertion.
	 */
	if (!gpio_get_level(GPIO_PCH_SLP_S3_L)) {
		gpio_set_level(GPIO_PG_EC_ALL_SYS_PWRGD, 0);
		gpio_set_level(GPIO_EN_VCCIO_EXT, 0);
		gpio_set_level(GPIO_VCCST_PWRGD_OD, 0);
		gpio_set_level(GPIO_EC_AP_PCH_PWROK_OD, 0);
	}
	/* Now chain off to the normal power signal interrupt handler. */
	power_signal_interrupt(signal);
}

__override int intel_x86_get_pg_ec_all_sys_pwrgd(void)
{
	/*
	 * SLP_S3_L is a qualifying input signal to ALL_SYS_PWRGD logic.
	 * So ensure ALL_SYS_PWRGD remains LOW during SLP_S3_L assertion.
	 */
	if (!gpio_get_level(GPIO_PCH_SLP_S3_L))
		return 0;
	/*
	 * ALL_SYS_PWRGD is an AND of DRAM PGOOD, VCCST PGOOD, and VCCIO_EXT
	 * PGOOD.
	 */
	return gpio_get_level(GPIO_PG_PP1050_ST_S_OD) &&
		gpio_get_level(GPIO_PG_DRAM_OD) &&
		gpio_get_level(GPIO_PG_VCCIO_EXT_OD);
}

static void pp3300_a_pgood_cb(const struct device *dev, void *data)
{
	if (dev == pp330_on_dev) {
		atomic_or(&pp3300_a_pgood, 1);

		/* Disable this interrupt while it's asserted. */
		threshold_comparator_enable(pp330_on_dev, false);

		/* Enable the voltage low interrupt. */
		threshold_comparator_enable(pp330_off_dev, true);
	}
	else if (dev == pp330_off_dev) {
		atomic_clear_bits(&pp3300_a_pgood, 1);

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
	t_config.callback = pp3300_a_pgood_cb;
	t_config.callback_data = NULL;

	threshold_comparator_setup(pp330_on_dev, &t_config);
	threshold_comparator_enable(pp330_on_dev, true);

	t_config.comparison = THRESHOLD_LESS_OR_EQUAL;
	t_config.raw_threshold = 200;
	threshold_comparator_setup(pp330_off_dev, &t_config);

	return 0;
}
SYS_INIT(baseboard_init, POST_KERNEL, 52);
