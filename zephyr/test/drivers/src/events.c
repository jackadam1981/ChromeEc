/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Unit Tests for AP power events (for legacy hooks).
 *
 * Assumes CONFIG_AP_PWRSEQ is disabled. These tests are for
 * checking events are received into the AP power event callbacks
 * from the hooks subsystem for chipset events such
 * HOOK_CHIPSET_SUSPEND etc.
 * When CONFIG_AP_PWRSEQ is disabled, events from hooks to the
 * AP power callbacks.
 * For tests that cover when CONFIG_AP_PWRSEQ is enabled, see tests/ap_power
 */

#include <device.h>

#include <logging/log.h>
#include <zephyr.h>
#include <ztest.h>

#include "ap_power/ap_power.h"
#include "common.h"
#include "hooks.h"
#include "ec_tasks.h"
#include "stubs.h"
#include "util.h"
#include "test_state.h"

/*
 * Structure passed to event listeners.
 */
struct events {
	struct ap_power_ev_callback cb;
	enum ap_power_events event;
	int count;
};

/*
 * Common handler.
 * Increment count, and store event received.
 */
static void ev_handler(struct ap_power_ev_callback *callback,
		       struct ap_power_ev_data data)
{
	struct events *ev = CONTAINER_OF(callback, struct events, cb);

	ev->count++;
	ev->event = data.event;
}

/**
 * @brief TestPurpose: Verify AP power events can be registered and received
 *
 * @details
 * Validate callbacks can be registered and can receive events.
 *
 * Expected Results
 *  - Hook events are received.
 */
ZTEST(events, test_hook_event)
{
	static struct events cb;

	ap_power_ev_init_callback(&cb.cb, ev_handler, AP_POWER_SUSPEND);
	ap_power_ev_add_callback(&cb.cb);
	hook_notify(HOOK_CHIPSET_SUSPEND);
	zassert_equal(1, cb.count, "Callback not called");
	zassert_equal(AP_POWER_SUSPEND, cb.event, "Wrong event");
	ap_power_ev_remove_callback(&cb.cb);
	hook_notify(HOOK_CHIPSET_SUSPEND);
	zassert_equal(1, cb.count, "Callback called");

	cb.count = 0;
	ap_power_ev_init_callback(&cb.cb, ev_handler,
		AP_POWER_SUSPEND|AP_POWER_RESUME);
	ap_power_ev_add_callback(&cb.cb);
	hook_notify(HOOK_CHIPSET_SUSPEND);
	zassert_equal(1, cb.count, "Callbacks not called");
	zassert_equal(AP_POWER_SUSPEND, cb.event, "Wrong event");
	hook_notify(HOOK_CHIPSET_RESUME);
	zassert_equal(2, cb.count, "Callbacks not called");
	zassert_equal(AP_POWER_RESUME, cb.event, "Wrong event");

	ap_power_ev_remove_events(&cb.cb, AP_POWER_SUSPEND);
	hook_notify(HOOK_CHIPSET_SUSPEND);
	zassert_equal(2, cb.count, "Suspend allback called");

	hook_notify(HOOK_CHIPSET_STARTUP);
	zassert_equal(2, cb.count, "Startup callback called");
	ap_power_ev_add_events(&cb.cb, AP_POWER_STARTUP);
	hook_notify(HOOK_CHIPSET_STARTUP);
	zassert_equal(3, cb.count, "Startup callback not called");
}


/**
 * @brief Test Suite: Verifies AP events functionality.
 */
ZTEST_SUITE(events, NULL, NULL, NULL, NULL, NULL);
