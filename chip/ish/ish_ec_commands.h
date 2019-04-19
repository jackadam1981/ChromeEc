/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_ISH_EC_COMMANDS_H
#define __CROS_EC_ISH_EC_COMMANDS_H

#define EC_CMD_PRIVATE_ISH_DSLEEP  0x0001

/* D0ix statistics data, including each state's count and total stay time */
struct pm_statistics {
	uint64_t d0i0_cnt;
	uint64_t d0i0_time_us;

#ifdef CONFIG_ISH_PM_D0I1
	uint64_t d0i1_cnt;
	uint64_t d0i1_time_us;
#endif

#ifdef CONFIG_ISH_PM_D0I2
	uint64_t d0i2_cnt;
	uint64_t d0i2_time_us;
#endif

#ifdef CONFIG_ISH_PM_D0I3
	uint64_t d0i3_cnt;
	uint64_t d0i3_time_us;
#endif

} __packed;

enum ish_ec_cmd_dsleep {
	DEEP_SLEEP_ENABLE = 0,
	DEEP_SLEEP_DISABLE,
	DEEP_SLEEP_GET_STATS
};

struct ec_params_dsleep {
	uint8_t cmd;
} __ec_align4;

struct ec_response_dsleep {
	uint8_t status;
	int aon_valid;
	int dsleep_enabled;
	uint32_t aon_error_count;
	int aon_last_error;
	uint64_t total_time;
	struct pm_statistics pm_stats;
} __ec_align4;

#endif /* __CROS_EC_ISH_EC_COMMANDS_H*/
