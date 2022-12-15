/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "chipset.h"
#include "ec_commands.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_tbt.h"
#include "usb_pd_tcpm.h"
#include "usb_pd_vdo.h"
#include "usbc_ppc.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

extern int test_set_tbt_ufp_reply(int port, enum typec_tbt_ufp_reply reply);

FAKE_VALUE_FUNC(int, pd_get_rev, int, enum tcpci_msg_type);
FAKE_VALUE_FUNC(mux_state_t, usb_mux_get, int);
FAKE_VOID_FUNC(pd_ufp_set_enter_mode, int, uint32_t *);
FAKE_VOID_FUNC(set_tbt_compat_mode_ready, int);
FAKE_VALUE_FUNC(bool, usb_mux_set_completed, int);

int pd_get_rev_pd_rev3_mock(int port, enum tcpci_msg_type type)
{
	return PD_REV30;
}

int pd_get_rev_pd_rev2_mock(int port, enum tcpci_msg_type type)
{
	return PD_REV20;
}

mux_state_t usb_mux_get_usb_mock(int port)
{
	return USB_PD_MUX_USB_ENABLED;
}

mux_state_t usb_mux_get_safe_mock(int port)
{
	return USB_PD_MUX_USB_ENABLED;
}

mux_state_t usb_mux_get_none_mock(int port)
{
	return USB_PD_MUX_NONE;
}

void pd_ufp_set_enter_mode_mock(int port, uint32_t *payload)
{
}

void set_tbt_cpmpat_mode_ready_mock(int port)
{
}

bool usb_mux_set_completed_true_mock(int port)
{
	return true;
}

bool usb_mux_set_completed_false_mock(int port)
{
	return false;
}

ZTEST_USER(usb_pd_ufp, test_svdm_tbt_compat_response_identity_case_0)
{
	uint32_t payload[8] = { 0 };

	zassert_equal(0, svdm_rsp.identity(0, &payload[0]));
}

ZTEST_USER(usb_pd_ufp, test_svdm_tbt_compat_response_identity_case_1)
{
	uint32_t payload[8] = { 0 };

	pd_get_rev_fake.custom_fake = pd_get_rev_pd_rev3_mock;
	payload[0] =
		VDO(USB_SID_PD, 1, VDO_SVDM_VERS_MAJOR(1) | CMD_DISCOVER_IDENT);
	zassert_equal(VDO_INDEX_PTYPE_DFP_VDO + 1,
		      svdm_rsp.identity(0, &payload[0]));
}

ZTEST_USER(usb_pd_ufp, test_svdm_tbt_compat_response_identity_case_2)
{
	uint32_t payload[8] = { 0 };

	pd_get_rev_fake.custom_fake = pd_get_rev_pd_rev2_mock;
	payload[0] =
		VDO(USB_SID_PD, 1, VDO_SVDM_VERS_MAJOR(1) | CMD_DISCOVER_IDENT);
	zassert_equal(VDO_INDEX_PRODUCT + 1, svdm_rsp.identity(0, &payload[0]));
}

ZTEST_USER(usb_pd_ufp, test_svdm_tbt_compat_response_svids_case_0)
{
	uint32_t payload[8] = { 0 };

	payload[0] =
		VDO(USB_SID_PD, 1, VDO_SVDM_VERS_MAJOR(1) | CMD_DISCOVER_SVID);
	zassert_equal(2, svdm_rsp.svids(0, &payload[0]));
}

ZTEST_USER(usb_pd_ufp, test_svdm_tbt_compat_response_svids_case_1)
{
	uint32_t payload[8] = { 0 };

	zassert_equal(0, svdm_rsp.svids(0, &payload[0]));
}

ZTEST_USER(usb_pd_ufp, test_svdm_tbt_compat_response_modes_case_0)
{
	uint32_t payload[8] = { 0 };
	union tbt_mode_resp_device vdo_tbt_modes = {
		.tbt_alt_mode = 0x0001,
		.tbt_adapter = TBT_ADAPTER_TBT3,
		.intel_spec_b0 = 0,
		.vendor_spec_b0 = 0,
		.vendor_spec_b1 = 0,
	};

	payload[0] = VDO(USB_VID_INTEL, 1,
			 VDO_SVDM_VERS_MAJOR(1) | CMD_DISCOVER_MODES);
	zassert_equal(2, svdm_rsp.modes(0, &payload[0]));
	zassert_equal(vdo_tbt_modes.raw_value, payload[1]);
}

ZTEST_USER(usb_pd_ufp, test_svdm_tbt_compat_response_modes_case_1)
{
	uint32_t payload[8] = { 0 };

	payload[0] = VDO(USB_VID_INTEL + 1, 1,
			 VDO_SVDM_VERS_MAJOR(1) | CMD_DISCOVER_MODES);
	zassert_equal(0, svdm_rsp.modes(0, &payload[0]));
}

ZTEST_USER(usb_pd_ufp, test_svdm_tbt_compat_response_enter_mode_0)
{
	uint32_t payload[8] = { 0 };

	test_set_tbt_ufp_reply(0, TYPEC_TBT_UFP_REPLY_ACK + 1);
	zassert_equal(0, svdm_rsp.enter_mode(0, &payload[0]));

	test_set_tbt_ufp_reply(0, TYPEC_TBT_UFP_REPLY_NAK);
	zassert_equal(0, svdm_rsp.enter_mode(0, &payload[0]));

	payload[0] = VDO(USB_VID_INTEL + 1, 1,
			 VDO_SVDM_VERS_MAJOR(1) | CMD_ENTER_MODE | (1 << 8));
	test_set_tbt_ufp_reply(0, TYPEC_TBT_UFP_REPLY_ACK);
	zassert_equal(0, svdm_rsp.enter_mode(0, &payload[0]));

	payload[0] =
		VDO(USB_VID_INTEL, 1, VDO_SVDM_VERS_MAJOR(1) | CMD_ENTER_MODE);
	test_set_tbt_ufp_reply(0, TYPEC_TBT_UFP_REPLY_ACK);
	zassert_equal(0, svdm_rsp.enter_mode(0, &payload[0]));
}

ZTEST_USER(usb_pd_ufp, test_svdm_tbt_compat_response_enter_mode_1)
{
	uint32_t payload[8];

	usb_mux_get_fake.custom_fake = usb_mux_get_usb_mock;
	usb_mux_set_completed_fake.custom_fake =
		usb_mux_set_completed_false_mock;
	pd_ufp_set_enter_mode_fake.custom_fake = pd_ufp_set_enter_mode_mock;
	set_tbt_compat_mode_ready_fake.custom_fake =
		set_tbt_cpmpat_mode_ready_mock;
	payload[0] =
		VDO(USB_VID_INTEL, 1,
		    VDO_SVDM_VERS_MAJOR(1) | CMD_ENTER_MODE | (VDO_OPOS(1)));
	test_set_tbt_ufp_reply(0, TYPEC_TBT_UFP_REPLY_ACK);
	zassert_equal(1, svdm_rsp.enter_mode(0, &payload[0]));
}

ZTEST_USER(usb_pd_ufp, test_svdm_tbt_compat_response_enter_mode_2)
{
	uint32_t payload[8];

	usb_mux_get_fake.custom_fake = usb_mux_get_safe_mock;
	usb_mux_set_completed_fake.custom_fake =
		usb_mux_set_completed_true_mock;
	pd_ufp_set_enter_mode_fake.custom_fake = pd_ufp_set_enter_mode_mock;
	set_tbt_compat_mode_ready_fake.custom_fake =
		set_tbt_cpmpat_mode_ready_mock;
	payload[0] =
		VDO(USB_VID_INTEL, 1,
		    VDO_SVDM_VERS_MAJOR(1) | CMD_ENTER_MODE | (VDO_OPOS(1)));
	test_set_tbt_ufp_reply(0, TYPEC_TBT_UFP_REPLY_ACK);
	zassert_equal(1, svdm_rsp.enter_mode(0, &payload[0]));
}

ZTEST_SUITE(usb_pd_ufp, NULL, NULL, NULL, NULL, NULL);