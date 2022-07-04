/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Shotzo doesn't have battery, but needs charger to convert type-c power
 * to Vsys.
 * Add a fake battery info to set charger voltage.
 */

#include "charge_state.h"

static const struct battery_info info = {
	.voltage_max = 8600, /* mV */
	.voltage_normal = 8500,
	.voltage_min = 8400,
};

const struct battery_info *battery_get_info(void)
{
	return &info;
}
