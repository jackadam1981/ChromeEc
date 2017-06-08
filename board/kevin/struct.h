/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STRUCT_H
#define __CROS_EC_STRUCT_H
#include "battery.h"

#ifdef BOARD_KEVIN

const struct battery_info info = {
	.voltage_max            = 8688, /* 8700mA, round down for chg reg */
	.voltage_normal         = 7600,
	.voltage_min            = 6000,
	.precharge_current      = 200,
	.start_charging_min_c   = 0,
	.start_charging_max_c   = 45,
	.charging_min_c         = 0,
	.charging_max_c         = 60,
	.discharging_min_c      = -20,
	.discharging_max_c      = 70,
};
const struct battery_info *ptr_info = &info;

#else

const struct battery_info *ptr_info;

#endif

#endif /* __CROS_EC_STRUCT_H */
