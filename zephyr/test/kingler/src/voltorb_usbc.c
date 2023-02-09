/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "hooks.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"

#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define PDO_FIXED_FLAGS \
	(PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP | PDO_FIXED_COMM_CAP)

enum chipset_state_mask set_state;

FAKE_VALUE_FUNC(int, charge_get_percent);
FAKE_VALUE_FUNC(int, chipset_in_state, int);
FAKE_VALUE_FUNC(int, tc_is_attached_src, int);

FAKE_VOID_FUNC(x_ec_interrupt);
FAKE_VOID_FUNC(bmi3xx_interrupt);
FAKE_VOID_FUNC(pd_update_contract, int);
FAKE_VOID_FUNC(check_src_port);
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, check_src_port, HOOK_PRIO_DEFAULT);
FAKE_VOID_FUNC(resume_src_port);
DECLARE_HOOK(HOOK_CHIPSET_RESUME, resume_src_port, HOOK_PRIO_DEFAULT);

int chipset_in_state_mock(int state_mask)
{
	return state_mask & set_state;
}

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
	RESET_FAKE(check_src_port);
	RESET_FAKE(resume_src_port);
}

ZTEST(current_limit, test_check_src_port_1)
{
	const int fake_port = 0;
	const uint32_t fake_pdo[] = {
		PDO_FIXED(5000, 3000, PDO_FIXED_FLAGS),
	};

	charge_get_percent_fake.return_val = 20;
	tc_is_attached_src_fake.custom_fake = tc_is_attached_src_mock;
	chipset_in_state_fake.custom_fake = chipset_in_state_mock;
	set_state = CHIPSET_STATE_SUSPEND;
	hook_notify(HOOK_CHIPSET_SUSPEND);
	k_sleep(K_SECONDS(3));
	zassert_equal(1, check_src_port_fake.call_count);
	zassert_equal(1, dpm_get_source_pdo(fake_pdo, fake_port));
}

ZTEST(current_limit, test_check_src_port_2)
{
	charge_get_percent_fake.return_val = 40;
	tc_is_attached_src_fake.custom_fake = tc_is_attached_src_mock;
	chipset_in_state_fake.custom_fake = chipset_in_state_mock;
	set_state = CHIPSET_STATE_SUSPEND;
	hook_notify(HOOK_CHIPSET_SUSPEND);
	k_sleep(K_SECONDS(3));
	zassert_equal(1, check_src_port_fake.call_count);
}

ZTEST(current_limit, test_check_src_port_3)
{
	charge_get_percent_fake.return_val = 40;
	tc_is_attached_src_fake.custom_fake = tc_is_attached_src_mock;
	chipset_in_state_fake.custom_fake = chipset_in_state_mock;
	set_state = CHIPSET_STATE_SOFT_OFF;
	hook_notify(HOOK_CHIPSET_SUSPEND);
	k_sleep(K_SECONDS(33));
	zassert_equal(1, check_src_port_fake.call_count);
}

ZTEST(current_limit, test_check_src_port_4)
{
	tc_is_attached_src_fake.return_val = 0;
	hook_notify(HOOK_CHIPSET_SUSPEND);
	k_sleep(K_SECONDS(3));
	zassert_equal(1, check_src_port_fake.call_count);
}

ZTEST(current_limit, test_resume_src_port)
{
	const int fake_port = 0;
	const uint32_t fake_pdo[] = {
		PDO_FIXED(5000, 3000, PDO_FIXED_FLAGS),
	};

	tc_is_attached_src_fake.custom_fake = tc_is_attached_src_mock;
	chipset_in_state_fake.custom_fake = chipset_in_state_mock;
	set_state = CHIPSET_STATE_ON;
	hook_notify(HOOK_CHIPSET_RESUME);
	k_sleep(K_SECONDS(3));
	zassert_equal(1, resume_src_port_fake.call_count);
	zassert_equal(1, dpm_get_source_pdo(fake_pdo, fake_port));
}

ZTEST_SUITE(current_limit, NULL, NULL, current_limit_before, NULL, NULL);
