/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_power/ap_power_events.h"
#include "charge_manager.h"
#include "driver/tcpm/nct38xx.h"
#include "emul/tcpc/emul_tcpci.h"
#include "extpower.h"
#include "nissa_hdmi.h"
#include "system.h"
#include "tcpm/tcpci.h"
#include "typec_control.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include <dt-bindings/gpio_defines.h>
#include <typec_control.h>

#define TCPC0 EMUL_DT_GET(DT_NODELABEL(tcpci_emul0))
#define TCPC1 EMUL_DT_GET(DT_NODELABEL(tcpci_emul1))

LOG_MODULE_REGISTER(nissa, LOG_LEVEL_INF);

FAKE_VOID_FUNC(nissa_configure_hdmi_rails);
FAKE_VOID_FUNC(nissa_configure_hdmi_vcc);
FAKE_VALUE_FUNC(int, cbi_get_board_version, uint32_t *);
FAKE_VALUE_FUNC(int, bq25710_is_acok, int);
FAKE_VOID_FUNC(extpower_handle_update, int);
FAKE_VALUE_FUNC(int, charge_manager_get_active_charge_port);
FAKE_VALUE_FUNC(enum ec_error_list, charger_discharge_on_ac, int);
FAKE_VALUE_FUNC(int, chipset_in_state, int);

static int bq25710_is_acok_absent(int charger)
{
	return 0;
}

static int bq25710_is_acok_present(int charger)
{
	return 1;
}

static int bq25710_is_acok_error(int charger)
{
	return 0;
}

static void test_before(void *fixture)
{
	RESET_FAKE(nissa_configure_hdmi_rails);
	RESET_FAKE(nissa_configure_hdmi_vcc);
	RESET_FAKE(cbi_get_board_version);
	RESET_FAKE(bq25710_is_acok);
	RESET_FAKE(extpower_handle_update);
	RESET_FAKE(charge_manager_get_active_charge_port);
	RESET_FAKE(charger_discharge_on_ac);
	RESET_FAKE(chipset_in_state);

	bq25710_is_acok_fake.custom_fake = bq25710_is_acok_absent;

	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC0),
		I2C_COMMON_EMUL_NO_FAIL_REG);
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC1),
		I2C_COMMON_EMUL_NO_FAIL_REG);
}

ZTEST_SUITE(nokris, NULL, NULL, test_before, NULL, NULL);

static int cbi_get_board_version_1(uint32_t *version)
{
	*version = 1;
	return 0;
}

static int cbi_get_board_version_2(uint32_t *version)
{
	*version = 2;
	return 0;
}

ZTEST(nokris, test_hdmi_power)
{
	/* Board version less than 2 configures both */
	cbi_get_board_version_fake.custom_fake = cbi_get_board_version_1;
	nissa_configure_hdmi_power_gpios();
	zassert_equal(nissa_configure_hdmi_vcc_fake.call_count, 1);
	zassert_equal(nissa_configure_hdmi_rails_fake.call_count, 1);

	/* Later versions only enable core rails */
	cbi_get_board_version_fake.custom_fake = cbi_get_board_version_2;
	nissa_configure_hdmi_power_gpios();
	zassert_equal(nissa_configure_hdmi_vcc_fake.call_count, 1);
	zassert_equal(nissa_configure_hdmi_rails_fake.call_count, 2);
}

ZTEST(nokris, test_check_extpower)
{
	/* Ensure initial state is no expower present */
	board_check_extpower();
	RESET_FAKE(extpower_handle_update);

	/* Update with no change does nothing. */
	board_check_extpower();
	zassert_equal(extpower_handle_update_fake.call_count, 0);

	/* Becoming present updates */
	bq25710_is_acok_fake.custom_fake = bq25710_is_acok_present;
	board_check_extpower();
	zassert_equal(extpower_handle_update_fake.call_count, 1);
	zassert_equal(extpower_handle_update_fake.arg0_val, 1);

	/* Errors are treated as not plugged in */
	bq25710_is_acok_fake.custom_fake = bq25710_is_acok_error;
	board_check_extpower();
	zassert_equal(extpower_handle_update_fake.call_count, 2);
	zassert_equal(extpower_handle_update_fake.arg0_val, 0);
}

ZTEST(nokris, test_is_sourcing_vbus)
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

ZTEST(nokris, test_set_active_charge_port_none)
{
	uint16_t reg;

	/* Setting CHARGE_PORT_NONE disables sinking on all ports */
	zassert_ok(board_set_active_charge_port(CHARGE_PORT_NONE));
	tcpci_emul_get_reg(TCPC0, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SNK_CTRL_LOW);
	tcpci_emul_get_reg(TCPC1, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SNK_CTRL_LOW);
}

ZTEST(nokris, test_set_active_charge_port_invalid_port)
{
	zassert_equal(board_set_active_charge_port(3), EC_ERROR_INVAL,
		      "port 3 doesn't exist, should return error");
}

ZTEST(nokris, test_set_active_charge_port_currently_sourcing)
{
	/* Attempting to sink on a port that's sourcing is an error */
	tcpci_emul_set_reg(TCPC1, TCPC_REG_POWER_STATUS,
			   TCPC_REG_POWER_STATUS_SOURCING_VBUS);
	zassert_equal(board_set_active_charge_port(1), EC_ERROR_INVAL);
}

ZTEST(nokris, test_set_active_charge_port)
{
	uint16_t reg;

	/* We can successfully start sinking on a port */
	zassert_ok(board_set_active_charge_port(0));
	zassert_equal(charger_discharge_on_ac_fake.call_count, 2);

	/* Requested charging stop initially */
	zassert_equal(charger_discharge_on_ac_fake.arg0_history[0], 1);
	/* Sinking on the other port was disabled */
	tcpci_emul_get_reg(TCPC1, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SNK_CTRL_LOW);
	/* Sinking was enabled on the new port */
	tcpci_emul_get_reg(TCPC0, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SNK_CTRL_HIGH);
	/* Resumed charging */
	zassert_equal(charger_discharge_on_ac_fake.arg0_history[1], 0);
}

ZTEST(nokris, test_set_active_charge_port_enable_fail)
{
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC0),
		TCPC_REG_COMMAND);
	zassert_equal(board_set_active_charge_port(0), EC_ERROR_UNKNOWN);

	/* Charging was enabled again after the error */
	zassert_equal(charger_discharge_on_ac_fake.arg0_val, 0);
}

ZTEST(nokris, test_set_active_charge_port_disable_fail)
{
	/* Failing to disable sinking on the other port isn't fatal */
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC1),
		TCPC_REG_COMMAND);
	zassert_ok(board_set_active_charge_port(0));
}

ZTEST(nokris, test_pd_power_supply_reset)
{
	uint16_t reg;

	/* Stops any active sourcing on the given port */
	pd_power_supply_reset(0);
	tcpci_emul_get_reg(TCPC0, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SRC_CTRL_LOW);
}

static int chipset_in_state_break_tcpc_command(int state_mask)
{
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC0),
		TCPC_REG_COMMAND);
	return 0;
}

ZTEST(nokris, test_pd_set_power_supply_ready)
{
	uint16_t reg;

	/* Initially sinking VBUS so we can see that gets disabled */
	tcpci_emul_set_reg(TCPC0, TCPC_REG_POWER_STATUS,
			   TCPC_REG_POWER_STATUS_SINKING_VBUS);

	zassert_ok(pd_set_power_supply_ready(0));
	tcpci_emul_get_reg(TCPC0, TCPC_REG_POWER_STATUS, &reg);
	zassert_equal(reg, TCPC_REG_POWER_STATUS_SOURCING_VBUS);

	/* Write to enable VBUS fails */
	chipset_in_state_fake.custom_fake = chipset_in_state_break_tcpc_command;
	zassert_not_equal(pd_set_power_supply_ready(0), EC_SUCCESS);
	chipset_in_state_fake.custom_fake = NULL;

	/* Write to disable sinking fails */
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC0),
		TCPC_REG_COMMAND);
	zassert_not_equal(pd_set_power_supply_ready(0), EC_SUCCESS);
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC0),
		I2C_COMMON_EMUL_NO_FAIL_REG);

	/* AP is off */
	chipset_in_state_fake.return_val = 1;
	zassert_equal(pd_set_power_supply_ready(0), EC_ERROR_NOT_POWERED);

	/* Invalid port number requested */
	zassert_equal(pd_set_power_supply_ready(2), EC_ERROR_INVAL);
}

ZTEST(nokris, test_reset_pd_mcu)
{
	/* Doesn't do anything */
	board_reset_pd_mcu();
}
