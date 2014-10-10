/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "console.h"
#include "hooks.h"
#include "usb_pd_config.h"
#include "util.h"

/* Keep track of available charge for each charge port. */
static int available_charge_ma[CHARGE_SUPPLIER_COUNT][PD_PORT_COUNT];

/* Store current state of port enable / charge. */
static int charge_port = CHARGE_PORT_NONE;
static int charge_ma = CHARGE_CURRENT_UNINITIALIZED;

/**
 * Initialize charge currents. Run before board init, so board init can
 * initialize charge current, if needed.
 */
static void charge_manager_init(void)
{
	int i, j;

	for (i = 0; i < CHARGE_SUPPLIER_COUNT; ++i)
		for (j = 0; j < PD_PORT_COUNT; ++j)
			available_charge_ma[i][j] =
				CHARGE_CURRENT_UNINITIALIZED;
}
DECLARE_HOOK(HOOK_INIT, charge_manager_init, HOOK_PRIO_DEFAULT-1);

/**
 * Returns 1 if all ports + suppliers have reported in with some initial charge,
 * 0 otherwise.
 */
static int charge_manager_is_seeded(void)
{
	/* Once we're seeded, we don't need to check again. */
	static int is_seeded;
	int i, j;

	if (is_seeded)
		return 1;

	for (i = 0; i < CHARGE_SUPPLIER_COUNT; ++i)
		for (j = 0; j < PD_PORT_COUNT; ++j)
			if (available_charge_ma[i][j] ==
			    CHARGE_CURRENT_UNINITIALIZED)
				return 0;
	is_seeded = 1;
	return 1;
}

/**
 * Charge manager refresh -- responsible for selecting the active charge port
 * and charge current. Called as a deferred task.
 */
static void charge_manager_refresh(void)
{
	/**
	 * Use a copy of charge table so it doesn't change mid-calculation.
	 * Static variable to save stack space.
	 */
	static int charge_copy[CHARGE_SUPPLIER_COUNT][PD_PORT_COUNT];
	enum charge_supplier new_supplier = CHARGE_SUPPLIER_NONE;
	int new_port = CHARGE_PORT_NONE;
	int new_charge_ma, i, j;

	memcpy(charge_copy, available_charge_ma, sizeof(available_charge_ma));

	/*
	 * Charge supplier selection logic:
	 * 1. Prefer higher priority (lower CHARGE_SUPPLIER index) supply.
	 * 2. Prefer higher current over lower.
	 */
	for (i = 0; i < CHARGE_SUPPLIER_COUNT; ++i)
		for (j = 0; j < PD_PORT_COUNT; ++j)
			if (charge_copy[i][j] > 0) {
				new_supplier = i;
				new_port = j;
				goto got_supplier;
			}

got_supplier:
	if (new_supplier != CHARGE_SUPPLIER_NONE)
		for (i = new_port + 1; i < PD_PORT_COUNT; ++i)
			if (charge_copy[new_supplier][i] >
			    charge_copy[new_supplier][new_port])
				new_port = i;

	if (new_supplier == CHARGE_SUPPLIER_NONE)
		new_charge_ma = 0;
	else
		new_charge_ma = charge_copy[new_supplier][new_port];

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
 * @param supplier	Charge supplier to update.
 * @param charge_port	Charge port to update.
 * @param charge_ma	Updated charge (mA).
 */
void charge_manager_update(enum charge_supplier supplier,
			   int charge_port,
			   int charge_ma)
{
	/* Update charge table if needed. */
	if (available_charge_ma[supplier][charge_port] != charge_ma) {
		available_charge_ma[supplier][charge_port] = charge_ma;

		/**
		 * Don't call charge_manager_refresh unless all ports +
		 * suppliers have reported in. We don't want to make changes
		 * to our charge port until we are certain we know what is
		 * attached.
		 */
		if (charge_manager_is_seeded())
			hook_call_deferred(charge_manager_refresh, 0);
	}
}
