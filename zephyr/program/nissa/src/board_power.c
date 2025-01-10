/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio/gpio.h"
#include "gpio/gpio_int.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "system_boot_time.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#ifndef CONFIG_AP_PWRSEQ_DRIVER
#include <ap_power/ap_power.h>
#include <ap_power/ap_power_events.h>
#else
#include "ap_power/ap_pwrseq_sm.h"
#endif
#include <ap_power/ap_power_interface.h>
#include <ap_power/ap_pwrseq.h>
#include <ap_power_override_functions.h>
#include <power_signals.h>
#include <x86_power_signals.h>

LOG_MODULE_DECLARE(ap_pwrseq, LOG_LEVEL_INF);

/******************************************************************************/
/*
 * PWROK signal configuration, see the PWROK Generation Flow Diagram in the
 * Jasper Lake Platform Design Guide for the list of potential signals.
 *
 * Dedede boards use this PWROK sequence:
 *	GPIO_ALL_SYS_PWRGD - turns on VCCIN rail
 *	GPIO_EC_AP_VCCST_PWRGD_OD - asserts VCCST_PWRGD to AP, requires 2ms
 *		delay from VCCST stable to meet the tCPU00 platform sequencing
 *		timing
 *	GPIO_EC_AP_PCH_PWROK_OD - asserts PMC_PCH_PWROK to the AP. Note that
 *		PMC_PCH_PWROK is also gated by the IMVP9_VRRDY_OD output from
 *		the VCCIN voltage rail controller.
 *	GPIO_EC_AP_SYS_PWROK - asserts PMC_SYS_PWROK to the AP
 *
 * Both PMC_PCH_PWROK and PMC_SYS_PWROK signals must both be asserted before
 * the Jasper Lake SoC deasserts PMC_RLTRST_N. The platform may deassert
 * PMC_PCH_PWROK and PMC_SYS_PWROK in any order to optimize overall boot
 * latency.
 */

/*
 * Pass through the state of the ALL_SYS_PWRGD input to all the PWROK outputs
 * defined by the board.
 */
void all_sys_pwrgd_pass_thru(void)
{
	int all_sys_pwrgd_in = !!(power_signal_get(PWR_ALL_SYS_PWRGD));

	if (all_sys_pwrgd_in) {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_all_sys_pwrgd),
				all_sys_pwrgd_in);
		k_msleep(2);
		gpio_pin_set_dt(
			GPIO_DT_FROM_NODELABEL(gpio_ec_soc_vccst_pwrgd_od),
			all_sys_pwrgd_in);
		gpio_pin_set_dt(
			GPIO_DT_FROM_NODELABEL(gpio_ec_soc_pch_pwrok_od),
			all_sys_pwrgd_in);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_soc_sys_pwrok),
				all_sys_pwrgd_in);
	} else {
		gpio_pin_set_dt(
			GPIO_DT_FROM_NODELABEL(gpio_ec_soc_vccst_pwrgd_od),
			all_sys_pwrgd_in);
		gpio_pin_set_dt(
			GPIO_DT_FROM_NODELABEL(gpio_ec_soc_pch_pwrok_od),
			all_sys_pwrgd_in);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_soc_sys_pwrok),
				all_sys_pwrgd_in);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_all_sys_pwrgd),
				all_sys_pwrgd_in);
	}
}

void baseboard_all_sys_pgood_interrupt(enum gpio_signal signal)
{
	int slp_s3_lvl = gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_slp_s3_l));
	/*
	 * We need to deassert ALL_SYS_PGOOD within 200us of SLP_S3_L asserting.
	 * that is why we do this here instead of waiting for the chipset
	 * driver to.
	 * Early protos do not pull VCCST_PWRGD below Vil in hardware logic,
	 * so we need to do the same for this signal.
	 * Pull EN_VCCIO_EXT to LOW, which ensures VCCST_PWRGD remains LOW
	 * during SLP_S3_L assertion.
	 */
	/* dedede power sequence in baseboard_all_sys_pgood_interrupt */
	if (slp_s3_lvl == 0) {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_all_sys_pwrgd), 0);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_vccio_ext), 0);
		gpio_pin_set_dt(
			GPIO_DT_FROM_NODELABEL(gpio_ec_soc_vccst_pwrgd_od), 0);
		gpio_pin_set_dt(
			GPIO_DT_FROM_NODELABEL(gpio_ec_soc_pch_pwrok_od), 0);
	}
	/* Now chain off to the normal power signal interrupt handler. */
	LOG_INF("slp_s3_l=%d", slp_s3_lvl);
	power_signal_set(PWR_SLP_S3, !slp_s3_lvl);
	//	power_signal_interrupt(signal);
}

void board_after_rsmrst(int rsmrst)
{
	/*
	 * b:148688874: If RSMRST# is de-asserted, enable the pull-up on
	 * PG_PP1050_ST_OD.  It won't be enabled prior to this signal going high
	 * because the load switch for PP1050_ST cannot pull the PG low.  Once
	 * it's asserted, disable the pull up so we don't inidicate that the
	 * power is good before the rail is actually ready.
	 */
	int flags = rsmrst ? GPIO_PULL_UP : 0;

	flags |= GPIO_INT_EDGE_BOTH;

	gpio_pin_configure_dt(GPIO_DT_FROM_NODELABEL(gpio_pg_pp1050_st_od),
			      flags);
}

static void baseboard_prepare_power_signals(void)
{
	//	const int *stored;
	//	int version, size;

	//	stored = (const int *)system_get_jump_tag(BASEBOARD_SYSJUMP_TAG,
	//						  &version, &size);
	//	if (stored && (version == BASEBOARD_HOOK_VERSION) &&
	//	    (size == sizeof(pp3300_a_pgood)))
	/* Valid PP3300 status found, restore before CHIPSET init */
	//		pp3300_a_pgood = *stored;

	/* Restore pull-up on PG_PP1050_ST_OD */
	if (system_jumped_to_this_image() && power_signal_get(PWR_RSMRST_PWRGD))
		board_after_rsmrst(1);
}
DECLARE_HOOK(HOOK_INIT, baseboard_prepare_power_signals, HOOK_PRIO_FIRST);

#define X86_NON_DSX_ADLP_NONPWRSEQ_FORCE_SHUTDOWN_TO_MS 5

#ifndef CONFIG_AP_PWRSEQ_DRIVER
test_export_static bool s0_stable;
#endif

static void generate_ec_soc_dsw_pwrok_handler(int delay)
{
	int in_sig_val = power_signal_get(PWR_DSW_PWROK);

	if (in_sig_val != power_signal_get(PWR_EC_SOC_DSW_PWROK)) {
		if (in_sig_val)
			k_msleep(delay);
		power_signal_set(PWR_EC_SOC_DSW_PWROK, 1);
	}
}

void board_ap_power_force_shutdown(void)
{
	int timeout_ms = X86_NON_DSX_ADLP_NONPWRSEQ_FORCE_SHUTDOWN_TO_MS;

#ifndef CONFIG_AP_PWRSEQ_DRIVER
	if (s0_stable) {
		/* Enable these power signals in case of sudden shutdown */
		power_signal_enable(PWR_DSW_PWROK);
	}
#endif

	power_signal_set(PWR_EC_SOC_DSW_PWROK, 0);
	power_signal_set(PWR_EC_PCH_RSMRST, 1);

	while (power_signal_get(PWR_RSMRST_PWRGD) == 1 &&
	       power_signal_get(PWR_SLP_SUS) == 0 && timeout_ms > 0) {
		k_msleep(1);
		timeout_ms--;
	}

	/* LCOV_EXCL_START messages are informational only */
	if (power_signal_get(PWR_SLP_SUS) == 0) {
		LOG_WRN("SLP_SUS is not asserted! Assuming G3");
	}
	if (power_signal_get(PWR_RSMRST_PWRGD) == 1) {
		LOG_WRN("RSMRST_PWRGD is asserted! Assuming G3");
	}
	/* LCOV_EXCL_STOP */

	power_signal_set(PWR_EN_PP3300_A, 0);

	power_signal_set(PWR_EN_PP5000_A, 0);

	timeout_ms = X86_NON_DSX_ADLP_NONPWRSEQ_FORCE_SHUTDOWN_TO_MS;
	while (power_signal_get(PWR_DSW_PWROK) && timeout_ms > 0) {
		k_msleep(1);
		timeout_ms--;
	};

	/* LCOV_EXCL_START informational */
	if (power_signal_get(PWR_DSW_PWROK))
		LOG_WRN("DSW_PWROK didn't go low!  Assuming G3.");
	/* LCOV_EXCL_STOP */

	power_signal_disable(PWR_DSW_PWROK);
#ifndef CONFIG_AP_PWRSEQ_DRIVER
	s0_stable = false;
#endif
}

#ifndef CONFIG_AP_PWRSEQ_DRIVER
void board_ap_power_action_g3_s5(void)
{
	power_signal_enable(PWR_DSW_PWROK);

	LOG_DBG("Turning on PWR_EN_PP5000_A and PWR_EN_PP3300_A");
	power_signal_set(PWR_EN_PP5000_A, 1);
	power_signal_set(PWR_EN_PP3300_A, 1);

	update_ap_boot_time(ARAIL);
	power_wait_signals_on_timeout(IN_PGOOD_ALL_CORE,
				      AP_PWRSEQ_DT_VALUE(wait_signal_timeout));

	generate_ec_soc_dsw_pwrok_handler(AP_PWRSEQ_DT_VALUE(dsw_pwrok_delay));
	s0_stable = false;
}

void board_ap_power_action_s3_s0(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_vccio_ext), 1);
	s0_stable = false;
}

void board_ap_power_action_s0_s3(void)
{
	power_signal_enable(PWR_DSW_PWROK);
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_vccio_ext), 0);
	s0_stable = false;
}

void board_ap_power_action_s0(void)
{
	if (s0_stable) {
		return;
	}
	LOG_INF("Reaching S0");
	power_signal_disable(PWR_DSW_PWROK);
	s0_stable = true;
}

int board_ap_power_assert_pch_power_ok(void)
{
	/* Pass though PCH_PWROK */
	if (power_signal_get(PWR_PCH_PWROK) == 0) {
		k_msleep(AP_PWRSEQ_DT_VALUE(pch_pwrok_delay));
		power_signal_set(PWR_PCH_PWROK, 1);
	}

	return 0;
}

bool board_ap_power_check_power_rails_enabled(void)
{
	return power_signal_get(PWR_EN_PP3300_A) &&
	       power_signal_get(PWR_EN_PP5000_A) &&
	       power_signal_get(PWR_EC_SOC_DSW_PWROK);
}
#else
#ifndef CONFIG_EMUL_AP_PWRSEQ_DRIVER
/* This is called by AP Power Sequence driver only when AP exits S0 or S0IX */
static void board_ap_power_cb(const struct device *dev,
			      const enum ap_pwrseq_state entry,
			      const enum ap_pwrseq_state exit)
{
	if (entry == AP_POWER_STATE_S0IX) {
		/* Avoid enabling signals when entering S0IX */
		return;
	}
	power_signal_enable(PWR_DSW_PWROK);
}

static int board_ap_power_init(void)
{
	const struct device *ap_pwrseq_dev = ap_pwrseq_get_instance();
	static struct ap_pwrseq_state_callback exit_cb = {
		.cb = board_ap_power_cb,
		.states_bit_mask =
			(BIT(AP_POWER_STATE_S0) | BIT(AP_POWER_STATE_S0IX)),
	};

	ap_pwrseq_register_state_exit_callback(ap_pwrseq_dev, &exit_cb);

	return 0;
}
SYS_INIT(board_ap_power_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
#endif /* CONFIG_EMUL_AP_PWRSEQ_DRIVER */

static int board_ap_power_g3_entry(void *data)
{
	board_ap_power_force_shutdown();

	return 0;
}

static int board_ap_power_g3_run(void *data)
{
	if (ap_pwrseq_sm_is_event_set(data, AP_PWRSEQ_EVENT_POWER_STARTUP)) {
		power_signal_enable(PWR_DSW_PWROK);

		LOG_INF("Turning on PWR_EN_PP5000_A and PWR_EN_PP3300_A");

		power_signal_set(PWR_EN_PP5000_A, 1);
		power_signal_set(PWR_EN_PP3300_A, 1);

		power_wait_signals_on_timeout(
			POWER_SIGNAL_MASK(PWR_DSW_PWROK),
			AP_PWRSEQ_DT_VALUE(wait_signal_timeout));
	}

	generate_ec_soc_dsw_pwrok_handler(AP_PWRSEQ_DT_VALUE(dsw_pwrok_delay));

	if (power_signal_get(PWR_EN_PP5000_A) &&
	    power_signal_get(PWR_EN_PP3300_A) &&
	    power_signal_get(PWR_EC_SOC_DSW_PWROK)) {
		return 0;
	}

	return 1;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_G3, board_ap_power_g3_entry,
			  board_ap_power_g3_run, NULL);

static int board_ap_power_s0_run(void *data)
{
	if (power_signal_get(PWR_ALL_SYS_PWRGD) &&
	    power_signal_get(PWR_VCCST_PWRGD) &&
	    power_signal_get(PWR_PCH_PWROK) &&
	    power_signal_get(PWR_EC_PCH_SYS_PWROK)) {
		/*
		 * Make sure all the signals checked inside the condition are
		 * asserted before disabling these two power signals.
		 */
		power_signal_disable(PWR_DSW_PWROK);
	}

	return 0;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_S0, NULL, board_ap_power_s0_run, NULL);
#endif /* CONFIG_AP_PWRSEQ_DRIVER */

int board_power_signal_get(enum power_signal signal)
{
	switch (signal) {
	default:
		LOG_ERR("Unknown signal for board get: %d", signal);
		return -EINVAL;

	case PWR_SLP_S3:
		return !gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_slp_s3_l));

	case PWR_ALL_SYS_PWRGD:
		/*
		 * All system power is good.
		 * Checks that PWR_SLP_S3 is off, and
		 * the GPIO signal for all power good is set,
		 * and that the 1.05 volt line is ready.
		 */
		if (power_signal_get(PWR_SLP_S3)) {
			return 0;
		}
		if (!gpio_pin_get_dt(
			    GPIO_DT_FROM_NODELABEL(gpio_pg_pp1050_st_od))) {
			return 0;
		}
		if (!gpio_pin_get_dt(
			    GPIO_DT_FROM_NODELABEL(gpio_pg_pp1050_mem_s3_od))) {
			return 0;
		}
		if (!gpio_pin_get_dt(
			    GPIO_DT_FROM_NODELABEL(gpio_pg_vccio_ext_od))) {
			return 0;
		}
		return 1;
	}
}

int board_power_signal_set(enum power_signal signal, int value)
{
	return -EINVAL;
}

static void board_init(void)
{
	/*
	 * Enable USB-C interrupts.
	 */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_slp_s3_l));
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_INIT_CHIPSET);

/*
 * As a soft power signal, PWR_ALL_SYS_PWRGD will never wake the power state
 * machine on its own. Since its value depends on the state of
 * gpio_all_sys_pwrgd, wake the state machine to re-evaluate ALL_SYS_PWRGD
 * anytime the input changes.
 */
void board_all_sys_pwrgd_interrupt(const struct device *unused_device,
				   struct gpio_callback *unused_callback,
				   gpio_port_pins_t unused_pin)
{
#ifndef CONFIG_AP_PWRSEQ_DRIVER
	ap_pwrseq_wake();
#else
	ap_pwrseq_post_event(ap_pwrseq_get_instance(),
			     AP_PWRSEQ_EVENT_POWER_SIGNAL);
#endif
}

static int board_config_pwrgd_interrupt(void)
{
	const struct gpio_dt_spec *const pwrgd_gpio =
		GPIO_DT_FROM_NODELABEL(gpio_all_sys_pwrgd);
	static struct gpio_callback cb;
	int rv;

	gpio_init_callback(&cb, board_all_sys_pwrgd_interrupt,
			   BIT(pwrgd_gpio->pin));
	gpio_add_callback(pwrgd_gpio->port, &cb);

	rv = gpio_pin_interrupt_configure_dt(pwrgd_gpio, GPIO_INT_EDGE_BOTH);
	__ASSERT(rv == 0,
		 "all_sys_pwrgd interrupt configuration returned error %d", rv);

	return 0;
}
SYS_INIT(board_config_pwrgd_interrupt, APPLICATION,
	 CONFIG_APPLICATION_INIT_PRIORITY);
