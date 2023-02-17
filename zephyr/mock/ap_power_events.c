/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_power/ap_power_events.h"
#include "mock/ap_power_events.h"

#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(mock_ap_power_events);

/* Mocks for ec/zephyr/include/ap_power/ap_power_events.h */
DEFINE_FAKE_VOID_FUNC(ap_power_ev_send_callbacks, enum ap_power_events);

#define MOCK_AP_POWER_EVENTS_LIST(FAKE)           \
	{                                         \
		FAKE(ap_power_ev_send_callbacks); \
	}

/**
 * @brief Reset all the fakes before each test.
 */
static void mock_ap_power_events_rule_before(const struct ztest_unit_test *test,
					     void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

	MOCK_AP_POWER_EVENTS_LIST(RESET_FAKE);

	FFF_RESET_HISTORY();

	ap_power_ev_send_callbacks_fake.custom_fake =
		ap_power_ev_send_callbacks_custom_fake;
}

ZTEST_RULE(mock_ap_power_events_rule, mock_ap_power_events_rule_before, NULL);

void ap_power_ev_send_callbacks_custom_fake(enum ap_power_events event)
{
#ifdef CONFIG_TEST_BOARD_POWER
	zassert_equal(event, AP_POWER_PRE_INIT);
#endif
}
