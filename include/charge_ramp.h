/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Charge input current limit ramp header for Chrome EC */

#ifndef __CROS_EC_CHG_RAMP_H
#define __CROS_EC_CHG_RAMP_H

#include "timer.h"

/**
 * Check if ramping is allowed for given supplier
 *
 * @supplier Supplier to check
 *
 * @return Ramping is allowed for given supplier
 */
int board_is_ramp_allowed(int supplier);

/**
 * Get the maximum current limit that we are allowed to ramp to
 *
 * @supplier Active supplier type
 *
 * @return Maximum current in mA
 */
int board_get_ramp_current_limit(int supplier);

/**
 * Check if board is consuming full input current
 *
 * @return Board is consuming full input current
 */
int board_is_consuming_full_charge(void);

/**
 * Check if VBUS is too low
 *
 * @return VBUS is sagging low
 */
int board_is_vbus_too_low(void);

#ifdef HAS_TASK_CHG_RAMP
/**
 * Notify charge ramp module of supplier type change on a port. If port
 * is CHARGE_PORT_NONE, the call indicates the last charge supplier went
 * away.
 *
 * @port Active charging port
 * @supplier Active charging supplier
 * @registration_time Timestamp of when the supplier is registered
 */
void chg_ramp_charge_supplier_change(int port, int supplier,
				     timestamp_t registration_time);

/**
 * Set minimum input current limit for charge ramp module
 *
 * @current Minimum input current limit
 */
void chg_ramp_set_min_current(int current);
#else
static inline void chg_ramp_charge_supplier_change(
		int port, int supplier, timestamp_t registration_time) { }

/* Point directly to board function to set charge limit */
#define chg_ramp_set_min_current board_set_charge_limit
#endif

#endif /* __CROS_EC_CHG_RAMP_H */
