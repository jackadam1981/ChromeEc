/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/raa489000.h"
#include "driver/tcpm/tcpci.h"
#include "emul/tcpc/emul_raa489000.h"
#include "emul/tcpc/emul_tcpci.h"
#include "test/drivers/test_state.h"
#include "usb_pd_tcpm.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#define RAA489000_PORT 0

ZTEST(tcpc_raa489000, test_check_vendor)
{
	int v;

	zassert_ok(tcpc_read16(RAA489000_PORT, TCPC_REG_VENDOR_ID, &v));
	zassert_equal(v, 0x45b);

	tcpm_dump_registers(RAA489000_PORT);
}

FAKE_VALUE_FUNC(int, pd_vbus_valid_for_bist, int);
bool raa489000_tcpm_should_enter_bist_mode(int port, uint32_t *payload,
					   int *head);

bool mock_pd_vbus_valid_for_bist_fail(int port)
{
	return false;
}

bool mock_pd_vbus_valid_for_bist_pass(int port)
{
	return true;
}

ZTEST(tcpc_raa489000, test_raa489000_tcpm_should_enter_bist_mode)
{
	uint32_t payload[1];
	int hdr;

	hdr = 0x3003;
	payload[0] = 0x80000000;
	pd_vbus_valid_for_bist_fake.custom_fake =
		mock_pd_vbus_valid_for_bist_pass;

	bool result = raa489000_tcpm_should_enter_bist_mode(RAA489000_PORT,
							    payload, &hdr);
	zassert_true(result, "BIST mode should be enabled");
}

ZTEST(tcpc_raa489000, test_raa489000_tcpm_should_not_enter_bist_mode)
{
	uint32_t payload[1] = { 0 };
	int head = 0;

	bool result = raa489000_tcpm_should_enter_bist_mode(RAA489000_PORT,
							    payload, &head);
	zassert_false(result, "BIST mode should not be enabled");

	bool enable = true;
	zassert_equal(tcpc_set_bist_test_mode(RAA489000_PORT, enable),
		      EC_SUCCESS);
}

ZTEST_SUITE(tcpc_raa489000, drivers_predicate_post_main, NULL, NULL, NULL,
	    NULL);
