/* Copyright 2024 The ChromiumOS Authors
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

#define RAA489000_PORT 1
#define RAA489000_NODE DT_NODELABEL(raa489000_emul)

const struct emul *raa489000_emul = EMUL_DT_GET(RAA489000_NODE);

ZTEST(tcpc_raa489000, test_check_vendor)
{
	int v;

	zassert_ok(tcpc_read16(RAA489000_PORT, TCPC_REG_VENDOR_ID, &v));
	zassert_equal(v, 0x45b);

	tcpm_dump_registers(RAA489000_PORT);
}

#define MAX_PORTS 2
static bool raa489000_bist_mode[MAX_PORTS] = { false };
FAKE_VALUE_FUNC(int, pd_vbus_valid_for_bist, int);

bool mock_pd_vbus_valid_for_bist_fail(int port)
{
	return false;
}

bool mock_pd_vbus_valid_for_bist_pass(int port)
{
	return true;
}

static bool raa489000_tcpm_should_enter_bist_mode(int port, uint32_t *payload,
						  int *head)
{
	uint32_t hdr = *head;

	if ((PD_HEADER_EXT(hdr) == 0) && (PD_HEADER_CNT(hdr) > 0) &&
	    (PD_HEADER_TYPE(hdr) == PD_DATA_BIST) &&
	    (BIST_MODE(payload[0]) == BIST_TEST_DATA) &&
	    (!raa489000_bist_mode[port]) && pd_vbus_valid_for_bist(port)) {
		raa489000_bist_mode[port] = true;
		return tcpci_set_bist_test_mode(port, true);
	}
	return EC_SUCCESS;
}

ZTEST(tcpc_raa489000, test_raa489000_tcpm_should_enter_bist_mode)
{
	uint32_t payload[0];
	int hdr;

	hdr = 0x3003;
	raa489000_bist_mode[0] = false;
	payload[0] = 0x80000000;
	pd_vbus_valid_for_bist_fake.custom_fake =
		mock_pd_vbus_valid_for_bist_pass;

	bool result = raa489000_tcpm_should_enter_bist_mode(0, payload, &hdr);

	zassert_true(result == EC_SUCCESS, "BIST mode should be enabled");
	zassert_true(raa489000_bist_mode[0] == true,
		     "BIST mode should be enabled for port 0");
}

ZTEST(tcpc_raa489000, test_raa489000_tcpm_should_not_enter_bist_mode)
{
	uint32_t payload[1] = { 0 };
	int head = 0;

	raa489000_bist_mode[0] = false;

	bool result = raa489000_tcpm_should_enter_bist_mode(0, payload, &head);

	zassert_true(result == EC_SUCCESS, "BIST mode should not be enabled");
	zassert_true(raa489000_bist_mode[0] == false,
		     "BIST mode should not be enabled for port 0");
}

ZTEST(tcpc_raa489000, test_raa489000_tcpm_should_enter_bist_mode_invalid_port)
{
	uint32_t payload[1] = { BIST_TEST_DATA };
	int head = 1;

	bool result = raa489000_tcpm_should_enter_bist_mode(MAX_PORTS, payload,
							    &head);

	zassert_true(result == false, "Invalid port should return an error");
}
ZTEST_SUITE(tcpc_raa489000, drivers_predicate_post_main, NULL, NULL, NULL,
	    NULL);
