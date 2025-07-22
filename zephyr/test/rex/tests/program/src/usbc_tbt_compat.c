/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "host_command.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#define TEST_PORT 0

FAKE_VOID_FUNC(charge_manager_update_charge, int, int,
	       const struct charge_port_info *);
FAKE_VOID_FUNC(pd_set_input_current_limit, int, uint32_t, uint32_t);
FAKE_VOID_FUNC(host_set_single_event, enum host_event_code);

FAKE_VALUE_FUNC(int, pd_get_rev, int, enum tcpci_msg_type);

static void usbc_tbt_compat_before(void *fixture)
{
	ARG_UNUSED(fixture);
	RESET_FAKE(charge_manager_update_charge);
	RESET_FAKE(pd_set_input_current_limit);
	RESET_FAKE(host_set_single_event);
	RESET_FAKE(pd_get_rev);
}

int pd_get_rev_pd3_mock(int port, enum tcpci_msg_type type)
{
	return PD_REV30;
}

ZTEST_USER(usbc_tbt_compat, test_svdm_response_identity_nak)
{
	uint32_t payload_buf[7]; // VDO_INDEX_*

	memset(payload_buf, 0, sizeof(payload_buf));
	payload_buf[0] = VDO(USB_SID_DISPLAYPORT, 0, 0); /* unexpected value */
	zassert_equal(svdm_rsp.identity(TEST_PORT, payload_buf), 0,
		      "Identity did not NAK");
	zassert_equal(pd_get_rev_fake.call_count, 0,
		      "Unexpected call to pd_get_rev");
}

ZTEST_USER(usbc_tbt_compat, test_svdm_response_identity_pd3_tbt_ack)
{
	uint32_t payload_buf[7]; // VDO_INDEX_*

	memset(payload_buf, 0, sizeof(payload_buf));
	payload_buf[0] = VDO(USB_SID_PD, 0, 0);
	pd_get_rev_fake.custom_fake = pd_get_rev_pd3_mock;
	board_set_tbt_ufp_reply(TEST_PORT, TYPEC_TBT_UFP_REPLY_ACK);
	zassert_not_equal(svdm_rsp.identity(TEST_PORT, payload_buf), 0,
			  "Identity did not ACK");
}

ZTEST_USER(usbc_tbt_compat, test_svdm_response_identity_pd3_tbt_nck)
{
	uint32_t payload_buf[7]; // VDO_INDEX_*

	memset(payload_buf, 0, sizeof(payload_buf));
	payload_buf[0] = VDO(USB_SID_PD, 0, 0);
	pd_get_rev_fake.custom_fake = pd_get_rev_pd3_mock;
	board_set_tbt_ufp_reply(TEST_PORT, TYPEC_TBT_UFP_REPLY_NAK);
	zassert_not_equal(svdm_rsp.identity(TEST_PORT, payload_buf), 0,
			  "Identity did not NAK");
}

ZTEST_USER(usbc_tbt_compat, test_svdm_response_svids_ack)
{
	uint32_t payload_buf[7]; // VDO_INDEX_*

	memset(payload_buf, 0, sizeof(payload_buf));
	payload_buf[0] = VDO(USB_SID_PD, 0, 0);
	board_set_tbt_ufp_reply(TEST_PORT, TYPEC_TBT_UFP_REPLY_ACK);
	zassert_not_equal(svdm_rsp.svids(TEST_PORT, payload_buf), 0,
			  "Svids did not ACK");
}

ZTEST_USER(usbc_tbt_compat, test_svdm_response_svids_nak)
{
	uint32_t payload_buf[7]; // VDO_INDEX_*

	memset(payload_buf, 0, sizeof(payload_buf));
	payload_buf[0] = VDO(0, 0, 0); /* unexpected value */
	zassert_equal(svdm_rsp.svids(TEST_PORT, payload_buf), 0,
		      "Svids did not NAK");
}

ZTEST_USER(usbc_tbt_compat, test_svdm_response_modes_ack)
{
	uint32_t payload_buf[7]; // VDO_INDEX_*

	memset(payload_buf, 0, sizeof(payload_buf));
	payload_buf[0] = VDO(USB_VID_INTEL, 0, 0);
	board_set_tbt_ufp_reply(TEST_PORT, TYPEC_TBT_UFP_REPLY_ACK);
	zassert_not_equal(svdm_rsp.modes(TEST_PORT, payload_buf), 0,
			  "Modes did not ACK");
}

ZTEST_USER(usbc_tbt_compat, test_svdm_response_modes_nak)
{
	uint32_t payload_buf[7]; // VDO_INDEX_*

	memset(payload_buf, 0, sizeof(payload_buf));
	payload_buf[0] = VDO(0, 0, 0); /* unexpected value */
	zassert_equal(svdm_rsp.modes(TEST_PORT, payload_buf), 0,
		      "Modes did not NAK");
}

#if 0
ZTEST_USER(usbc_tbt_compat, test_svdm_response_enter_mode)
{
}
#endif

ZTEST_SUITE(usbc_tbt_compat, NULL, NULL, usbc_tbt_compat_before, NULL, NULL);
