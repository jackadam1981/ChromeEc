/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <x86_non_dsx_adlp_pwrseq_sm.h>

LOG_MODULE_DECLARE(ap_pwrseq, 4);

static struct chipset_pwrseq_config chip_cfg;

/* Power sequencing GPIOs */
struct gpio_config power_seq_gpios[] = {
	{
		POWER_SEQ_GPIO(PCH_EC_SLP_SUS_L),
	},
	{
		POWER_SEQ_GPIO(PCH_EC_SLP_S0_L),
	},
	{
		POWER_SEQ_GPIO(PCH_EC_SLP_S3_L),
	},
	{
		POWER_SEQ_GPIO(VR_PG_EC_RSMRST_ODL),
	},
	{
		POWER_SEQ_GPIO(VR_EC_ALL_SYS_PWRGD),
	},
#if POWER_SEQ_GPIO_PRESENT(VR_EC_DSW_PWROK)
	{
		POWER_SEQ_GPIO(VR_EC_DSW_PWROK),
	},
#endif
	{
		POWER_SEQ_GPIO(EC_PCH_RSMRST_L),
	},
#if POWER_SEQ_GPIO_PRESENT(EC_PCH_DSW_PWROK)
	{
		POWER_SEQ_GPIO(EC_PCH_DSW_PWROK),
	},
#endif
#if POWER_SEQ_GPIO_PRESENT(EC_PCH_SYS_PWROK)
	{
		POWER_SEQ_GPIO(EC_PCH_SYS_PWROK),
	},
#endif
	{
		POWER_SEQ_GPIO(EC_PCH_PWR_BTN_ODL),
	},
#if POWER_SEQ_GPIO_PRESENT(EC_VR_EN_PP5000_A)
	{
		POWER_SEQ_GPIO(EC_VR_EN_PP5000_A),
	},
#endif
#if POWER_SEQ_GPIO_PRESENT(IMVP9_VRRDY_OD)
	{
		POWER_SEQ_GPIO(IMVP9_VRRDY_OD),
	},
#endif
#if POWER_SEQ_GPIO_PRESENT(VCCST_PWRGD_OD)
	{
		POWER_SEQ_GPIO(VCCST_PWRGD_OD),
	},
#endif
	{
		POWER_SEQ_GPIO(PCH_PWROK),
	},
	{
		POWER_SEQ_GPIO(SYS_RESET_L),
	},
};

const int power_seq_gpios_count = ARRAY_SIZE(power_seq_gpios);

void ap_off(void)
{
	/* TODO: This could be added as g3action handler */
	gpio_set_lvl(GPIO_NET_NAME(VCCST_PWRGD_OD), 0);
	gpio_set_lvl(GPIO_NET_NAME(PCH_PWROK), 0);
	gpio_set_lvl(GPIO_NET_NAME(EC_PCH_SYS_PWROK), 0);
}

/* This should be overridden if there is no power sequencer chip */
__attribute__((weak)) int intel_x86_get_pg_ec_all_sys_pwrgd(void)
{
	return gpio_get_lvl(GPIO_NET_NAME(VR_EC_ALL_SYS_PWRGD));
}

/* Handle ALL_SYS_PWRGD signal
 * This will be overridden if the custom signal handler is needed
 */
int all_sys_pwrgd_handler(void)
{
	int sys_pg;
	int vccst_pg;
	int retry = 0;

	/* TODO: Add condition for no power sequencer */
	sys_pg = intel_x86_get_pg_ec_all_sys_pwrgd();

	/* Todo: Remove workaround for the retry
	 * without this change the system hits G3 as it detects
	 * ALL_SYS_PWRGD as 0 and then 1 as a glitch
	 */
	while (sys_pg != 1 || retry < 2) {
		sys_pg = intel_x86_get_pg_ec_all_sys_pwrgd();
		k_msleep(10);
		retry++;
	}

	if (sys_pg == 0) {
		LOG_ERR("PG_EC_ALL_SYS_PWRGD not ok\n");
		ap_off();
		return -1;
	}

	/* PG_EC_ALL_SYS_PWRGD is asserted, enable VCCST_PWRGD_OD. */

	vccst_pg = gpio_get_lvl(GPIO_NET_NAME(VCCST_PWRGD_OD));
	if (vccst_pg == 0) {
		k_msleep(chip_cfg.vccst_pwrgd_delay_ms);
		gpio_set_lvl(GPIO_NET_NAME(VCCST_PWRGD_OD), 1);
	}
	return 0;
}

/*
 * We have asserted VCCST_PWRGO_OD, now wait for the IMVP9.1
 * to assert IMVP9_VRRDY_OD.
 *
 * Returns state of VRRDY.
 */

static int wait_for_vrrdy(void)
{
	int timeout_ms = chip_cfg.vrrdy_timeout_ms;
	int vrrdy;

	for (; timeout_ms > 0; --timeout_ms) {
		vrrdy = gpio_get_lvl(GPIO_NET_NAME(IMVP9_VRRDY_OD));
		if (vrrdy != 0)
			return 1;
		k_msleep(1);
	}
	return 0;
}

/* PCH_PWROK to PCH from EC */
int generate_pch_pwrok_handler(void)
{
	int pch_pok;

	/* Enable PCH_PWROK, gated by VRRDY. */
	pch_pok = gpio_get_lvl(GPIO_NET_NAME(PCH_PWROK));
	if (pch_pok == 0) {
		if (wait_for_vrrdy() == 0) {
			LOG_DBG("Timed out waiting for VRRDY, "
				"shutting AP off!");
			ap_off();
			return -1;
		}
		k_msleep(chip_cfg.pch_pwrok_delay_ms);
		gpio_set_lvl(GPIO_NET_NAME(PCH_PWROK), 1);
		LOG_DBG("Set PCH_PWROK\n");
	}

	return 0;
}

/* Generate SYS_PWROK->SOC if needed by system */
void generate_sys_pwrok_handler(int delay)
{
	int sys_pok;
	int sys_pg;

	/* Enable PCH_SYS_PWROK. */
	sys_pok = gpio_get_lvl(GPIO_NET_NAME(EC_PCH_SYS_PWROK));
	if (sys_pok == 0) {
		k_msleep(delay);
		/* Check if we lost power while waiting. */
		sys_pg = intel_x86_get_pg_ec_all_sys_pwrgd();
		if (sys_pg == 0) {
			LOG_DBG("PG_EC_ALL_SYS_PWRGD deasserted, "
				"shutting AP off!");
			ap_off();
			return;
		}
		gpio_set_lvl(GPIO_NET_NAME(EC_PCH_SYS_PWROK), 1);
		/* PCH will now release PLT_RST */
	}
}

/* Chipset specific power state machine handler */

/* TODO: Separate with and without power sequencer logic here */

void s0_action_handler(void)
{
	int ret;

	/* Handle DSW_PWROK passthrough */
	/* This is not needed for alderlake silego, guarded by CONFIG? */

	/* Check ALL_SYS_PWRGD and take action */
	ret = all_sys_pwrgd_handler();
	if (ret) {
		LOG_DBG("ALL_SYS_PWRGD handling failed err= %d\n", ret);
		return;
	}

	/* Send PCH_PWROK->SoC if conditions met */
	/* TODO: There is possibility of EC not needing to generate
	 * this as power sequencer may do it
	 */
	ret = generate_pch_pwrok_handler();
	if (ret) {
		LOG_DBG("PCH_PWROK handling failed err=%d\n", ret);
		return;
	}

	/* SYS_PWROK may be optional and the delay must be
	 * configurable as it is variable with platform
	 */
	/* Send SYS_PWROK->SoC if conditions met */
	generate_sys_pwrok_handler(chip_cfg.sys_pwrok_delay_ms);
}

/* This should be overridden if there is no power sequencer chip */
__attribute__((weak)) int intel_x86_get_pg_ec_dsw_pwrok(void)
{
	return gpio_get_lvl(GPIO_NET_NAME(VR_EC_DSW_PWROK));
}

void intel_x86_sys_reset_delay(void)
{
	/*
	 * Debounce time for SYS_RESET_L is 16 ms. Wait twice that period
	 * to be safe.
	 */
	k_msleep(chip_cfg.sys_reset_delay_ms);
}

void enable_power_rail(const char *net_name, int enable)
{
	gpio_set_lvl(net_name, enable);
}

void g3s5_action_handler(void)
{
	enable_power_rail(GPIO_NET_NAME(EC_VR_EN_PP5000_A), 1);
}

void init_chipset_pwr_seq_state(void)
{
	/* TODO: Read from device tree */
	/* Override any common cfg values here */
	chip_cfg.pch_pwrok_delay_ms = 2;
	chip_cfg.sys_pwrok_delay_ms = 45;

	chip_cfg.vccst_pwrgd_delay_ms = 2;
	chip_cfg.vrrdy_timeout_ms = 50;
	chip_cfg.sys_reset_delay_ms = 32;
}

enum power_states_ndsx chipset_pwr_sm_run(enum power_states_ndsx curr_state)
{
/* Add chipset specific state handling if any */
	switch (curr_state) {
	case SYS_POWER_STATE_G3S5:
		g3s5_action_handler();
		break;
	case SYS_POWER_STATE_S5:
		break;
	case SYS_POWER_STATE_S0:
		s0_action_handler();
		break;
	default:
		break;
	}
	return curr_state;
}
