/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file tests the dead battery policies on type-C ports.
 */

#include "dead_battery_policy.h"
#include "emul/emul_pdc.h"
#include "fakes.h"

#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

/* Validate scenario where no battery is present and running on AC
 * only with AP on. */
ZTEST_USER_F(dead_battery_policy, test_dead_battery_policy_ac_only_ap_on)
{
	const struct pdc_fixture *pdc = &fixture->pdc[0];
	uint32_t rdo;
	union connector_status_t connector_status;

	set_battery_present(BP_NO);

	configure_dead_battery(pdc);

	verify_dead_battery_config(pdc->emul_pdc);

	/* PDC APIs provide unexpected behavior before driver init */
	pdc_driver_init();

	pdc_power_mgmt_wait_for_sync(pdc->port, -1);

	/* Make sure RDO is the same as before init */
	zassert_ok(pdc_power_mgmt_get_connector_status(pdc->port,
						       &connector_status));

	zassert_equal(connector_status.connect_status, 1, "port=%d", pdc->port);
	zassert_equal(connector_status.power_direction, 0, "port=%d",
		      pdc->port);
	zassert_equal(connector_status.sink_path_status, 1);
	zassert_ok(emul_pdc_get_rdo(pdc->emul_pdc, &rdo));
	zassert_equal(RDO_POS(rdo), 1);
}
