/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack specific functions.
 */

#include "battery_pack.h"

int battery_start_charging_range(int deci_k)
{
	return (deci_k >= CELSIUS_TO_DECI_KELVIN(5) &&
		deci_k < CELSIUS_TO_DECI_KELVIN(45));
}

int battery_charging_range(int deci_k)
{
	return (deci_k >= CELSIUS_TO_DECI_KELVIN(5) &&
		deci_k < CELSIUS_TO_DECI_KELVIN(60));
}

int battery_discharging_range(int deci_k)
{
	return (deci_k >= CELSIUS_TO_DECI_KELVIN(0) &&
		deci_k < CELSIUS_TO_DECI_KELVIN(100));
}

