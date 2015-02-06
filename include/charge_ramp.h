/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Charge input current limit ramp header for Chrome EC */

#ifndef __CROS_EC_CHG_RAMP_H
#define __CROS_EC_CHG_RAMP_H

/* Return if ramping is allowed for given supplier */
int board_is_ramp_allowed(int supplier);

/* Return if board is consuming full input current */
int board_is_consuming_full_charge(void);

/* Return if VBUS is sagging low */
int board_is_vbus_too_low(void);

#ifdef HAS_TASK_CHG_RAMP
/*
 * Notify charge ramp module of supplier type change on a port. If port
 * is CHARGE_PORT_NONE, the call indicates the last charge supplier went
 * away.
 */
void chg_ramp_charge_supplier_change(int port, int supplier);

/* Set minimum input current limit for charge ramp module */
void chg_ramp_set_min_current(int current);
#else
static inline void chg_ramp_charge_supplier_change(int port, int supplier) { }
#define chg_ramp_set_min_current board_set_charge_limit
#endif

#endif /* __CROS_EC_CHG_RAMP_H */
