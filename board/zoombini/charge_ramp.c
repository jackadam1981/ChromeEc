/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "charge_ramp.h"
#include "driver/tcpm/tcpm.h"

int board_is_vbus_too_low(int port, enum chg_ramp_vbus_state ramp_state)
{
	/*
	 * The only method for detecting Vbus on Zoombini is via the TCPCs.
	 * Furthermore, it only indicates if Vbus is present and not what the
	 * actual voltage is...  Therefore, we can't tell if Vbus is too low.
	 */
	return 0;
}
