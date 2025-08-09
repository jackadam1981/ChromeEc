/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file tests the dead battery policies on type-C ports.
 */

#include "battery.h"
#include "chipset.h"
#include "dead_battery_policy.h"
#include "emul/emul_pdc.h"
#include "test/util.h"
#include "timer.h"
#include "zephyr/sys/util.h"
#include "zephyr/sys/util_macro.h"

#include <stdbool.h>

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

void clear_partner_pdos(const struct emul *e, enum pdo_type_t type)
{
	uint32_t clear_pdos[PDO_MAX_OBJECTS] = { 0 };

	emul_pdc_set_pdos(e, type, PDO_OFFSET_0, ARRAY_SIZE(clear_pdos),
			  PARTNER_PDO, clear_pdos);
}

static enum chipset_state_mask fake_chipset_state = CHIPSET_STATE_ON;

void set_chipset_state(enum chipset_state_mask state)
{
	fake_chipset_state = state;
}

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

	/* RDO should not be sent when we're sinking from this port with AP on
	 * and battery is not present */
	if (IS_BIT_SET(sink_path_en_mask, port) &&
	    chipset_in_state(CHIPSET_STATE_ON)) {
		zassert_equal(battery_is_present(), BP_YES);
	}

	return pdc_set_rdo(dev, rdo);
}

void pdc_driver_init(void)
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

int configure_dead_battery(const struct pdc_fixture *pdc)
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
void verify_dead_battery_config(const struct emul *e)
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
	/* Drivers cannot be deinitialized, so we can only have one test per
	 * binary to validate the driver initialization flow for dead battery.*/
	zassert_equal(
		ZTEST_TEST_COUNT, 1,
		"Only one test allowed per binary due to validating driver initialization");
	RESET_FAKE(chipset_in_state);
	RESET_FAKE(sniff_pdc_set_sink_path);
	RESET_FAKE(sniff_pdc_set_rdo);

	chipset_in_state_fake.custom_fake = custom_fake_chipset_in_state;
	sniff_pdc_set_sink_path_fake.custom_fake =
		custom_fake_pdc_set_sink_path;
	sniff_pdc_set_rdo_fake.custom_fake = custom_fake_pdc_set_rdo;

	sink_path_en_mask = BIT_MASK(CONFIG_USB_PD_PORT_MAX_COUNT);
}

ZTEST_SUITE(dead_battery_policy, NULL, dead_battery_policy_setup,
	    dead_battery_policy_before, NULL, NULL);
