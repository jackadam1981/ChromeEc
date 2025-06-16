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

ZTEST_USER_F(dead_battery_policy, test_dead_battery_policy_two_chargers)
{
	union connector_status_t connector_status;
	uint32_t rdo;

	for (int port = 0; port < CONFIG_USB_PD_PORT_MAX_COUNT; port++) {
		configure_dead_battery(&fixture->pdc[port]);
	}

	/* PDC APIs provide unexpected behavior before driver init */
	pdc_driver_init();

	/* Verify each port is configured as dead battery */
	for (int port = 0; port < CONFIG_USB_PD_PORT_MAX_COUNT; port++) {
		verify_dead_battery_config(fixture->pdc[port].emul_pdc);
	}

	/* Allow initialization to occur, verification of dead battery RDO
	 * selection comes from custom_fake_pdc_set_rdo */
	for (int port = 0; port < CONFIG_USB_PD_PORT_MAX_COUNT; port++) {
		pdc_power_mgmt_wait_for_sync(port, -1);
	}

	/* Verify after initialization both ports are connected as sink but
	 * only one has sink path enabled */
	int charge_port = 0;
	int num_charge_ports = 0;
	for (int port = 0; port < CONFIG_USB_PD_PORT_MAX_COUNT; port++) {
		zassert_ok(pdc_power_mgmt_get_connector_status(
			port, &connector_status));

		zassert_equal(connector_status.connect_status, 1, "port=%d",
			      port);
		zassert_equal(connector_status.power_direction, 0, "port=%d",
			      port);

		/* Verify dead battery is cleared */
		zassert_false(
			emul_pdc_get_dead_battery(fixture->pdc[port].emul_pdc),
			"port=%d", port);

		if (connector_status.sink_path_status) {
			charge_port = port;
			num_charge_ports++;
		}
	}

	zassert_equal(num_charge_ports, 1);

	/* Verify correct RDO is selected on PORT1 */
	zassert_ok(emul_pdc_get_rdo(fixture->pdc[charge_port].emul_pdc, &rdo));
	zassert_equal(RDO_POS(rdo), 3);
}
