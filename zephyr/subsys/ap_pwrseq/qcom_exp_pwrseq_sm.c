/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_power/ap_pwrseq.h"
#include "ap_power/ap_pwrseq_sm.h"
#include "chipset.h"
#include "gpio.h"
#include "hooks.h"
#include "power.h"
#include "power_signals.h"
#include "system.h"
#include "task.h"
#include "util.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

/* Power signal masks */
#define IN_POWER_GOOD POWER_SIGNAL_MASK(PWR_POWER_GOOD)
#define IN_AP_RST_L POWER_SIGNAL_MASK(PWR_AP_RST_L)
#define IN_PS_HOLD POWER_SIGNAL_MASK(PWR_PS_HOLD)
#define IN_SUSPEND POWER_SIGNAL_MASK(PWR_AP_SUSPEND)

/* PMIC power sequence timeouts */
#define PMIC_POWER_AP_RESPONSE_TIMEOUT_MS 350
#define FORCE_OFF_RESPONSE_TIMEOUT_MS 4000
#define SYSTEM_POWER_ON_DELAY_MS 300
#define PMIC_POWER_OFF_DELAY_MS 150
#define PMIC_RESIN_PULSE_LENGTH_MS 20

struct qcom_exp_data {
	enum power_on_event_t power_on_reason;
	bool auto_power_on;
	bool power_button_was_pressed;
	uint64_t power_off_deadline;
};

static struct qcom_exp_data qcom_data;

/* Helper functions */

static int wait_pmic_pwron(int enable, int timeout_ms)
{
	return power_wait_mask_signals_timeout(
		IN_POWER_GOOD, enable ? IN_POWER_GOOD : 0, timeout_ms);
}

static void set_system_power(int enable)
{
	board_set_switchcap_power(enable);

	if (enable) {
		k_msleep(SYSTEM_POWER_ON_DELAY_MS);
	} else {
		wait_pmic_pwron(0, FORCE_OFF_RESPONSE_TIMEOUT_MS);
	}
}

static int set_pmic_pwron(int enable, uint8_t event)
{
	int ret;

	if (enable && event == POWER_ON_BY_AC_ON) {
		passthru_ac_on_to_pmic();
		ret = wait_pmic_pwron(enable,
				      PMIC_POWER_AP_RESPONSE_TIMEOUT_MS);
	} else {
		power_signal_set(PWR_PMIC_KPD_PWR_L, 0); /* Assert */
		if (!enable)
			power_signal_set(PWR_PMIC_RESIN_L, 0); /* Assert */

		ret = wait_pmic_pwron(enable,
				      PMIC_POWER_AP_RESPONSE_TIMEOUT_MS);

		power_signal_set(PWR_PMIC_KPD_PWR_L, 1); /* Deassert */
		if (!enable)
			power_signal_set(PWR_PMIC_RESIN_L, 1); /* Deassert */
	}
	return ret;
}

static void power_off_seq(void)
{
	if (power_signal_get(PWR_POWER_GOOD)) {
		set_pmic_pwron(0, 0);
		k_msleep(PMIC_POWER_OFF_DELAY_MS);
	}

	power_signal_disable(PWR_AP_RST_L);

	if (board_is_switchcap_enabled()) {
		set_system_power(0);
	}
}

static int power_on_seq(uint8_t poweron_event)
{
	int ret;

	reset_all_passthru_pmic_signal();
	set_system_power(1);
	power_signal_enable(PWR_AP_RST_L);

	ret = set_pmic_pwron(1, poweron_event);
	if (ret) {
		LOG_ERR("POWER_GOOD not seen in time");
		return ret;
	}

	passthru_ac_on_to_pmic();
	passthru_lid_open_to_pmic();

	return 0;
}

static int warm_reset_seq(void)
{
	int ret;

	power_signal_set(PWR_PMIC_RESIN_L, 0); /* Assert */
	k_msleep(PMIC_RESIN_PULSE_LENGTH_MS);
	power_signal_set(PWR_PMIC_RESIN_L, 1); /* Deassert */

	/* Check that the PMIC asserts AP_RST_L (active low) */
	ret = power_wait_mask_signals_timeout(
		IN_AP_RST_L, 0, PMIC_POWER_AP_RESPONSE_TIMEOUT_MS);
	if (ret)
		return ret;

	/* Wait until PS_HOLD goes back high */
	ret = power_wait_mask_signals_timeout(
		IN_PS_HOLD, IN_PS_HOLD, PMIC_POWER_AP_RESPONSE_TIMEOUT_MS);
	return ret;
}

void ap_power_reset(enum ap_power_shutdown_reason reason)
{
	LOG_INF("Qualcomm warm reset: %d", reason);
	report_ap_reset((enum chipset_shutdown_reason)reason);

	if (warm_reset_seq() != 0) {
		LOG_ERR("AP refuses to warm reset. Cold resetting.");
		ap_pwrseq_post_event(ap_pwrseq_get_instance(),
				     AP_PWRSEQ_EVENT_POWER_SHUTDOWN);
	}
}

static void check_force_shutdown(void)
{
	bool pressed = power_button_is_pressed();
	uint64_t now = k_uptime_get();

	if (pressed) {
		if (!qcom_data.power_button_was_pressed) {
			qcom_data.power_off_deadline = now + 8000;
		} else if (now >= qcom_data.power_off_deadline) {
			LOG_INF("Force shutdown due to long power button press");
			ap_pwrseq_post_event(ap_pwrseq_get_instance(),
					     AP_PWRSEQ_EVENT_POWER_SHUTDOWN);
		}
	}
	qcom_data.power_button_was_pressed = pressed;
}

/* State Handlers */

static int qcom_exp_g3_run(void *data)
{
	uint8_t reason = POWER_ON_CANCEL;

	if (ap_pwrseq_sm_is_event_set(data, AP_PWRSEQ_EVENT_POWER_STARTUP)) {
		reason = POWER_ON_BY_POWER_REQ_ON;
	} else if (qcom_data.auto_power_on) {
		reason = POWER_ON_BY_AUTO_POWER_ON;
	} else if (lid_is_open()) {
		reason = POWER_ON_BY_LID_OPEN;
	} else if (extpower_is_present()) {
		reason = POWER_ON_BY_AC_ON;
	} else if (power_button_is_pressed()) {
		reason = POWER_ON_BY_POWER_BUTTON_PRESSED;
	}

	if (reason != POWER_ON_CANCEL) {
		qcom_data.power_on_reason = (enum power_on_event_t)reason;
		qcom_data.auto_power_on = false;
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S5);
	}

	return 0;
}

AP_POWER_CHIPSET_STATE_DEFINE(G3, NULL, qcom_exp_g3_run, NULL);

static int qcom_exp_s5_entry(void *data)
{
	hook_notify(HOOK_CHIPSET_PRE_INIT);

	if (power_on_seq(qcom_data.power_on_reason) != 0) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
	}

	hook_notify(HOOK_CHIPSET_STARTUP);
	return 0;
}

static int qcom_exp_s5_run(void *data)
{
	if (ap_pwrseq_sm_is_event_set(data, AP_PWRSEQ_EVENT_POWER_SHUTDOWN)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
	}

	/* Wait for power button release before moving to S3/S0 if it was the
	 * reason */
	if (qcom_data.power_on_reason == POWER_ON_BY_POWER_BUTTON_PRESSED &&
	    power_button_is_pressed()) {
		return 0;
	}

	return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S3);
}

static int qcom_exp_s5_exit(void *data)
{
	if (ap_pwrseq_sm_get_entry_state(data) == AP_POWER_STATE_G3) {
		hook_notify(HOOK_CHIPSET_SHUTDOWN);
		power_off_seq();
		hook_notify(HOOK_CHIPSET_SHUTDOWN_COMPLETE);
	}
	return 0;
}

AP_POWER_CHIPSET_STATE_DEFINE(S5, qcom_exp_s5_entry, qcom_exp_s5_run,
			      qcom_exp_s5_exit);

static int qcom_exp_s3_run(void *data)
{
	if (ap_pwrseq_sm_is_event_set(data, AP_PWRSEQ_EVENT_POWER_SHUTDOWN)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S5);
	}

	if (!power_signal_get(PWR_POWER_GOOD)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S5);
	}

	check_force_shutdown();

	if (!power_signal_get(PWR_AP_SUSPEND)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S0);
	}

	return 0;
}

AP_POWER_CHIPSET_STATE_DEFINE(S3, NULL, qcom_exp_s3_run, NULL);

static int qcom_exp_s0_entry(void *data)
{
	hook_notify(HOOK_CHIPSET_RESUME);
	disable_sleep(SLEEP_MASK_AP_RUN);
	return 0;
}

static int qcom_exp_s0_run(void *data)
{
	if (ap_pwrseq_sm_is_event_set(data, AP_PWRSEQ_EVENT_POWER_SHUTDOWN)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S5);
	}

	if (!power_signal_get(PWR_POWER_GOOD)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S5);
	}

	check_force_shutdown();

	if (power_signal_get(PWR_AP_SUSPEND)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S3);
	}

	return 0;
}

static int qcom_exp_s0_exit(void *data)
{
	hook_notify(HOOK_CHIPSET_SUSPEND);
	enable_sleep(SLEEP_MASK_AP_RUN);
	return 0;
}

AP_POWER_CHIPSET_STATE_DEFINE(S0, qcom_exp_s0_entry, qcom_exp_s0_run,
			      qcom_exp_s0_exit);

/* Initialization */

void request_start_from_g3(void)
{
	const struct device *dev = ap_pwrseq_get_instance();

	if (ap_pwrseq_get_current_state(dev) == AP_POWER_STATE_G3) {
		ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_STARTUP);
	}
}

enum ap_pwrseq_state chipset_pwr_seq_get_state(void)
{
	if (power_signal_get(PWR_POWER_GOOD)) {
		return AP_POWER_STATE_S0;
	}
	return AP_POWER_STATE_G3;
}

static int qcom_exp_init(void)
{
	uint32_t reset_flags = system_get_reset_flags();

	if (reset_flags & EC_RESET_FLAG_SYSJUMP) {
		if (power_signal_get(PWR_POWER_GOOD)) {
			/* We should ideally set the initial state of the SM
			 * here, but ap_pwrseq_start is called with an initial
			 * state.
			 */
		}
	} else {
		board_set_switchcap_power(0);
	}

	qcom_data.auto_power_on = true;
	if (reset_flags & EC_RESET_FLAG_AP_OFF) {
		qcom_data.auto_power_on = false;
	}

	return 0;
}
SYS_INIT(qcom_exp_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);