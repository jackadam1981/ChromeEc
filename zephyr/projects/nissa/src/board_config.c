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
	CPRINTS("FW_CONFIG fields = %d", CBI_FW_CONFIG_FIELD_COUNT);
}
DECLARE_HOOK(HOOK_INIT, nissa_board_config_init, HOOK_PRIO_INIT_I2C - 1);
