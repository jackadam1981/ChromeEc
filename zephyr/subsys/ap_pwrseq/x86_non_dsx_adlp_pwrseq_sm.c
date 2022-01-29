/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <x86_non_dsx_adlp_pwrseq_sm.h>

LOG_MODULE_DECLARE(ap_pwrseq, 4);

static const struct chipset_pwrseq_config chip_cfg = {
	.pch_pwrok_delay_ms = DT_INST_PROP(0, pch_pwrok_delay),
	.sys_pwrok_delay_ms = DT_INST_PROP(0, sys_pwrok_delay),
	.vccst_pwrgd_delay_ms = DT_INST_PROP(0, vccst_pwrgd_delay),
	.vrrdy_timeout_ms = DT_INST_PROP(0, vrrdy_timeout),
	.sys_reset_delay_ms = DT_INST_PROP(0, sys_reset_delay),
	.all_sys_pwrgd_timeout = DT_INST_PROP(0, all_sys_pwrgd_timeout),
};

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

struct gpio_interrupt_config power_seq_intr_gpios[] = {
	{
		POWER_SEQ_INTR_GPIO(PCH_EC_SLP_S0_L),
		/* Disable interrupt at boot up */
		.disable_at_boot = true,
	},
	{
		POWER_SEQ_INTR_GPIO(PCH_EC_SLP_SUS_L),
		.disable_at_boot = false,
	},
	{
		POWER_SEQ_INTR_GPIO(VR_PG_EC_RSMRST_ODL),
		.disable_at_boot = false,
	},
#if POWER_SEQ_GPIO_PRESENT(VR_EC_DSW_PWROK)
	{
		POWER_SEQ_INTR_GPIO(VR_EC_DSW_PWROK),
		.disable_at_boot = false,
	},
#endif
	{
		POWER_SEQ_INTR_GPIO(PCH_EC_SLP_S3_L),
		.disable_at_boot = false,
	},
	{
		POWER_SEQ_INTR_GPIO(VR_EC_ALL_SYS_PWRGD),
		.disable_at_boot = false,
	},
};

const int power_seq_intr_gpios_count = ARRAY_SIZE(power_seq_intr_gpios);

#if (DT_NODE_EXISTS(POWER_SIGNALS_LIST_NODE))
const struct power_signal_gpio_info power_signal_gpio_list[] = {
	DT_FOREACH_CHILD(
		POWER_SIGNALS_LIST_NODE,
		GEN_GPIO_POWER_SIGNAL_ENTRY_COMMA)
};

const int power_signal_gpio_count = ARRAY_SIZE(power_signal_gpio_list);

const struct power_signal_vw_info power_signal_vw_list[] = {
	DT_FOREACH_CHILD(
		POWER_SIGNALS_LIST_NODE,
		GEN_VW_POWER_SIGNAL_ENTRY_COMMA)
};

const int power_signal_vw_count = ARRAY_SIZE(power_signal_vw_list);
#endif /* DT_NODE_EXISTS(POWER_SIGNALS_LIST_NODE) */

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

	/* TODO: Add condition for no power sequencer */
	k_msleep(chip_cfg.all_sys_pwrgd_timeout);
	sys_pg = intel_x86_get_pg_ec_all_sys_pwrgd();

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

void chipset_reset(enum chipset_shutdown_reason reason)
{
	/*
	 * Irrespective of cold_reset value, always toggle SYS_RESET_L to
	 * perform a chipset reset. RCIN# which was used earlier to trigger
	 * a warm reset is known to not work in certain cases where the CPU
	 * is in a bad state (crbug.com/721853).
	 *
	 * The EC cannot control warm vs cold reset of the chipset using
	 * SYS_RESET_L; it's more of a request.
	 */
	LOG_DBG("%s: %d", __func__, reason);

	/*
	 * Toggling SYS_RESET_L will not have any impact when it's already
	 * low (i,e. Chipset is in reset state).
	 */
	if (gpio_get_lvl(GPIO_NET_NAME(SYS_RESET_L)) == 0) {
		LOG_DBG("Chipset is in reset state");
		return;
	}

	gpio_set_lvl(GPIO_NET_NAME(SYS_RESET_L), 0);
	intel_x86_sys_reset_delay();
	gpio_set_lvl(GPIO_NET_NAME(SYS_RESET_L), 1);
}

void chipset_force_shutdown(enum chipset_shutdown_reason reason)
{
	int timeout_ms = 50;

	/* TODO: below
	 * report_ap_reset(reason);
	 */

	/* Turn off RMSRST_L  to meet tPCH12 */
	gpio_set_lvl(GPIO_NET_NAME(EC_PCH_RSMRST_L), 0);

	/* Turn off S5 rails */
	gpio_set_lvl(GPIO_NET_NAME(EC_VR_EN_PP5000_A), 0);

	/*
	 * TODO(b/179519791): Replace this wait with
	 * power_wait_signals_timeout()
	 */
	/* Now wait for DSW_PWROK and  RSMRST_ODL to go away. */
	while (intel_x86_get_pg_ec_dsw_pwrok() &&
			gpio_get_lvl(GPIO_NET_NAME(VR_PG_EC_RSMRST_ODL)) &&
			(timeout_ms > 0)) {
		k_msleep(1);
		timeout_ms--;
	};

	if (!timeout_ms)
		LOG_DBG("DSW_PWROK or RSMRST_ODL didn't go low!  Assuming G3.");
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
	/* Do nothing */
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
