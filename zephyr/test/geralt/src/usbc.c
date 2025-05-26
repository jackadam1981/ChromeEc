/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "test_state.h"
#include "usb_pd.h"
#include "usb_tc_sm.h"

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "usb_pd_dpm_sm.hh"

FAKE_VALUE_FUNC(bool, tc_is_attached_src, int);

/*
 * Store a bitmask of active source ports.
 * Bit 0 for port 0, bit 1 for port 1.
 */
static int active_src_mask;

static bool tc_is_attached_src_custom_fake(int port)
{
	return (active_src_mask & BIT(port));
}

static void usbc_before(void *fixture)
{
	RESET_FAKE(tc_is_attached_src);
	tc_is_attached_src_fake.custom_fake = tc_is_attached_src_custom_fake;
	active_src_mask = 0;
}

ZTEST_SUITE(usbc, geralt_predicate_post_main, NULL, usbc_before, NULL, NULL);

ZTEST(usbc, test_dpm_get_source_info_msg)
{
	union sido sido;

	/*
	 * Scenario 1: Port 0 is the only active source.
	 * Should provide 3A (15W).
	 */
	active_src_mask = BIT(0);
	sido = dpm_get_source_info_msg(0);

	zassert_equal(sido.sido1.port_type, PD_SOURCE_PORT_CAPABILITY_MANAGED,
		      "port_type mismatch");
	zassert_equal(sido.sido2.port_type, PD_SOURCE_PORT_CAPABILITY_MANAGED,
		      "port_type mismatch");
	zassert_equal(sido.sido1.reserved, 0, "reserved not 0");
	zassert_equal(sido.sido2.reserved, 0, "reserved not 0");
	zassert_equal(sido.sido2.dps_port, 0, "dps_port not 0");
	zassert_equal(sido.sido2.port_maximum_pdp, PD_SIDO2_15W,
		      "max_pdp mismatch");
	zassert_equal(sido.sido1.port_maximum_pdp, 15, "max_pdp mismatch");
	zassert_equal(sido.sido2.port_guaranteed_pdp, PD_SIDO2_7_5W,
		      "guaranteed_pdp mismatch");
	zassert_equal(sido.sido1.port_reported_pdp, 15,
		      "reported_pdp mismatch for 3A case");
	zassert_equal(sido.sido1.port_present_pdp, 15,
		      "present_pdp mismatch for 3A case");

	/*
	 * Scenario 2: Port 1 is the only active source.
	 * Should provide 3A (15W).
	 */
	active_src_mask = BIT(1);
	sido = dpm_get_source_info_msg(1);

	zassert_equal(sido.sido1.port_reported_pdp, 15,
		      "reported_pdp mismatch for 3A case");
	zassert_equal(sido.sido1.port_present_pdp, 15,
		      "present_pdp mismatch for 3A case");

	/*
	 * Scenario 3: Both ports are active sources.
	 * Port 0 should provide 1.5A (7.5W, truncated to 7W).
	 */
	active_src_mask = BIT(0) | BIT(1);
	sido = dpm_get_source_info_msg(0);

	zassert_equal(sido.sido1.port_reported_pdp, 7,
		      "reported_pdp mismatch for 1.5A case");
	zassert_equal(sido.sido1.port_present_pdp, 7,
		      "present_pdp mismatch for 1.5A case");

	/*
	 * Scenario 4: Both ports are active sources.
	 * Port 1 should provide 1.5A (7.5W, truncated to 7W).
	 */
	sido = dpm_get_source_info_msg(1);

	zassert_equal(sido.sido1.port_reported_pdp, 7,
		      "reported_pdp mismatch for 1.5A case");
	zassert_equal(sido.sido1.port_present_pdp, 7,
		      "present_pdp mismatch for 1.5A case");

	/*
	 * Scenario 5: No ports are active.
	 * Should provide 1.5A (7.5W, truncated to 7W).
	 */
	active_src_mask = 0;
	sido = dpm_get_source_info_msg(0);

	zassert_equal(sido.sido1.port_reported_pdp, 7,
		      "reported_pdp mismatch for 1.5A case");
	zassert_equal(sido.sido1.port_present_pdp, 7,
		      "present_pdp mismatch for 1.5A case");
}
