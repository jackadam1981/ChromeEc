/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "battery_fuel_gauge.h"
// #include "board_config.h"
// #include "button.h"
#include "charge_manager.h"
#include "chipset.h"
#include "common.h"
#include "uldren.h"
// #include "cros_board_info.h"
// #include "cros_cbi.h"
#include "emul/tcpc/emul_tcpci.h"
#include "extpower.h"
// #include "fan.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
// #include "keyboard_8042_sharedlib.h"
// #include "keyboard_raw.h"
// #include "keyboard_scan.h"
// #include "led_onoff_states.h"
// #include "led_pwm.h"
// #include "motionsense_sensors.h"
// #include "nissa_sub_board.h"
// #include "tablet_mode.h"
#include "system.h"
#include "tcpm/tcpci.h"
// #include "thermal.h"


#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include <dt-bindings/gpio_defines.h>
#include <typec_control.h>

#define TCPC0 EMUL_DT_GET(DT_NODELABEL(tcpc_port0))
#define TCPC1 EMUL_DT_GET(DT_NODELABEL(tcpc_port1))

LOG_MODULE_REGISTER(nissa, LOG_LEVEL_INF);

FAKE_VALUE_FUNC(int, chipset_in_state, int);

FAKE_VALUE_FUNC(enum ec_error_list, raa489000_is_acok, int, bool *);
FAKE_VALUE_FUNC(enum battery_present, battery_is_present);
// FAKE_VOID_FUNC(set_pwm_led_color, enum pwm_led_id, int);
FAKE_VOID_FUNC(raa489000_hibernate, int, bool);

FAKE_VALUE_FUNC(int, raa489000_enable_asgate, int, bool);
FAKE_VALUE_FUNC(int, raa489000_set_output_current, int, enum tcpc_rp_value);
FAKE_VALUE_FUNC(enum ec_error_list, charger_discharge_on_ac, int);

static enum ec_error_list raa489000_is_acok_absent(int charger, bool *acok)
{
	*acok = false;
	return EC_SUCCESS;
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
static enum ec_error_list raa489000_is_acok_absent(int charger, bool *acok);

// static enum ec_error_list raa489000_is_acok_absent(int charger, bool *acok);
static void test_before(void *fixture)
{
	RESET_FAKE(raa489000_hibernate);
	RESET_FAKE(raa489000_is_acok);
	RESET_FAKE(raa489000_enable_asgate);
	RESET_FAKE(charger_discharge_on_ac);

	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_absent;

	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC0),
		I2C_COMMON_EMUL_NO_FAIL_REG);
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC1),
		I2C_COMMON_EMUL_NO_FAIL_REG);
}
ZTEST_SUITE(uldren, NULL, NULL, test_before, NULL, NULL);


ZTEST(uldren, test_extpower_is_present)
{
	/* Errors are not-OK */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_error;
	zassert_false(extpower_is_present());
	zassert_equal(raa489000_is_acok_fake.call_count, 2);

	/* When neither charger is connected, we check both and return no. */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_absent;
	zassert_false(extpower_is_present());
	zassert_equal(raa489000_is_acok_fake.call_count, 4);

	/* If one is connected, AC is present */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_present;
	zassert_true(extpower_is_present());
	zassert_equal(raa489000_is_acok_fake.call_count, 5);
}

static int extpower_handle_update_call_count;

static void call_extpower_handle_update(void)
{
	extpower_handle_update_call_count++;
}
DECLARE_HOOK(HOOK_AC_CHANGE, call_extpower_handle_update, HOOK_PRIO_DEFAULT);

ZTEST(uldren, test_board_check_extpower)
{
	/* Clear call count before testing. */
	extpower_handle_update_call_count = 0;

	/* Update with no change does nothing. */
	board_check_extpower();
	zassert_equal(extpower_handle_update_call_count, 0);

	/* Becoming present updates */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_present;
	board_check_extpower();
	zassert_equal(extpower_handle_update_call_count, 1);

	/* Errors are treated as not plugged in */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_error;
	board_check_extpower();
	zassert_equal(extpower_handle_update_call_count, 2);
}

ZTEST(uldren, test_charger_hibernate)
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

ZTEST(uldren, test_board_is_sourcing_vbus)
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

ZTEST(uldren, test_set_active_charge_port_none)
{
	uint16_t reg;

	/* Setting CHARGE_PORT_NONE disables sinking on all ports */
	zassert_ok(board_set_active_charge_port(CHARGE_PORT_NONE));
	zassert_equal(raa489000_enable_asgate_fake.call_count, 2);
	zassert_equal(raa489000_enable_asgate_fake.arg0_history[0], 0);
	zassert_equal(raa489000_enable_asgate_fake.arg1_history[0], false);
	zassert_equal(raa489000_enable_asgate_fake.arg0_history[1], 1);
	zassert_equal(raa489000_enable_asgate_fake.arg1_history[1], false);
	tcpci_emul_get_reg(TCPC0, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SNK_CTRL_LOW);
	tcpci_emul_get_reg(TCPC1, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SNK_CTRL_LOW);
}

ZTEST(uldren, test_set_active_charge_port_invalid_port)
{
	zassert_equal(board_set_active_charge_port(4), EC_ERROR_INVAL,
		      "port 4 doesn't exist, should return error");
}

ZTEST(uldren, test_set_active_charge_port_currently_sourcing)
{
	/* Attempting to sink on a port that's sourcing is an error */
	tcpci_emul_set_reg(TCPC1, TCPC_REG_POWER_STATUS,
			   TCPC_REG_POWER_STATUS_SOURCING_VBUS);
	zassert_equal(board_set_active_charge_port(1), EC_ERROR_INVAL);
}

ZTEST(uldren, test_set_active_charge_port)
{
	uint16_t reg;

	/* Setting old_port to a port not CHARGE_PORT_NONE. */
	charge_port = 1;
	/* We can successfully start sinking on a port */
	zassert_ok(board_set_active_charge_port(0));
	zassert_equal(raa489000_enable_asgate_fake.call_count, 2);
	zassert_equal(charger_discharge_on_ac_fake.call_count, 2);

	/* Requested charging stop initially */
	zassert_equal(charger_discharge_on_ac_fake.arg0_history[0], 1);
	/* Sinking on the other port was disabled */
	tcpci_emul_get_reg(TCPC1, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SNK_CTRL_LOW);
	zassert_equal(raa489000_enable_asgate_fake.arg0_history[0], 1);
	zassert_equal(raa489000_enable_asgate_fake.arg1_history[0], false);
	/* Sinking was enabled on the new port */
	tcpci_emul_get_reg(TCPC0, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SNK_CTRL_HIGH);
	zassert_equal(raa489000_enable_asgate_fake.arg0_history[1], 0);
	zassert_equal(raa489000_enable_asgate_fake.arg1_history[1], true);
	/* Resumed charging */
	zassert_equal(charger_discharge_on_ac_fake.arg0_history[1], 0);
}

ZTEST(uldren, test_set_active_charge_port_enable_fail)
{
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC0),
		TCPC_REG_COMMAND);
	zassert_equal(board_set_active_charge_port(0), EC_ERROR_UNKNOWN);
}
