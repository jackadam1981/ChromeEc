/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "emul/emul_pdc.h"
#include "emul/emul_realtek_rts54xx_public.h"
#include "gpio.h"
#include "system.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include <ap_power/ap_pwrseq.h>
#include <ap_power/ap_pwrseq_sm.h>
#include <drivers/intel_modular_aic.h>

static int board_ap_power_action_g3_run(void *data)
{
	if (ap_pwrseq_sm_is_event_set(data, AP_PWRSEQ_EVENT_POWER_SHUTDOWN)) {
		return 0;
	}

	return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S5);
}
AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_G3, NULL,
			      board_ap_power_action_g3_run, NULL);

static int board_ap_power_action_s5_run(void *data)
{
	return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S4);
}
AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_S5, NULL,
			      board_ap_power_action_s5_run, NULL);

static int board_ap_power_action_s4_run(void *data)
{
	if (ap_pwrseq_sm_is_event_set(data, AP_PWRSEQ_EVENT_POWER_SHUTDOWN)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
	}

	return 0;
}
AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_S4, NULL,
			      board_ap_power_action_s4_run, NULL);

static void ap_transition_through_s4(void)
{
	const struct device *dev = ap_pwrseq_get_instance();

	ap_pwrseq_start(dev, AP_POWER_STATE_G3);

	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_STARTUP);

	/* Allow callbacks to trigger */
	k_msleep(50);

	/* return system to G3 */
	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_SHUTDOWN);
}

static bool verify_dev_detected(const struct device *dev)
{
	struct intel_modular_aic_slot_data *data = dev->data;

	if (data->detection_done)
		return true;

	return false;
}

ZTEST(mod_aic_tests, test_aic_not_detected)
{
	const struct gpio_dt_spec *cc1 =
		GPIO_DT_FROM_NODELABEL(mod_aic1_detect);

	/* Toggle detection gpio high (not detected) */
	zassert_ok(gpio_emul_input_set(cc1->port, cc1->pin, 1));

	/* Change to S4 to send AP Callback to trigger device detection */
	ap_transition_through_s4();

	zassert_false(verify_dev_detected(
		DEVICE_DT_GET(DT_NODELABEL(mod_aic_slot1))));
	zassert_true(verify_dev_detected(
		DEVICE_DT_GET(DT_NODELABEL(mod_aic_slot2))));
}

ZTEST(mod_aic_tests, test_detected_after_gpio_toggle)
{
	const struct gpio_dt_spec *cc1 =
		GPIO_DT_FROM_NODELABEL(mod_aic1_detect);

	/* Toggle detection gpio low (detected) */
	zassert_ok(gpio_emul_input_set(cc1->port, cc1->pin, 0));

	/* Change to S4 to send AP Callback to trigger device detection */
	ap_transition_through_s4();

	zassert_true(verify_dev_detected(
		DEVICE_DT_GET(DT_NODELABEL(mod_aic_slot1))));
	zassert_true(verify_dev_detected(
		DEVICE_DT_GET(DT_NODELABEL(mod_aic_slot2))));

	/* Transition through S4 again to trigger ap callback and verify no
	 * issues */
	ap_transition_through_s4();

	zassert_true(verify_dev_detected(
		DEVICE_DT_GET(DT_NODELABEL(mod_aic_slot1))));
	zassert_true(verify_dev_detected(
		DEVICE_DT_GET(DT_NODELABEL(mod_aic_slot2))));
}

/* Test Suite Setup */
ZTEST_SUITE(mod_aic_tests, NULL, NULL, NULL, NULL, NULL);
