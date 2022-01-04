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
	if (cros_cbi_get_fw_config(dev, CBI_FW_CONFIG_FIELD_F1, &val) == 0) {
		switch (val) {
		default:
			CPRINTS("CBI FW: Unknown value (%d) for field %d",
				val, CBI_FW_CONFIG_FIELD_F1);
			break;

		case CBI_FW_CONFIG_VALUE_F1_OPTION_1:
			CPRINTS("CBI FW: Option 1");
			break;

		case CBI_FW_CONFIG_VALUE_F1_OPTION_2:
			CPRINTS("CBI FW: Option 2");
			break;
		}
	}
}
DECLARE_HOOK(HOOK_INIT, nissa_board_config_init, HOOK_PRIO_INIT_I2C - 1);
