/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test AC input current ramp.
 */

#include "charge_manager.h"
#include "charge_ramp.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

/* Charge supplier priority: lower number indicates higher priority. */
const int supplier_priority[] = {
	[CHARGE_SUPPLIER_TEST1] = 0,
	[CHARGE_SUPPLIER_TEST2] = 1,
	[CHARGE_SUPPLIER_TEST3] = 1,
	[CHARGE_SUPPLIER_TEST4] = 1,
	[CHARGE_SUPPLIER_TEST5] = 3,
	[CHARGE_SUPPLIER_TEST6] = 3,
	[CHARGE_SUPPLIER_TEST7] = 5,
	[CHARGE_SUPPLIER_TEST8] = 6,
	[CHARGE_SUPPLIER_TEST9] = 6,
};
BUILD_ASSERT(ARRAY_SIZE(supplier_priority) == CHARGE_SUPPLIER_COUNT);

#define TASK_EVENT_OVERCURRENT (1 << 0)

#define RAMP_STABLE_DELAY (120 * SECOND)

static int system_load_current_ma;
static int vbus_low_current_ma = 500;
static int active_supplier = CHARGE_SUPPLIER_NONE;
static int overcurrent_current_ma = 3000;

static int charge_limit_ma;

/* Mock functions */

int board_is_ramp_allowed(int supplier)
{
	/* Ramp for TEST4-TEST9 */
	return supplier > CHARGE_SUPPLIER_TEST3;
}

int board_is_consuming_full_charge(void)
{
	return charge_limit_ma <= system_load_current_ma;
}

int board_is_vbus_too_low(void)
{
	return MIN(system_load_current_ma, charge_limit_ma) >
	       vbus_low_current_ma;
}

void board_set_charge_limit(int limit_ma)
{
	charge_limit_ma = limit_ma;
	if (charge_limit_ma > overcurrent_current_ma)
		task_set_event(TASK_ID_TEST_RUNNER, TASK_EVENT_OVERCURRENT, 0);
}

int charge_manager_get_active_supplier(void)
{
	return active_supplier;
}

/* Test utilities */

static void plug_charger(int supplier_type, int port, int min_current,
			 int vbus_low_current, int overcurrent_current)
{
	vbus_low_current_ma = vbus_low_current;
	overcurrent_current_ma = overcurrent_current;
	active_supplier = supplier_type;
	chg_ramp_charge_port_change(port);
	chg_ramp_set_min_current(min_current);
}

static void unplug_charger(void)
{
	active_supplier = CHARGE_SUPPLIER_NONE;
	chg_ramp_charge_port_change(CHARGE_PORT_NONE);
	chg_ramp_set_min_current(0);
}

static int wait_stable_no_overcurrent(void)
{
	return task_wait_event(RAMP_STABLE_DELAY) != TASK_EVENT_OVERCURRENT;
}

/* Tests */

static int test_no_ramp(void)
{
	system_load_current_ma = 3000;
	/* A powerful charger, but hey, you're not allowed to ramp! */
	plug_charger(CHARGE_SUPPLIER_TEST1, 0, 500, 3000, 3000);
	usleep(SECOND);
	/* That's right. Start at 500 mA */
	TEST_ASSERT(charge_limit_ma == 500);
	TEST_ASSERT(wait_stable_no_overcurrent());
	/* ... and stays at 500 mA */
	TEST_ASSERT(charge_limit_ma == 500);

	unplug_charger();
	usleep(SECOND);
	TEST_ASSERT(charge_limit_ma == 0);
	return EC_SUCCESS;
}

static int test_full_ramp(void)
{
	system_load_current_ma = 3000;
	/* Now you get to ramp with this 3A charger */
	plug_charger(CHARGE_SUPPLIER_TEST4, 0, 500, 3000, 3000);
	usleep(SECOND);
	/* Start with something around 500 mA */
	TEST_ASSERT(charge_limit_ma >= 500 && charge_limit_ma < 1000);
	TEST_ASSERT(wait_stable_no_overcurrent());
	/* And ramp up to 3A */
	TEST_ASSERT(charge_limit_ma == 3000);

	unplug_charger();
	usleep(SECOND);
	TEST_ASSERT(charge_limit_ma == 0);

	return EC_SUCCESS;
}

static int test_vbus_dip(void)
{
	system_load_current_ma = 3000;
	/* VBUS dips too low right before the charger shuts down */
	plug_charger(CHARGE_SUPPLIER_TEST5, 0, 1000, 1500, 1600);

	TEST_ASSERT(wait_stable_no_overcurrent());
	TEST_ASSERT(charge_limit_ma <= 1600);

	unplug_charger();
	usleep(SECOND);
	TEST_ASSERT(charge_limit_ma == 0);

	return EC_SUCCESS;
}

static int test_overcurrent(void)
{
	system_load_current_ma = 3000;
	/* Huh...VBUS doesn't dip before the charger shuts down */
	plug_charger(CHARGE_SUPPLIER_TEST6, 0, 500, 3000, 1500);

	while (task_wait_event(RAMP_STABLE_DELAY) == TASK_EVENT_OVERCURRENT) {
		/* Charger goes away but comes back after 0.6 seconds */
		unplug_charger();
		usleep(MSEC * 600);
		plug_charger(CHARGE_SUPPLIER_TEST6, 0, 500, 3000, 1500);
	}

	TEST_ASSERT(charge_limit_ma <= 1500);

	unplug_charger();
	usleep(SECOND);
	TEST_ASSERT(charge_limit_ma == 0);

	return EC_SUCCESS;
}

static int test_switch_outlet(void)
{
	int i;

	system_load_current_ma = 3000;
	/* Here's a nice powerful charger */
	plug_charger(CHARGE_SUPPLIER_TEST7, 0, 500, 3000, 3000);

	/*
	 * Now the user decides to move it to a nearby outlet...actually
	 * he decides to move it 5 times!
	 */
	for (i = 0; i < 5; ++i) {
		usleep(SECOND * 20);
		unplug_charger();
		usleep(SECOND * 1.5);
		plug_charger(CHARGE_SUPPLIER_TEST7, 0, 500, 3000, 3000);
	}

	/* Should still ramp up to 3000 mA */
	TEST_ASSERT(wait_stable_no_overcurrent());
	TEST_ASSERT(charge_limit_ma == 3000);

	unplug_charger();
	usleep(SECOND);
	TEST_ASSERT(charge_limit_ma == 0);

	return EC_SUCCESS;
}

static int test_fast_switch(void)
{
	int i;

	system_load_current_ma = 3000;
	plug_charger(CHARGE_SUPPLIER_TEST4, 0, 500, 3000, 3000);

	/*
	 * Here comes that naughty user again, and this time he's switching
	 * outlet really quickly. Fortunately this time he only does it twice.
	 */
	for (i = 0; i < 2; ++i) {
		usleep(SECOND * 20);
		unplug_charger();
		usleep(600 * MSEC);
		plug_charger(CHARGE_SUPPLIER_TEST4, 0, 500, 3000, 3000);
	}

	/* Should still ramp up to 3000 mA */
	TEST_ASSERT(wait_stable_no_overcurrent());
	TEST_ASSERT(charge_limit_ma == 3000);

	unplug_charger();
	usleep(SECOND);
	TEST_ASSERT(charge_limit_ma == 0);

	return EC_SUCCESS;
}

static int test_overcurrent_after_switch_outlet(void)
{
	system_load_current_ma = 3000;
	/* Here's a nice powerful charger */
	plug_charger(CHARGE_SUPPLIER_TEST8, 0, 500, 3000, 1500);
	usleep(SECOND * 5);

	/* Now the user decides to move it to a nearby outlet */
	unplug_charger();
	usleep(SECOND * 1.5);
	plug_charger(CHARGE_SUPPLIER_TEST8, 0, 500, 3000, 1500);

	/* Okay the user is satisified */
	while (task_wait_event(RAMP_STABLE_DELAY) == TASK_EVENT_OVERCURRENT) {
		/* Charger goes away but comes back after 0.6 seconds */
		unplug_charger();
		usleep(MSEC * 600);
		plug_charger(CHARGE_SUPPLIER_TEST8, 0, 500, 3000, 1500);
	}

	TEST_ASSERT(charge_limit_ma <= 1500);

	unplug_charger();
	usleep(SECOND);
	TEST_ASSERT(charge_limit_ma == 0);

	return EC_SUCCESS;
}

static int test_partial_load(void)
{
	/* We have a 3A charger, but we just want 1.5A */
	system_load_current_ma = 1500;
	plug_charger(CHARGE_SUPPLIER_TEST9, 0, 500, 3000, 2500);

	/* We should end up with a little bit more than 1.5A */
	TEST_ASSERT(wait_stable_no_overcurrent());
	TEST_ASSERT(charge_limit_ma >= 1500 && charge_limit_ma < 1600);

	/* Ok someone just started watching YouTube */
	system_load_current_ma = 2000;
	TEST_ASSERT(wait_stable_no_overcurrent());
	TEST_ASSERT(charge_limit_ma >= 2000 && charge_limit_ma < 2100);

	/* Somehow the system load increases again */
	system_load_current_ma = 2600;
	while (task_wait_event(RAMP_STABLE_DELAY) == TASK_EVENT_OVERCURRENT) {
		/* Charger goes away but comes back after 0.6 seconds */
		unplug_charger();
		usleep(MSEC * 600);
		plug_charger(CHARGE_SUPPLIER_TEST9, 0, 500, 3000, 2500);
	}

	/* Alright the charger isn't powerful enough, so we'll stop at 2.5A */
	TEST_ASSERT(charge_limit_ma >= 2400 && charge_limit_ma <= 2500);

	unplug_charger();
	usleep(SECOND);
	TEST_ASSERT(charge_limit_ma == 0);

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_no_ramp);
	RUN_TEST(test_full_ramp);
	RUN_TEST(test_vbus_dip);
	RUN_TEST(test_overcurrent);
	RUN_TEST(test_switch_outlet);
	RUN_TEST(test_fast_switch);
	RUN_TEST(test_overcurrent_after_switch_outlet);
	RUN_TEST(test_partial_load);

	test_print_result();
}
