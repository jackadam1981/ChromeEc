/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file tests the dead battery policies on type-C ports.
 */

#include "dead_battery_policy.h"
#include "emul/emul_pdc.h"

#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

/* Validate scenario where no battery is present and running on AC
 * only. */
ZTEST_USER_F(dead_battery_policy, test_dead_battery_policy_ac_only)
{
	const struct pdc_fixture *pdc = &fixture->pdc[0];

	configure_dead_battery(pdc);

	verify_dead_battery_config(pdc->emul_pdc);

	/* PDC APIs provide unexpected behavior before driver init */
	pdc_driver_init();

	/* TODO(b/397148920) - do not call SET_RDO on PDC if currently
	 * sinking from that port and no battery is present */
}
