/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STRUCT_H
#define __CROS_EC_STRUCT_H
#include "battery.h"
#include "driver/battery/max17055.h"

/* Do not change the enum values. We directly use strap gpio level to index. */
enum battery_type {
	BATTERY_SIMPLO = 0,
	BATTERY_AETECH,
	BATTERY_COUNT
};

static const struct battery_info info[] = {
	[BATTERY_SIMPLO] = {
		.voltage_max            = 4400,
		.voltage_normal         = 3840,
		.voltage_min            = 3000,
		.precharge_current      = 256,
		.start_charging_min_c   = 0,
		.start_charging_max_c   = 45,
		.charging_min_c         = 0,
		.charging_max_c         = 60,
		.discharging_min_c      = -20,
		.discharging_max_c      = 60,
	},
	[BATTERY_AETECH] = {
		.voltage_max            = 4350,
		.voltage_normal         = 3800,
		.voltage_min            = 3000,
		.precharge_current      = 700,
		.start_charging_min_c   = 0,
		.start_charging_max_c   = 45,
		.charging_min_c         = 0,
		.charging_max_c         = 45,
		.discharging_min_c      = -20,
		.discharging_max_c      = 55,
	}
};
const struct battery_info *ptr_battery_info = &info[0];
const uint16_t battery_info_size = sizeof(struct battery_info) * 2;

static const struct max17055_batt_profile batt_profile[] = {
	[BATTERY_SIMPLO] = {
		.is_ez_config           = 0,
		.design_cap             = 0x221e, /* 8734mAh */
		.ichg_term              = 0x589, /* 443 mA */
		/* Empty voltage = 3000mV, Recovery voltage = 3600mV */
		.v_empty_detect         = 0x965a,
		.learn_cfg              = 0x4406,
		.dpacc                  = 0x0c7a,
		.rcomp0                 = 0x0062,
		.tempco                 = 0x1327,
		.qr_table00             = 0x1680,
		.qr_table10             = 0x0900,
		.qr_table20             = 0x0280,
		.qr_table30             = 0x0280,
},
	[BATTERY_AETECH] = {
		.is_ez_config           = 0,
		.design_cap             = 0x232f, /* 9007mAh */
		.ichg_term              = 0x0240, /* 180mA */
		/* Empty voltage = 2700mV, Recovery voltage = 3280mV */
		.v_empty_detect         = 0x8752,
		.learn_cfg              = 0x4476,
		.dpacc                  = 0x0c7b,
		.rcomp0                 = 0x0077,
		.tempco                 = 0x1d3f,
		.qr_table00             = 0x1200,
		.qr_table10             = 0x0900,
		.qr_table20             = 0x0480,
		.qr_table30             = 0x0480,
	},
};
const struct max17055_batt_profile *ptr_batt_profile = &batt_profile[0];
const uint16_t batt_profile_size = sizeof(struct max17055_batt_profile) * 2;

#endif /* __CROS_EC_STRUCT_H */
