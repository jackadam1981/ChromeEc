/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "fan.h"
#include "thermal.h"

void board_override_fan_control(int fan, int *temp)
{
	if (chipset_in_state(CHIPSET_STATE_ON)) {
		fan_set_duty(fan, 94);
	}
}
