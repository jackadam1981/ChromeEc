/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Stubs needed for fuzz testing the USB TCPMv2 state machines.
 */

#define HIDE_EC_STDLIB
#include "usb_pd.h"
#include "battery.h"
#include "charge_manager.h"

const struct svdm_response svdm_rsp = {
	.identity = NULL,
	.svids = NULL,
	.modes = NULL,
};

int pd_check_vconn_swap(int port)
{
	return 1;
}

void charge_manager_set_ceil(int port, enum ceil_requestor requestor, int ceil)
{
}

#define BATTERY_DESIGN_VOLTAGE 7600
#define BATTERY_DESIGN_CAPACITY 5131
#define BATTERY_FULL_CHARGE_CAPACITY 5131
#define BATTERY_REMAINING_CAPACITY 2566

enum battery_present battery_is_present(void)
{
	return BP_YES;
}

int battery_design_capacity(int *capacity)
{
	*capacity = BATTERY_DESIGN_CAPACITY;
	return 0;
}

int battery_design_voltage(int *voltage)
{
	*voltage = BATTERY_DESIGN_VOLTAGE;
	return 0;
}

int battery_remaining_capacity(int *capacity)
{
	*capacity = BATTERY_REMAINING_CAPACITY;
	return 0;
}

int battery_full_charge_capacity(int *capacity)
{
	*capacity = BATTERY_FULL_CHARGE_CAPACITY;
	return 0;
}

int battery_status(int *status)
{
	*status = 1;
	return 0;
}
