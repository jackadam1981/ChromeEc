/* Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* AC input current limit ramp header for Chrome EC */

#ifndef __CROS_EC_AC_RAMP_H
#define __CROS_EC_AC_RAMP_H

int board_is_full_charging(void);
int board_is_vbus_too_low(void);


void ac_ramp_charge_port_change(int new_port);
void ac_ramp_set_baseline_current(int current);

#endif /* __CROS_EC_AC_RAMP_H */
