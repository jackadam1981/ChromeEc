/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <x86_non_dsx_adlp_pwrseq_sm.h>

LOG_MODULE_DECLARE(ap_pwrseq, 4);

/* Power signals list */
const struct power_signal_info power_signal_list[] = {
	{
		.net_name = GPIO_NET_NAME(PCH_EC_SLP_S0_L),
		.power_sig = X86_SLP_S0,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
			//???POWER_SIGNAL_DISABLE_AT_BOOT,
		.name = "SLP_S0_DEASSERTED",
	},
	{
		.net_name = GPIO_NET_NAME(PCH_EC_SLP_SUS_L),
		.power_sig = X86_SLP_SUS,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_SUS_DEASSERTED",
	},
	{
		.net_name = GPIO_NET_NAME(VR_PG_EC_RSMRST_ODL),
		.power_sig = X86_RSMRST_L_PGOOD,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "RSMRST_L_PGOOD",
	},
	{
		.net_name = GPIO_NET_NAME(VR_EC_DSW_PWROK),
		.power_sig = X86_DSW_PWROK,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "DSW_DPWROK",
	},
	{
		.net_name = GPIO_NET_NAME(VR_EC_ALL_SYS_PWRGD),
		.power_sig = X86_ALL_SYS_PGOOD,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "ALL_SYS_PWRGD",
	},
};
const int power_signal_gpio_count = ARRAY_SIZE(power_signal_list);

const struct power_signal_vw_info power_signal_vw_list[] = {
	{
		.vw_signal = ESPI_VWIRE_SIGNAL_SLP_S3,
		.power_sig = X86_SLP_S3,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_S3_DEASSERTED",
	},
	{
		.vw_signal = ESPI_VWIRE_SIGNAL_SLP_S4,
		.power_sig = X86_SLP_S4,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_S4_DEASSERTED",
	},
	{
		.vw_signal = ESPI_VWIRE_SIGNAL_SLP_S5,
		.power_sig = X86_SLP_S5,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_S5_DEASSERTED",
	},
};
const int power_signal_vw_count = ARRAY_SIZE(power_signal_vw_list);

void ap_off(void)
{
	/* TODO: This could be added as g3action handler */
	gpio_set_lvl(GPIO_NET_NAME(VCCST_PWRGD_OD), 0);
	gpio_set_lvl(GPIO_NET_NAME(PCH_PWROK), 0);
	gpio_set_lvl(GPIO_NET_NAME(EC_PCH_SYS_PWROK), 0);
	pwr_sm_set_state(SYS_POWER_STATE_G3);
}

/* Handle ALL_SYS_PWRGD signal
 * This will be overridden if the custom signal handler is needed
 */
int all_sys_pwrgd_handler(void)
{
	int sys_pg;
	int vccst_pg;

	/* TODO: Add condition for no power sequencer */
	sys_pg = gpio_get_lvl(GPIO_NET_NAME(VR_EC_ALL_SYS_PWRGD));

	/* Todo: Remove workaround for the retry
	 * without this change the system hits G3 as it detects
	 * ALL_SYS_PWRGD as 0 and then 1 as a glitch
	 */
#if 0
	while (sys_pg != 1 || retry < 2) {
		sys_pg = gpio_get_lvl(GPIO_NET_NAME(VR_EC_ALL_SYS_PWRGD));
		k_msleep(10);
		retry++;
	}
#endif
	if (sys_pg == 0) {
		LOG_ERR("PG_EC_ALL_SYS_PWRGD not ok\n");
		ap_off();
		return -1;
	}

	/* PG_EC_ALL_SYS_PWRGD is asserted, enable VCCST_PWRGD_OD. */

	vccst_pg = gpio_get_lvl(GPIO_NET_NAME(VCCST_PWRGD_OD));
	if (vccst_pg == 0) {
		k_msleep(VCCST_PWRGD_DELAY_MS);
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
	int timeout_ms = VRRDY_TIMEOUT_MS;
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
		k_msleep(PCH_PWROK_DELAY_MS);
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
		sys_pg = gpio_get_lvl(GPIO_NET_NAME(VR_EC_ALL_SYS_PWRGD));
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
	/* TODO: Separate different configs between alderlake variants */
	/* Handle DSW_PWROK passthrough */
	/* This is not needed for alderlake silego, guarded by CONFIG? */

	/* Check ALL_SYS_PWRGD and take action */
	ret = all_sys_pwrgd_handler();
	if (ret) {
		LOG_DBG("Not moving forward ALL_SYS %d\n", ret);
		return;
	}

	/* Send PCH_PWROK->SoC if conditions met */
	/* TODO: There is possibility of EC not needing to generate
	 * this as power sequencer may do it
	 */
	ret = generate_pch_pwrok_handler();
	if (ret) {
		LOG_DBG("Not moving forward 2 %d\n", ret);
		return;
	}

	/* SYS_PWROK may be optional and the delay must be
	 * configurable as it is variable with platform
	 */
	/* Send SYS_PWROK->SoC if conditions met */
	generate_sys_pwrok_handler(SYS_PWROK_DELAY_MS);

}

int intel_x86_get_pg_ec_dsw_pwrok(void)
{
	return gpio_get_lvl(GPIO_NET_NAME(VR_EC_DSW_PWROK));
}

void intel_x86_sys_reset_delay(void)
{
	/*
	 * Debounce time for SYS_RESET_L is 16 ms. Wait twice that period
	 * to be safe.
	 */
	k_msleep(32);
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

