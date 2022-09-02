/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest.h>

#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "usb_pd.h"

#define TEST_PORT 0
BUILD_ASSERT(TEST_PORT == 0);

static void cros_ec_typec_before(void *data)
{
	ARG_UNUSED(data);

	zassume_true(chipset_in_state(CHIPSET_STATE_ON), NULL);
}

static void cros_ec_typec_after(void *data)
{
	ARG_UNUSED(data);

	test_set_chipset_to_s0();
}

ZTEST(cros_ec_typec, verify_hard_reset_event_cleared_at_startup)
{
	test_set_chipset_to_g3();

	zassert_false(pd_get_events(TEST_PORT) & PD_STATUS_EVENT_HARD_RESET,
		      NULL);

	pd_notify_event(TEST_PORT, PD_STATUS_EVENT_HARD_RESET);

	zassert_true(pd_get_events(TEST_PORT) & PD_STATUS_EVENT_HARD_RESET,
		     NULL);

	test_set_chipset_to_power_level(POWER_S3);
	k_sleep(K_SECONDS(2));

	zassert_false(pd_get_events(TEST_PORT) & PD_STATUS_EVENT_HARD_RESET,
		      NULL);
}

ZTEST_SUITE(cros_ec_typec, drivers_predicate_post_main, NULL,
	    cros_ec_typec_before, cros_ec_typec_after, NULL);
