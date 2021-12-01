/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "i2c/i2c.h"
#include "i2c.h"
#include "hooks.h"
#include "motionsense_sensors.h"
#include "system.h"

static void check_alternate_devices(void)
{
	MOTIONSENSE_PROBE_AND_ENABLE_ALTERNATE(alt_lid_accel);

	if (system_get_sku_id() != 99) {
		MOTIONSENSE_ENABLE_ALTERNATE(alt_lid_accel);
	}
}
DECLARE_HOOK(HOOK_INIT, check_alternate_devices, HOOK_PRIO_INIT_I2C + 2);
