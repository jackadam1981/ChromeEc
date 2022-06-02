/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EMUL_POWER_SIGNALS_H_
#define EMUL_POWER_SIGNALS_H_

#define AP_PWR_TEST_PLATFORM_ENUM_WITH_COMA(inst) \
	DT_STRING_TOKEN(inst, name_id),

/**
 * @brief Contains all test_platform's nodes found in devicetree.
 */
enum test_platforms_id {
	DT_FOREACH_STATUS_OKAY(ap_pwr_test_platform,
			       AP_PWR_TEST_PLATFORM_ENUM_WITH_COMA)
		TEST_ID_COUNT,
};

/**
 * @brief Load test platform.
 *
 * This initialize each of the test platform nodes.
 *
 * @param test_id Test platform enumeration ID.
 *
 * @return 0 indicating success.
 * @return -EINVAL `test_id` parameter is invalid.
 * @return -EBUSY `test_id` One test platform is currently loaded.
 */
int power_signal_emul_load(enum test_platforms_id test_id);

/**
 * @brief Unload test platform.
 *
 * @return 0 indicating success.
 * @return -EINVAL no test platform has been loaded.
 */
int power_signal_emul_unload(void);

#endif /* EMUL_POWER_SIGNALS_H_ */
