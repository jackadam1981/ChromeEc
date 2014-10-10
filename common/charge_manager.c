/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "console.h"
#include "hooks.h"
#include "usb_pd_config.h"
#include "util.h"

/**
 * Keep track of available charge for each charge port -- assume each port
 * can provide 500mA until told otherwise.
 */
static int available_charge_ma[PD_PORT_COUNT][CHARGE_SUPPLIER_COUNT] = {
	{ 500 }
};

/* Store current state of port enable / charge. */
static int charge_port = CHARGE_PORT_NONE;
static int charge_ma = CHARGE_CURRENT_UNINITIALIZED;

/**
 * Charge manager refresh -- responsible for selecting the active charge port
 * and charge current. Called as a deferred task.
 */
static void charge_manager_refresh(void)
{
	int charge_copy[PD_PORT_COUNT][CHARGE_SUPPLIER_COUNT];
	enum charge_supplier new_supplier;
	int new_port, new_charge_ma, i;

	/* Use a copy of charge table so it doesn't change mid-calculation. */
	for (i = 0; i < PD_PORT_COUNT; ++i)
		memcpy(charge_copy[i],
		       available_charge_ma[i],
		       sizeof(available_charge_ma[i]));

	/*
	 * Charge supplier selection logic:
	 * 1. Prefer PD over BC1.2.
	 * 2. Prefer higher current over lower.
	 */
	new_supplier = CHARGE_SUPPLIER_BC12;

	/* Check for a PD port we can charge from. */
	for (i = 0; i < PD_PORT_COUNT; ++i)
		if (charge_copy[i][CHARGE_SUPPLIER_PD] > 0) {
			new_supplier = CHARGE_SUPPLIER_PD;
			break;
		}

	/* Check for the port which can supply the most current. */
	new_port = CHARGE_PORT_NONE;
	for (i = 0; i < PD_PORT_COUNT; ++i)
		if (charge_copy[i][new_supplier] > 0)
			if (new_port == CHARGE_PORT_NONE ||
			    charge_copy[i][new_supplier] >
			    charge_copy[new_port][new_supplier])
				new_port = i;

	/* Update chosen port + available charge. */
	if (new_port == CHARGE_PORT_NONE)
		new_supplier = CHARGE_SUPPLIER_NONE;

	if (new_supplier == CHARGE_SUPPLIER_NONE)
		new_charge_ma = 0;
	else
		new_charge_ma = charge_copy[new_port][new_supplier];

	/* Change the charge limit + charge port if changed. */
	if (new_port != charge_port || new_charge_ma != charge_ma) {
		board_set_charge_limit(new_charge_ma);
		board_set_active_charge_port(new_port);

		charge_ma = new_charge_ma;
		charge_port = new_port;
	}
}
DECLARE_DEFERRED(charge_manager_refresh);

/**
 * Update available charge for a given port / supplier.
 *
 * @param charge_port	Charge port to update.
 * @param supplier	Charge supplier to update.
 * @param charge_ma	Updated charge (mA).
 */
void charge_manager_update(int charge_port,
			   enum charge_supplier supplier,
			   int charge_ma)
{
	/* Update charge table if needed. */
	if (available_charge_ma[charge_port][supplier] != charge_ma) {
		available_charge_ma[charge_port][supplier] = charge_ma;
		hook_call_deferred(charge_manager_refresh, 0);
	}
}
