/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/charger/isl923x_public.h"
#include "driver/tcpm/raa489000.h"
#include "emul/tcpc/emul_tcpci.h"
#include "extpower.h"
#include "system.h"
#include "tcpm/tcpci.h"
#include "usb_charge.h"

#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>


LOG_MODULE_REGISTER(nissa, LOG_LEVEL_INF);

FAKE_VALUE_FUNC(int, raa489000_enable_asgate, int, bool);
FAKE_VALUE_FUNC(int, raa489000_set_output_current, int, enum tcpc_rp_value);
FAKE_VOID_FUNC(raa489000_hibernate, int, bool);
FAKE_VALUE_FUNC(enum ec_error_list, raa489000_is_acok, int, bool *);
FAKE_VOID_FUNC(extpower_handle_update, int);

static void test_before(void *fixture)
{
	RESET_FAKE(raa489000_enable_asgate);
	RESET_FAKE(raa489000_set_output_current);
	RESET_FAKE(raa489000_hibernate);
	RESET_FAKE(raa489000_is_acok);
	RESET_FAKE(extpower_handle_update);
}

ZTEST_SUITE(nivviks_charger, NULL, NULL, test_before, NULL, NULL);

ZTEST(nivviks_charger, test_charger_hibernate)
{
	/* board_hibernate() asks the chargers to hibernate. */
	board_hibernate();

	zassert_equal(raa489000_hibernate_fake.call_count, 2);
	zassert_equal(raa489000_hibernate_fake.arg0_history[0],
		      CHARGER_SECONDARY);
	zassert_true(raa489000_hibernate_fake.arg1_history[0]);
	zassert_equal(raa489000_hibernate_fake.arg0_history[1],
		      CHARGER_PRIMARY);
	zassert_true(raa489000_hibernate_fake.arg1_history[1]);
}

static enum ec_error_list raa489000_is_acok_present(int charger, bool *acok)
{
	*acok = true;
	return EC_SUCCESS;
}

static enum ec_error_list raa489000_is_acok_error(int charger, bool *acok)
{
	return EC_ERROR_UNIMPLEMENTED;
}

ZTEST(nivviks_charger, test_check_extpower)
{
	/* Defalt state is absent; update with no change does nothing. */
	board_check_extpower();
	zassert_equal(extpower_handle_update_fake.call_count, 0);

	/* Becoming present updates */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_present;
	board_check_extpower();
	zassert_equal(extpower_handle_update_fake.call_count, 1);
	zassert_equal(extpower_handle_update_fake.arg0_val, 1);

	/* Errors are treated as not plugged in */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_error;
	board_check_extpower();
	zassert_equal(extpower_handle_update_fake.call_count, 2);
	zassert_equal(extpower_handle_update_fake.arg0_val, 0);
}

#define TCPC0 EMUL_DT_GET(DT_NODELABEL(tcpci_emul_0))
#define TCPC1 EMUL_DT_GET(DT_NODELABEL(tcpci_emul_1))

ZTEST(nivviks_charger, test_is_sourcing_vbus)
{
	tcpci_emul_set_reg(TCPC0, TCPC_REG_POWER_STATUS,
			   TCPC_REG_POWER_STATUS_SOURCING_VBUS |
				   TCPC_REG_POWER_STATUS_VBUS_PRES);
	zassert_true(board_is_sourcing_vbus(0));

	tcpci_emul_set_reg(TCPC1, TCPC_REG_POWER_STATUS,
			   TCPC_REG_POWER_STATUS_SINKING_VBUS |
				   TCPC_REG_POWER_STATUS_VBUS_PRES);
	zassert_false(board_is_sourcing_vbus(1));
}