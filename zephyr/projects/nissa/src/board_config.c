/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nissa daughter board detection */

#include <drivers/cros_cbi.h>
#include "console.h"
#include "hooks.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

static void nissa_board_config_init(void)
{
	uint32_t val;
	const struct device *dev = device_get_binding(CROS_CBI_LABEL);

	if (dev == NULL) {
		CPRINTS("No %s device", CROS_CBI_LABEL);
		return;
	}
	CPRINTS("CBI FW: field X: %d, field Y: %d", FW_CONFIG_X, FW_CONFIG_Y);
	if (cros_cbi_get_fw_config(dev, FW_CONFIG_X, &val) == 0) {
		switch (val) {
		default:
			CPRINTS("CBI FW: Unknown value (%d) for field %d",
				val, FW_CONFIG_X);
			break;

		case FW_X_OPTION_1:
			CPRINTS("CBI FW: Option 1");
			break;

		case FW_X_OPTION_2:
			CPRINTS("CBI FW: Option 2");
			break;
		}
	}
}
DECLARE_HOOK(HOOK_INIT, nissa_board_config_init, HOOK_PRIO_INIT_I2C - 1);
