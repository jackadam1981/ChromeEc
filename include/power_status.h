/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */
#ifndef __CROS_EC_POWER_STATUS_H
#define __CROS_EC_POWER_STATUS_H

struct dbpt_batt_params {
        uint16_t sys_resistance;
	uint16_t min_sys_volt;
	uint8_t charger_type;
};

/* Forward declare board specific data to be used by common code */
extern const struct dbpt_batt_params batt_param;

enum dtt_charger_type {
	TRADITIONAL = 1,
	HYBRID,
	NVDC,
};
#endif /* __CROS_EC_POWER_STATUS_H */
