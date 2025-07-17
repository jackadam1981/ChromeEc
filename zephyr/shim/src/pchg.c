/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio_signal.h"
#include "peripheral_charger.h"
#include "wpc/scp8601.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

#define PCHG_INIT(inst) WPC_CHIP_CPS8601(DT_INST(inst, CPS8601_PCHG_COMPAT))

struct pchg pchgs[] = { DT_INST_FOREACH_STATUS_OKAY(PCHG_INIT) };

unsigned int pchg_count = ARRAY_SIZE(pchgs);

int board_get_pchg_count(void)
{
	return ARRAY_SIZE(pchgs);
}
