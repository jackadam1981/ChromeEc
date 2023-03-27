/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "hooks.h"
#include "usb_pd_tcpm.h"

#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(int, charge_get_percent);
FAKE_VALUE_FUNC(int, tc_is_attached_src, int);
FAKE_VALUE_FUNC(int, typec_select_src_current_limit_rp, enum tcpc_rp_value);
FAKE_VALUE_FUNC(int, typec_update_cc, int);

//DECLARE_FAKE_VALUE_FUNC(int, tc_is_attached_src, int);

//DEFINE_FAKE_VALUE_FUNC(int, tc_is_attached_src, int);

FAKE_VOID_FUNC(limit_output_current);
FAKE_VOID_FUNC(resume_output_current);

FAKE_VOID_FUNC(x_ec_interrupt);
FAKE_VOID_FUNC(bmi3xx_interrupt);

int tc_is_attached_src_mock(int port)
{
	return 1;
}
uint8_t board_get_usb_pd_port_count(void)
{
	return 2;
}

static void current_limit_before(void *fixture)
{
	RESET_FAKE(charge_get_percent);
	RESET_FAKE(tc_is_attached_src);
	RESET_FAKE(limit_output_current);
	RESET_FAKE(resume_output_current);
}

ZTEST(current_limit, test_limit_output_current)
{
	charge_get_percent_fake.return_val = 20;
	tc_is_attached_src_fake.custom_fake = tc_is_attached_src_mock;
	hook_notify(HOOK_CHIPSET_SUSPEND);
	zassert_equal(0, limit_output_current_fake.call_count,"call_count is", limit_output_current_fake.call_count);
}

ZTEST(current_limit, test_resume_output_current)
{
	tc_is_attached_src_fake.custom_fake = tc_is_attached_src_mock;
	hook_notify(HOOK_CHIPSET_RESUME);
	zassert_equal(1, resume_output_current_fake.call_count, "call_count is", limit_output_current_fake.call_count);
}

ZTEST_SUITE(current_limit, NULL, NULL, current_limit_before, NULL,
	    NULL);