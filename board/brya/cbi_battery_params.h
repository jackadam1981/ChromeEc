/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#ifndef __CBI_BATTERY_PARAMS_H
#define __CBI_BATTERY_PARAMS_H

#include "compile_time_macros.h"
#include "battery_fuel_gauge.h"

#define CBI_BATTERY_INFO_APPLIED_VERSION 1

enum cbi_battery_type {
	CBI_BATTERY_POWER_TECH,
	CBI_BATTERY_TYPE_COUNT
};

extern struct board_batt_params cbi_board_battery_info[];

#endif /* __CBI_BATTERY_PARAMS_H */
