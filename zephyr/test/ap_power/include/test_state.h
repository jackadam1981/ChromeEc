/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_TEST_AP_POWER_INCLUDE_TEST_STATE_H_
#define ZEPHYR_TEST_AP_POWER_INCLUDE_TEST_STATE_H_

#define AP_PWR_TEST_PLATFORM_ENUM_WITH_COMA(inst)       \
	DT_STRING_TOKEN(inst, name_id),

enum test_platforms_id {
DT_FOREACH_STATUS_OKAY(ap_pwr_test_platform, AP_PWR_TEST_PLATFORM_ENUM_WITH_COMA)
	TEST_ID_COUNT,
};

struct test_state {
	bool ec_app_main_run;
};

bool ap_power_predicate_pre_main(const void *state);

bool ap_power_predicate_post_main(const void *state);

void power_signal_emul_load(enum test_platforms_id test_id);

#endif /* ZEPHYR_TEST_AP_POWER_INCLUDE_TEST_STATE_H_ */
