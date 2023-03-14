/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test clamshell/tablet when Only the GMR sensor is driving the tablet mode.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "tablet_mode.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

static int tablet_hook_count;

static void tablet_mode_change_hook(void)
{
	tablet_hook_count++;
}
DECLARE_HOOK(HOOK_TABLET_MODE_CHANGE, tablet_mode_change_hook,
	     HOOK_PRIO_DEFAULT);

void before_test(void)
{
	tablet_hook_count = 1;
}

void test_init(void)
{
	/* Make sure the device lid is in a consistent state (close) */
	gpio_set_level(GPIO_LID_OPEN, 0);
	gpio_set_level(GPIO_TABLET_MODE_L, 1);
}

test_static int test_start_lid_close(void)
{
	tablet_hook_count = 1;
	TEST_ASSERT(!tablet_get_mode());

	/* Opening, No change. */
	gpio_set_level(GPIO_LID_OPEN, 1);
	msleep(50);
	TEST_ASSERT(tablet_hook_count == 1);
	TEST_ASSERT(!tablet_get_mode());

	/* full 360, tablet mode. */
	gpio_set_level(GPIO_TABLET_MODE_L, 0);
	msleep(50);
	TEST_ASSERT(tablet_hook_count == 2);
	TEST_ASSERT(tablet_get_mode());

	/* Closing, Immediately back to clamshell mode. */
	gpio_set_level(GPIO_TABLET_MODE_L, 1);
	msleep(50);
	TEST_ASSERT(tablet_hook_count == 3);
	TEST_ASSERT(!tablet_get_mode());

	/* Back to close, no change. */
	gpio_set_level(GPIO_LID_OPEN, 0);
	msleep(50);
	TEST_ASSERT(tablet_hook_count == 3);
	TEST_ASSERT(!tablet_get_mode());

	return EC_SUCCESS;
}

test_static int test_start_tablet_mode(void)
{
	/* Go in tablet mode */
	gpio_set_level(GPIO_LID_OPEN, 1);
	gpio_set_level(GPIO_TABLET_MODE_L, 0);
	msleep(50);
	TEST_ASSERT(tablet_hook_count == 2);

	/* Shutdown device */
	hook_notify(HOOK_CHIPSET_SHUTDOWN);

	/* Check we start in tablet mode */
	msleep(50);
	TEST_ASSERT(tablet_get_mode());

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();

	RUN_TEST(test_start_lid_close);
	RUN_TEST(test_start_tablet_mode);

	test_print_result();
}
