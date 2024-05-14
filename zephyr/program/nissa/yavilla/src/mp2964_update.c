/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Tune the MP2964 IMVP9.1 parameters for yavilla */

#include "common.h"
#include "console.h"
#include "driver/mp2964.h"
#include "hooks.h"
#include "i2c.h"

const static struct mp2964_reg_val rail_a[] = {
	{ 0x51, 0x1234 },
};

const static struct mp2964_reg_val rail_b[] = {};

static void mp2964_lock(void)
{
	static int chip_locked;
	int status;

	if (chip_locked)
		goto set_i2c_400k;

	ccprintf("%s\n", __func__);
	status = mp2964_tune(rail_a, ARRAY_SIZE(rail_a), rail_b,
			     ARRAY_SIZE(rail_b));

set_i2c_400k:
	i2c_set_freq(I2C_PORT_SENSOR, I2C_FREQ_400KHZ);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, mp2964_lock, HOOK_PRIO_DEFAULT);
