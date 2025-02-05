/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file tests the dead battery policies on type-C ports.
 */

#include "chipset.h"
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

#define TEST_USBC_PORT0 USBC_PORT_FROM_PDC_DRIVER_NODE(PDC_NODE_PORT0)
#define TEST_USBC_PORT1 USBC_PORT_FROM_PDC_DRIVER_NODE(PDC_NODE_PORT1)

#define IS_ONE_BIT_SET IS_POWER_OF_TWO

static void clear_partner_pdos(const struct emul *e, enum pdo_type_t type)
{
	uint32_t clear_pdos[PDO_MAX_OBJECTS] = { 0 };

	emul_pdc_set_pdos(e, type, PDO_OFFSET_0, ARRAY_SIZE(clear_pdos),
			  PARTNER_PDO, clear_pdos);
}

struct pdc_fixture {
	const struct device *dev;
	const struct emul *emul_pdc;
	uint32_t pdos[PDO_MAX_OBJECTS];
	uint8_t port;
};

struct dead_battery_policy_fixture {
	struct pdc_fixture pdc[CONFIG_USB_PD_PORT_MAX_COUNT];
};

static enum chipset_state_mask fake_chipset_state = CHIPSET_STATE_ON;

static int custom_fake_chipset_in_state(int mask)
{
	return !!(fake_chipset_state & mask);
}

static uint8_t sink_path_en_mask;

/* PORT1 will have best PDO at RDO_POS=3 -- Validate this after init */
static struct dead_battery_policy_fixture fixture = {
	.pdc = {
		[0] = {
			.dev = DEVICE_DT_GET(PDC_NODE_PORT0),
			.emul_pdc = EMUL_DT_GET(PDC_NODE_PORT0),
			.port = TEST_USBC_PORT0,
			.pdos = {
				PDO_FIXED(5000, 1500, 0),
				PDO_FIXED(9000, 3000, 0),
				PDO_FIXED(12000, 3000, 0),
			},
		},
		[1] = {
			.dev = DEVICE_DT_GET(PDC_NODE_PORT1),
			.emul_pdc = EMUL_DT_GET(PDC_NODE_PORT1),
			.port = TEST_USBC_PORT1,
			.pdos = {
				PDO_FIXED(5000, 1500, 0),
				PDO_FIXED(9000, 3000, 0),
				PDO_FIXED(20000, 3000, 0),
			},
		},
	},
};

static int pdc_dev_to_port(const struct device *dev)
{
	for (int port = 0; port < CONFIG_USB_PD_PORT_MAX_COUNT; port++) {
		if (dev == fixture.pdc[port].dev)
			return fixture.pdc[port].port;
	}

	zassert_true(0, "Unable to find port");

	return -1;
}

static int custom_fake_pdc_set_sink_path(const struct device *dev, bool en)
{
	int port = pdc_dev_to_port(dev);
	uint8_t before = sink_path_en_mask;

	WRITE_BIT(sink_path_en_mask, port, en);
	LOG_INF("FAKE C%d: pdc_set_sink_path en_mask=0x%X", port,
		sink_path_en_mask);
	if (before != sink_path_en_mask && en) {
		zassert_true(IS_ONE_BIT_SET(sink_path_en_mask));
	}

	return pdc_set_sink_path(dev, en);
}

static int custom_fake_pdc_set_rdo(const struct device *dev, uint32_t rdo)
{
	int port = pdc_dev_to_port(dev);

	LOG_INF("FAKE C%d: pdc_set_rdo en_mask=0x%X", port, sink_path_en_mask);

	/* Assert only one sink path is enabled before changing RDOs */
	zassert_true(IS_ONE_BIT_SET(sink_path_en_mask) ||
		     sink_path_en_mask == 0);

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

	pdc_subsys_start();
}

static int configure_dead_battery(const struct pdc_fixture *pdc)
{
	union connector_status_t cs = { 0 };

	zassert_ok(emul_pdc_set_dead_battery(pdc->emul_pdc, 1));
	emul_pdc_configure_snk(pdc->emul_pdc, &cs);
	clear_partner_pdos(pdc->emul_pdc, SOURCE_PDO);
	zassert_ok(emul_pdc_set_pdos(pdc->emul_pdc, SOURCE_PDO, PDO_OFFSET_0,
				     ARRAY_SIZE(pdc->pdos), PARTNER_PDO,
				     pdc->pdos));

	emul_pdc_set_rdo(pdc->emul_pdc, RDO_FIXED(1, 1500, 1500, 0));
	cs.sink_path_status = 1;
	zassert_ok(emul_pdc_connect_partner(pdc->emul_pdc, &cs));

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
	for (int port = 0; port < CONFIG_USB_PD_PORT_MAX_COUNT; port++) {
		configure_dead_battery(&fixture->pdc[port]);
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
	for (int port = 0; port < CONFIG_USB_PD_PORT_MAX_COUNT; port++) {
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
			emul_pdc_get_dead_battery(fixture->pdc[port].emul_pdc),
			"port=%d", port);
	}

	/* Verify correct RDO is selected on PORT1 */
	zassert_ok(
		emul_pdc_get_rdo(fixture->pdc[TEST_USBC_PORT1].emul_pdc, &rdo));
	zassert_equal(RDO_POS(rdo), 3);
}
