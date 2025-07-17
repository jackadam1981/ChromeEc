/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio_signal.h"
#include "peripheral_charger.h"
#include "wpc/scp8200.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

#if DT_HAS_COMPAT_STATUS_OKAY(CPS8200_PCHG_COMPAT)

#define WPC_CHIP_ELE(id, fn) fn(id),

struct pchg pchgs[] = { DT_FOREACH_STATUS_OKAY_VARGS(
	CPS8200_PCHG_COMPAT, WPC_CHIP_ELE, WPC_CHIP_CPS8200) };

unsigned int pchg_count = ARRAY_SIZE(pchgs);

int board_get_pchg_count(void)
{
	return pchg_count;
}
#endif
