/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "i2c/i2c.h"
#include "i2c.h"
#include "hooks.h"
#include "motionsense_sensors.h"

static void check_alternate_devices(void)
{
	MOTIONSENSE_SSFC_ENABLE_ALTERNATE();
}
DECLARE_HOOK(HOOK_INIT, check_alternate_devices, HOOK_PRIO_INIT_I2C + 2);
