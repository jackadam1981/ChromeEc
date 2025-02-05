/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file tests the sourcing policies on type-C ports.  See the diagram
 * under "ChromeOS as Source - Policy for Type-C" in the usb_power.md.
 */

#include "chipset.h"
#include "emul/emul_pdc.h"
#include "test/util.h"
#include "timer.h"
#include "usbc/pdc_power_mgmt.h"
#include "usbc/utils.h"

#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

LOG_MODULE_REGISTER(pdc_dead_battery_policy);

DECLARE_FAKE_VALUE_FUNC(int, chipset_in_state, int);

BUILD_ASSERT(
	CONFIG_USB_PD_PORT_MAX_COUNT == 2,
	"PDC dead battery policy test suite must supply exactly 2 PDC ports");

#define PDC_TEST_TIMEOUT 2000

/* TODO: b/343760437 - Once the emulator can detect the PDC threads are idle,
 * remove the sleep delay to let the policy code run.
 */
#define PDC_POLICY_DELAY_MS 500
#define PDC_NODE_PORT0 DT_NODELABEL(pdc_emul1)
#define PDC_NODE_PORT1 DT_NODELABEL(pdc_emul2)

#define TEST_USBC_PORT0 USBC_PORT_FROM_DRIVER_NODE(PDC_NODE_PORT0, pdc)
#define TEST_USBC_PORT1 USBC_PORT_FROM_DRIVER_NODE(PDC_NODE_PORT1, pdc)

static void clear_partner_pdos(const struct emul *e, enum pdo_type_t type)
{
	uint32_t clear_pdos[PDO_MAX_OBJECTS] = { 0 };

	emul_pdc_set_pdos(e, type, PDO_OFFSET_0, ARRAY_SIZE(clear_pdos),
			  PARTNER_PDO, clear_pdos);
}

struct dead_battery_policy_fixture {
	const struct emul *emul_pdc[CONFIG_USB_PD_PORT_MAX_COUNT];
};

static enum chipset_state_mask fake_chipset_state = CHIPSET_STATE_ON;

static int custom_fake_chipset_in_state(int mask)
{
	return !!(fake_chipset_state & mask);
}

static void *dead_battery_policy_setup(void)
{
	static struct dead_battery_policy_fixture fixture;

	fixture.emul_pdc[0] = EMUL_DT_GET(PDC_NODE_PORT0);
	fixture.emul_pdc[1] = EMUL_DT_GET(PDC_NODE_PORT1);

	return &fixture;
};

static void dead_battery_policy_before(void *f)
{
	struct dead_battery_policy_fixture *fixture = f;

	union connector_status_t connector_status_port;
	uint32_t pdos[] = {
		PDO_FIXED(5000, 1500, 0),
		PDO_FIXED(9000, 3000, 0),
		PDO_FIXED(20000, 3000, 0),
	};

	RESET_FAKE(chipset_in_state);

	chipset_in_state_fake.custom_fake = custom_fake_chipset_in_state;

	for (int i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
		/* Start with both ports disconnected. */
		zassert_ok(emul_pdc_disconnect(fixture->emul_pdc[i]));

		zassert_true(TEST_WAIT_FOR(!pdc_power_mgmt_is_connected(i),
					   PDC_TEST_TIMEOUT));

		emul_pdc_configure_snk(fixture->emul_pdc[i],
				       &connector_status_port);
		clear_partner_pdos(fixture->emul_pdc[i], SOURCE_PDO);
		zassert_ok(emul_pdc_set_pdos(fixture->emul_pdc[i], SOURCE_PDO,
					     PDO_OFFSET_0, ARRAY_SIZE(pdos),
					     PARTNER_PDO, pdos));

		zassert_ok(emul_pdc_connect_partner(fixture->emul_pdc[i],
						    &connector_status_port));
		zassert_ok(pdc_power_mgmt_wait_for_sync(i, -1));
	}
}

static void dead_battery_policy_after(void *f)
{
	struct dead_battery_policy_fixture *fixture = f;

	for (int i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
		/* Start with both ports disconnected. */
		zassert_ok(emul_pdc_disconnect(fixture->emul_pdc[i]));

		zassert_true(TEST_WAIT_FOR(!pdc_power_mgmt_is_connected(i),
					   PDC_TEST_TIMEOUT));
	}
}

ZTEST_SUITE(dead_battery_policy, NULL, dead_battery_policy_setup,
	    dead_battery_policy_before, dead_battery_policy_after, NULL);

ZTEST_USER_F(dead_battery_policy, test_dead_battery_policy)
{
	union connector_status_t connector_status;
	uint32_t rdo, pdo;

	/* Verify both ports connected is capped to 5v. */
	for (int i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
		zassert_ok(pdc_power_mgmt_get_connector_status(
			i, &connector_status));

		zassert_equal(connector_status.connect_status, 1, "port=%d", i);
		zassert_equal(connector_status.power_direction, 0, "port=%d",
			      i);

		/* TODO - upon dead battery startup both ports should indicate
		sink_path enabled */
		zassert_equal(connector_status.sink_path_status,
			      (i == 0 ? 1 : 0), "port=%d", i);

		zassert_ok(emul_pdc_get_rdo(fixture->emul_pdc[i], &rdo));
		emul_pdc_get_pdos(fixture->emul_pdc[i], SOURCE_PDO,
				  RDO_POS(rdo) - 1, 1, PARTNER_PDO, &pdo);
		/* TODO - uncomment when emulating dead battery is ready
		zassert_equal(PDO_FIXED_VOLTAGE(pdo), 5000,
			      "RDO_POS=%d, pdo voltage=%u, expected=%u",
			      RDO_POS(rdo), PDO_FIXED_VOLTAGE(pdo), 5000);
		*/
	}
}
