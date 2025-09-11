/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "charge_state.h"
#include "charger.h"
#include "charger_test.h"
#include "drivers/ucsi_v3.h"
#include "ec_commands.h"
#include "emul/emul_pdc.h"
#include "usbc/pdc_power_mgmt.h"

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(enum ec_error_list, charger_get_min_required_voltage, int,
		int *);

#define DEFAULT_VOLTAGE 15000
static int test_voltage = DEFAULT_VOLTAGE;

enum ec_error_list fake_get_min_required_voltage(int charger, int *voltage)
{
	*voltage = test_voltage;

	return EC_SUCCESS;
}

static void test_before(void *fixture)
{
	RESET_FAKE(charger_get_min_required_voltage);
	test_voltage = DEFAULT_VOLTAGE;
}

#define RTS5453P_NODE DT_NODELABEL(pdc_emul1)
static const struct emul *emul = EMUL_DT_GET(RTS5453P_NODE);

ZTEST_SUITE(charge_state, charger_predicate_post_main, NULL, test_before, NULL,
	    NULL);

ZTEST(charge_state, test_sufficient_charger)
{
	union connector_status_t connector_status = {};

	charger_get_min_required_voltage_fake.custom_fake =
		fake_get_min_required_voltage;

	charge_manager_leave_safe_mode();

	/* Default best PDO is 20 volts */
	emul_pdc_configure_snk(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	pdc_power_mgmt_wait_for_sync(0, -1);

	zassert_true(charge_is_charger_sufficient());
}

ZTEST(charge_state, test_insufficient_charger)
{
	union connector_status_t connector_status = {};

	test_voltage = 25000;
	charger_get_min_required_voltage_fake.custom_fake =
		fake_get_min_required_voltage;

	charge_manager_leave_safe_mode();

	/* Default best PDO is 20 volts */
	emul_pdc_configure_snk(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	pdc_power_mgmt_wait_for_sync(0, -1);

	zassert_false(charge_is_charger_sufficient());
}
