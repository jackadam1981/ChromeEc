/* Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
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

/* Set active charging port for the charge ramp module */
void chg_ramp_charge_port_change(int new_port);

/* Set minimum input current limit for charge ramp module */
void chg_ramp_set_min_current(int current);

#endif /* __CROS_EC_CHG_RAMP_H */
