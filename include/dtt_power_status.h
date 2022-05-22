/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */
#ifndef __CROS_EC_DTT_POWER_STATUS_H
#define __CROS_EC_DTT_POWER_STATUS_H

#define PMAX_THRESHOLD_MW 250
#define PBSS_THRESHOLD_MW 100
#define RBFH_THRESHOLD_MOHM 5
#define VBNL_THRESHOLD_MV 50
#define CMPP_THRESHOLD_MA 100

struct dbpt_batt_params {
        uint16_t sys_resistance;
	uint16_t min_sys_volt;
};

/* Forward declare board specific data to be used by common code */
extern const struct dbpt_batt_params batt_param;

#endif /* __CROS_EC_DTT_POWER_STATUS_H */
