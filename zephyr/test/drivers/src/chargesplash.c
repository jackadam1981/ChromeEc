/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <kernel.h>
#include <stdbool.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <ztest.h>

#include "chargesplash.h"
#include "chipset.h"
#include "config.h"
#include "ec_commands.h"
#include "extpower.h"
#include "hooks.h"
#include "lid_switch.h"
#include "timer.h"
#include "test/drivers/test_state.h"

LOG_MODULE_REGISTER(chargesplash_test);

static struct k_poll_signal shutdown_complete_signal =
	K_POLL_SIGNAL_INITIALIZER(shutdown_complete_signal);
static struct k_poll_event shutdown_complete_event = K_POLL_EVENT_INITIALIZER(
	K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &shutdown_complete_signal);

static void handle_chipset_shutdown_complete_event(void)
{
	LOG_INF("Chipset shutdown complete");
	k_poll_signal_raise(&shutdown_complete_signal, 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN_COMPLETE,
	     handle_chipset_shutdown_complete_event, HOOK_PRIO_LAST);

static void force_chipset_off(void)
{
	if (!chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
		k_poll_signal_reset(&shutdown_complete_signal);
		chipset_force_shutdown(CHIPSET_RESET_INIT);
		k_poll(&shutdown_complete_event, 1, K_FOREVER);
	}

	/* Give the power sequencing code a bit to get from S3->S5 to S5 */
	while (!chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
		LOG_INF("WAIT");
		msleep(5);
	}
}

static void wait_for_chipset_off(void)
{
	if (!chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
		k_poll_signal_reset(&shutdown_complete_signal);
		k_poll(&shutdown_complete_event, 1, K_FOREVER);
	}
}

static struct k_poll_signal s0_signal = K_POLL_SIGNAL_INITIALIZER(s0_signal);
static struct k_poll_event s0_event = K_POLL_EVENT_INITIALIZER(
	K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &s0_signal);

static void handle_chipset_s0_event(void)
{
	LOG_INF("Chipset to S0");
	k_poll_signal_raise(&s0_signal, 0);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, handle_chipset_s0_event, HOOK_PRIO_LAST);

static void wait_for_chipset_startup(void)
{
	if (!chipset_in_state(CHIPSET_STATE_ON)) {
		k_poll_signal_reset(&s0_signal);
		k_poll(&s0_event, 1, K_FOREVER);
	}
}

#define GPIO_ACOK_OD_NODE DT_NODELABEL(gpio_acok_od)
#define GPIO_ACOK_OD_CTLR DT_GPIO_CTLR(GPIO_ACOK_OD_NODE, gpios)
#define GPIO_ACOK_OD_PORT DT_GPIO_PIN(GPIO_ACOK_OD_NODE, gpios)

static void set_ac(bool connected)
{
	const struct device *acok_dev = DEVICE_DT_GET(GPIO_ACOK_OD_CTLR);

	zassert_not_equal(
		extpower_is_present(), connected,
		"AC change was requested, but it's already in that state");

	zassert_ok(gpio_emul_input_set(acok_dev, GPIO_ACOK_OD_PORT, connected),
		   NULL);

	while (extpower_is_present() != connected) {
		msleep(CONFIG_EXTPOWER_DEBOUNCE_MS + 1);
	}
}

#define GPIO_LID_OPEN_EC_NODE DT_NODELABEL(gpio_lid_open_ec)
#define GPIO_LID_OPEN_EC_CTLR DT_GPIO_CTLR(GPIO_LID_OPEN_EC_NODE, gpios)
#define GPIO_LID_OPEN_EC_PORT DT_GPIO_PIN(GPIO_LID_OPEN_EC_NODE, gpios)

static void set_lid(bool open, bool inhibit_boot)
{
	const struct device *lid_switch_dev =
		DEVICE_DT_GET(GPIO_LID_OPEN_EC_CTLR);

	zassert_not_equal(
		lid_is_open(), open,
		"Lid change was requested, but it's already in that state");

	if (!open) {
		zassert_false(
			inhibit_boot,
			"inhibit_boot should not be used with a lid close");
	}

	zassert_ok(gpio_emul_input_set(lid_switch_dev, GPIO_LID_OPEN_EC_PORT,
				       open),
		   NULL);

	while (lid_is_open() != open) {
		usleep(LID_DEBOUNCE_US + 1);
	}

	if (inhibit_boot) {
		wait_for_chipset_startup();
		force_chipset_off();
	}
}

#define GPIO_POWER_BUTTON_NODE DT_NODELABEL(gpio_ec_pwr_btn_odl)
#define GPIO_POWER_BUTTON_CTLR DT_GPIO_CTLR(GPIO_POWER_BUTTON_NODE, gpios)
#define GPIO_POWER_BUTTON_PORT DT_GPIO_PIN(GPIO_POWER_BUTTON_NODE, gpios)

static void set_power_button(bool pressed)
{
	const struct device *power_button_dev =
		DEVICE_DT_GET(GPIO_POWER_BUTTON_CTLR);

	zassert_ok(gpio_emul_input_set(power_button_dev, GPIO_POWER_BUTTON_PORT,
				       pressed),
		   NULL);
}

/* Simulate a regular power button press */
static void pulse_power_button(void)
{
	set_power_button(true);
	msleep(400);
	set_power_button(false);
}

static void before_fn(void *unused)
{
	set_power_button(false);
	force_chipset_off();

	if (lid_is_open()) {
		set_lid(false, false);
	}

	if (extpower_is_present()) {
		set_ac(false);
	}

	chargesplash_reset();
}

static void teardown_fn(void *unused)
{
	/* Other tests expect the chipset to be off, make sure we're there */
	force_chipset_off();
}

ZTEST_SUITE(chargesplash, drivers_predicate_post_main, NULL, before_fn, NULL,
	    teardown_fn);

/**
 * When the lid is open and AC is connected, the chargesplash should
 * be requested.
 */
ZTEST_USER(chargesplash, test_connect_ac)
{
	set_lid(true, true);

	set_ac(true);
	zassert_true(chargesplash_get_boot_mode(),
		     "chargesplash should be requested");
}

/**
 * When AC is not connected and we open the lid, the chargesplash
 * should not be requested.
 */
ZTEST_USER(chargesplash, test_no_connect_ac)
{
	set_lid(true, false);
	zassert_false(chargesplash_get_boot_mode(),
		      "chargesplash should not be requested");
	wait_for_chipset_startup();
}

/**
 * When we connect AC with the lid closed, the chargesplash should not
 * be requested.
 */
ZTEST_USER(chargesplash, test_ac_connect_when_lid_closed)
{
	set_ac(true);
	zassert_false(chargesplash_get_boot_mode(),
		      "chargesplash should not be requested");
}

/**
 * Test the chipset is shut down when AC is disconnected in the middle
 * of a request.
 */
ZTEST_USER(chargesplash, test_ac_disconnect_during_request)
{
	LOG_INF("begin test_ac_disconnect_during_request");
	set_lid(true, true);
	set_ac(true);
	LOG_INF("lid open, ac connected!");

	zassert_true(chargesplash_get_boot_mode(),
		     "chargesplash should be requested");
	LOG_INF("waiting for chipset startup");
	wait_for_chipset_startup();

	LOG_INF("disconnecting ac");
	set_ac(false);
	zassert_false(chargesplash_get_boot_mode(),
		      "chargesplash should have been canceled");
	LOG_INF("wait for chipset to go off");
	wait_for_chipset_off();
	LOG_INF("done");
}

/**
 * Test that, after many repeated cancellations, the chargesplash
 * feature becomes locked and non-functional.  This condition
 * replicates a damaged charger or port which cannot maintain a
 * reliable connection.
 */
ZTEST_USER(chargesplash, test_lockout)
{
	int i;

	set_lid(true, true);

	for (i = 0; i < CONFIG_PLATFORM_EC_CHARGESPLASH_BOOT_MAX_TRIES; i++) {
		set_ac(true);

		zassert_true(chargesplash_get_boot_mode(),
			     "chargesplash should be requested");
		wait_for_chipset_startup();

		set_ac(false);
		zassert_false(chargesplash_get_boot_mode(),
			      "chargesplash should have been canceled");
		wait_for_chipset_off();
	}

	set_ac(true);
	zassert_false(chargesplash_get_boot_mode(),
		      "chargesplash should be locked out");
	wait_for_chipset_off();
}

/**
 * Test cancel chargesplash request by power button push
 */
ZTEST_USER(chargesplash, test_power_button)
{
	set_lid(true, true);

	set_ac(true);
	zassert_true(chargesplash_get_boot_mode(),
		     "chargesplash should be requested");
	wait_for_chipset_startup();
	zassert_true(chargesplash_get_boot_mode(),
		     "chargesplash should still be requested");

	pulse_power_button();
	zassert_false(chargesplash_get_boot_mode(),
		      "chargesplash should be canceled by power button push");
	zassert_true(chipset_in_state(CHIPSET_STATE_ON),
		     "chipset should be on");
}
