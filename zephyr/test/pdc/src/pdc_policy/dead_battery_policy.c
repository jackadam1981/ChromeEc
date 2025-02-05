/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file tests the sourcing policies on type-C ports.  See the diagram
 * under "ChromeOS as Source - Policy for Type-C" in the usb_power.md.
 */

#include "chipset.h"
// #include "drivers/pdc.h"
#include "emul/emul_pdc.h"
#include "test/util.h"
#include "timer.h"
#include "usbc/pdc_power_mgmt.h"
#include "usbc/utils.h"
#include "zephyr/sys/util.h"
#include "zephyr/sys/util_macro.h"

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

FAKE_VALUE_FUNC(int, chipset_in_state, int);
FAKE_VALUE_FUNC(int, sniff_pdc_set_sink_path, const struct device *, bool);
FAKE_VALUE_FUNC(int, sniff_pdc_set_rdo, const struct device *, uint32_t);

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

#define USBC_NODE0 DT_NODELABEL(usbc0)
#define USBC_NODE1 DT_NODELABEL(usbc1)

#define TEST_USBC_PORT0 USBC_PORT_FROM_DRIVER_NODE(PDC_NODE_PORT0, pdc)
#define TEST_USBC_PORT1 USBC_PORT_FROM_DRIVER_NODE(PDC_NODE_PORT1, pdc)

#define FOREACH_USB_PD_PORT \
	for (int port = 0; port < CONFIG_USB_PD_PORT_MAX_COUNT; port++)

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

static uint8_t sink_path_en_mask;

static int pdc_dev_to_port(const struct device *dev)
{
	const struct device *devs[] = {
		DEVICE_DT_GET(PDC_NODE_PORT0), /* PDC Drivers */
		DEVICE_DT_GET(PDC_NODE_PORT1),
	};

	FOREACH_USB_PD_PORT
	{
		if (dev == devs[port])
			return port;
	}

	zassert_true(0, "Unable to find port");

	return -1;
}

static int custom_fake_pdc_set_sink_path(const struct device *dev, bool en)
{
	int port = pdc_dev_to_port(dev);

	WRITE_BIT(sink_path_en_mask, port, en);
	LOG_INF("FAKE C%d: pdc_set_sink_path en_mask=0x%X", port,
		sink_path_en_mask);

	return pdc_set_sink_path(dev, en);
}

static int custom_fake_pdc_set_rdo(const struct device *dev, uint32_t rdo)
{
	int port = pdc_dev_to_port(dev);

	LOG_INF("FAKE C%d: pdc_set_rdo en_mask=0x%X", port, sink_path_en_mask);

	/* Assert only one sink path is enabled before changing RDOs */
	zassert_true(IS_POWER_OF_TWO(sink_path_en_mask));

	return pdc_set_rdo(dev, rdo);
}

static void pdc_driver_init(void)
{
	const struct device *devs[] = {
		DEVICE_DT_GET(PDC_NODE_PORT0), /* PDC Drivers */
		DEVICE_DT_GET(PDC_NODE_PORT1),
		DEVICE_DT_GET(USBC_NODE0), /* PDC Power Management */
		DEVICE_DT_GET(USBC_NODE1),
	};

	for (int i = 0; i < ARRAY_SIZE(devs); i++) {
		zassert_false(device_is_ready(devs[i]));
		zassert_ok(device_init(devs[i]));
	}
}

static int configure_dead_battery(const struct emul *e, int port)
{
	union connector_status_t cs;
	/* PORT1 will have best PDO at RDO_POS=3 -- Validate this after init */
	uint32_t pdos[] = {
		PDO_FIXED(5000, 1500, 0),
		PDO_FIXED(9000, 3000, 0),
		PDO_FIXED((port ? 20000 : 12000), 3000, 0),
	};

	zassert_ok(emul_pdc_set_dead_battery(e, 1));
	emul_pdc_configure_snk(e, &cs);
	clear_partner_pdos(e, SOURCE_PDO);
	zassert_ok(emul_pdc_set_pdos(e, SOURCE_PDO, PDO_OFFSET_0,
				     ARRAY_SIZE(pdos), PARTNER_PDO, pdos));

	emul_pdc_set_rdo(e, RDO_FIXED(1, 1500, 1500, 0));
	cs.sink_path_status = 1;
	zassert_ok(emul_pdc_connect_partner(e, &cs));

	return 0;
}

/* Verify port is capped to 5v. */
static void verify_dead_battery_config(const struct emul *e)
{
	uint32_t rdo, pdo;

	zassert_ok(emul_pdc_get_rdo(e, &rdo));
	emul_pdc_get_pdos(e, SOURCE_PDO, RDO_POS(rdo) - 1, 1, PARTNER_PDO,
			  &pdo);

	zassert_equal(PDO_FIXED_VOLTAGE(pdo), 5000,
		      "RDO_POS=%d, pdo voltage=%u, expected=%u", RDO_POS(rdo),
		      PDO_FIXED_VOLTAGE(pdo), 5000);

	zassert_true(emul_pdc_get_dead_battery(e));
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

	RESET_FAKE(chipset_in_state);
	RESET_FAKE(sniff_pdc_set_sink_path);
	RESET_FAKE(sniff_pdc_set_rdo);

	chipset_in_state_fake.custom_fake = custom_fake_chipset_in_state;
	sniff_pdc_set_sink_path_fake.custom_fake =
		custom_fake_pdc_set_sink_path;
	sniff_pdc_set_rdo_fake.custom_fake = custom_fake_pdc_set_rdo;

	sink_path_en_mask = BIT_MASK(CONFIG_USB_PD_PORT_MAX_COUNT);
	FOREACH_USB_PD_PORT
	{
		configure_dead_battery(fixture->emul_pdc[port], port);
	}
}

ZTEST_SUITE(dead_battery_policy, NULL, dead_battery_policy_setup,
	    dead_battery_policy_before, NULL, NULL);

ZTEST_USER_F(dead_battery_policy, test_dead_battery_policy)
{
	union connector_status_t connector_status;
	uint32_t rdo;

	/* PDC APIs provide unexpected behavior before driver init */
	pdc_driver_init();

	/* Verify each port is configured as dead battery */
	FOREACH_USB_PD_PORT
	{
		verify_dead_battery_config(fixture->emul_pdc[port]);
	}

	/* Allow initialization to occur, verification of dead battery RDO
	 * selection comes from custom_fake_pdc_set_rdo */
	pdc_power_mgmt_wait_for_sync(0, -1);

	/* Verify after initialization both ports are connected as sink but
	 * only one has sink path enabled */
	FOREACH_USB_PD_PORT
	{
		zassert_ok(pdc_power_mgmt_get_connector_status(
			port, &connector_status));

		zassert_equal(connector_status.connect_status, 1, "port=%d",
			      port);
		zassert_equal(connector_status.power_direction, 0, "port=%d",
			      port);
		zassert_equal(connector_status.sink_path_status,
			      (port ? 1 : 0));

		/* Verify dead battery is cleared */
		zassert_false(
			emul_pdc_get_dead_battery(fixture->emul_pdc[port]),
			"port=%d", port);
	}

	/* Verify correct RDO is selected on PORT1 */
	zassert_ok(emul_pdc_get_rdo(fixture->emul_pdc[TEST_USBC_PORT1], &rdo));
	zassert_equal(RDO_POS(rdo), 3);
}
