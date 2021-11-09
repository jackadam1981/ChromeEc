/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "hooks.h"
#include "adc.h"
#include "power.h"
#include "power/icelake.h"
#include "power/intel_x86.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

#define PP3300_ON_THRESHOLD_ID		0
#define PP3300_OFF_THRESHOLD_ID		1

int pp3_pgood = 0;

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

int extpower_is_present(void)
{
	/* Empty function to satisfy compiler and avoid build failure */
	return 0;
}

__override int intel_x86_get_pg_ec_dsw_pwrok(void)
{
	return pp3_pgood;
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

void pp3300_pgood_on_cb(void)
{
	pp3_pgood = 1;
	adc_disable_threshold_interrupt(PP3300_ON_THRESHOLD_ID);
	adc_enable_threshold_interrupt(PP3300_OFF_THRESHOLD_ID);
	power_signal_interrupt(GPIO_PG_EC_DSW_PWROK);
}

void pp3300_pgood_off_cb(void)
{
	pp3_pgood = 0;
	adc_disable_threshold_interrupt(PP3300_OFF_THRESHOLD_ID);
	adc_enable_threshold_interrupt(PP3300_ON_THRESHOLD_ID);
	power_signal_interrupt(GPIO_PG_EC_DSW_PWROK);
}

static int baseboard_init(const struct device *device)
{
	struct adc_threshold_cfg cfg;
	ARG_UNUSED(device);

	cfg.channel_id = 9;
	cfg.threshold_id = PP3300_ON_THRESHOLD_ID;
	cfg.value = 1000;
	cfg.mode = ADC_THR_MODE_RISE;
	cfg.adc_threshold_cb = pp3300_pgood_on_cb;
	adc_config_threshold_interrupt(&cfg);
	adc_enable_threshold_interrupt(PP3300_ON_THRESHOLD_ID);

	cfg.threshold_id = PP3300_OFF_THRESHOLD_ID;
	cfg.value = 800;
	cfg.mode = ADC_THR_MODE_FALL;
	cfg.adc_threshold_cb = pp3300_pgood_off_cb;
	adc_config_threshold_interrupt(&cfg);

	return 0;
}
SYS_INIT(baseboard_init, POST_KERNEL, 52);
