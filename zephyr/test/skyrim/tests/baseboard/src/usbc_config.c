/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "charge_ramp.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "ioexpander.h"
#include "system.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "usbc_config.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

void reset_nct38xx_port(int port);

FAKE_VOID_FUNC(pd_handle_overcurrent, int);
FAKE_VOID_FUNC(ppc_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(usb_charger_task_set_event, int, uint8_t);
FAKE_VOID_FUNC(battery_sleep_fuel_gauge);
FAKE_VALUE_FUNC(int, charge_manager_get_active_charge_port);
FAKE_VOID_FUNC(pd_request_source_voltage, int, int);
FAKE_VALUE_FUNC(enum ec_error_list, charger_get_vbus_voltage, int, int*);

static void usbc_config_before(void *fixture)
{
	ARG_UNUSED(fixture);
	RESET_FAKE(ppc_interrupt);
	RESET_FAKE(pd_handle_overcurrent);
	RESET_FAKE(usb_charger_task_set_event);
	RESET_FAKE(battery_sleep_fuel_gauge);
	RESET_FAKE(charge_manager_get_active_charge_port);
	RESET_FAKE(pd_request_source_voltage);
	RESET_FAKE(charger_get_vbus_voltage);
}

ZTEST_SUITE(usbc_config, NULL, NULL, usbc_config_before, NULL, NULL);

static int mock_voltage;
static enum ec_error_list charger_get_vbus_voltage_mock(int port, int *voltage)
{
	*voltage = mock_voltage;
	return 0;
}

static int gpio_emul_output_get_dt(const struct gpio_dt_spec *dt)
{
	return gpio_emul_output_get(dt->port, dt->pin);
}

static int gpio_emul_input_set_dt(const struct gpio_dt_spec *dt, int value)
{
	return gpio_emul_input_set(dt->port, dt->pin, value);
}

static int toggle_pin_falling(const struct gpio_dt_spec *dt)
{
	int rv;

	rv = gpio_emul_input_set_dt(dt, 1);
	if (rv)
		return rv;

	rv = gpio_emul_input_set_dt(dt, 0);
	if (rv)
		return rv;

	return 0;
}

static int toggle_pin_rising(const struct gpio_dt_spec *dt)
{
	int rv;

	rv = gpio_emul_input_set_dt(dt, 0);
	if (rv)
		return rv;

	rv = gpio_emul_input_set_dt(dt, 1);
	if (rv)
		return rv;

	return 0;
}

static int set_usb_fault_alert_inputs(int hub, int a0, int a1)
{
	const struct gpio_dt_spec *gpio_usb_hub_fault_q_odl = 
		GPIO_DT_FROM_NODELABEL(gpio_usb_hub_fault_q_odl);
	const struct gpio_dt_spec *ioex_usb_a0_fault_odl =
		GPIO_DT_FROM_NODELABEL(ioex_usb_a0_fault_odl);
	const struct gpio_dt_spec *ioex_usb_a1_fault_db_odl =
		GPIO_DT_FROM_NODELABEL(ioex_usb_a1_fault_db_odl);
	int rv;

	rv = gpio_emul_input_set_dt(gpio_usb_hub_fault_q_odl, hub);
	if (rv)
		return rv;

	rv = gpio_emul_input_set_dt(ioex_usb_a0_fault_odl, a0);
	if (rv)
		return rv;

	rv = gpio_emul_input_set_dt(ioex_usb_a1_fault_db_odl, a1);
	if (rv)
		return rv;

	return 0;
}

static int validate_usb_fault_alert_output(int hub, int a0, int a1)
{
	const struct gpio_dt_spec *gpio_usb_fault_odl =
		GPIO_DT_FROM_NODELABEL(gpio_usb_fault_odl);
	int rv = gpio_emul_output_get_dt(gpio_usb_fault_odl);
	return !(rv == (hub && a0 && a1));
}

ZTEST(usbc_config, test_usbc_interrupt_init)
{
	const struct gpio_dt_spec *gpio_usb_c0_ppc_int_odl =
		GPIO_DT_FROM_NODELABEL(gpio_usb_c0_ppc_int_odl);
	const struct gpio_dt_spec *gpio_usb_c1_ppc_int_odl =
		GPIO_DT_FROM_NODELABEL(gpio_usb_c1_ppc_int_odl);
	const struct gpio_dt_spec *ioex_usb_c0_sbu_fault_odl =
		GPIO_DT_FROM_NODELABEL(ioex_usb_c0_sbu_fault_odl);
	const struct gpio_dt_spec *ioex_usb_c1_sbu_fault_odl =
		GPIO_DT_FROM_NODELABEL(ioex_usb_c1_sbu_fault_odl);
	const struct gpio_dt_spec *gpio_usb_c0_bc12_int_odl =
		GPIO_DT_FROM_NODELABEL(gpio_usb_c0_bc12_int_odl);
	const struct gpio_dt_spec *gpio_usb_c1_bc12_int_odl =
		GPIO_DT_FROM_NODELABEL(gpio_usb_c1_bc12_int_odl);

	/* Ensure interrupts are disabled. */
	gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c0_bc12));
	gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c1_bc12));
	gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c0_bc12));
	gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c1_bc12));
	gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c0_sbu_fault));
	gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c1_sbu_fault));

	/* usbc_interrupt_init should be called on init. */
	hook_notify(HOOK_INIT);

	/* Verify that interrupts are enabled and the handler. */
	zassert_ok(toggle_pin_falling(gpio_usb_c0_ppc_int_odl));
	zassert_equal(ppc_interrupt_fake.call_count, 1);
	RESET_FAKE(ppc_interrupt);

	zassert_ok(toggle_pin_falling(gpio_usb_c1_ppc_int_odl));
	zassert_equal(ppc_interrupt_fake.call_count, 1);
	RESET_FAKE(ppc_interrupt);

	zassert_ok(toggle_pin_rising(gpio_usb_c0_ppc_int_odl));
	zassert_equal(ppc_interrupt_fake.call_count, 0);
	RESET_FAKE(ppc_interrupt);

	zassert_ok(toggle_pin_rising(gpio_usb_c1_ppc_int_odl));
	zassert_equal(ppc_interrupt_fake.call_count, 0);
	RESET_FAKE(ppc_interrupt);

	/* Verify bc12 interrupt handler is called. */
	zassert_ok(toggle_pin_falling(gpio_usb_c0_bc12_int_odl));
	zassert_equal(usb_charger_task_set_event_fake.call_count, 1);
	zassert_equal(usb_charger_task_set_event_fake.arg0_val, 0);
	zassert_equal(usb_charger_task_set_event_fake.arg1_val, USB_CHG_EVENT_BC12);
	RESET_FAKE(usb_charger_task_set_event);

	zassert_ok(toggle_pin_falling(gpio_usb_c1_bc12_int_odl));
	zassert_equal(usb_charger_task_set_event_fake.call_count, 1);
	zassert_equal(usb_charger_task_set_event_fake.arg0_val, 1);
	zassert_equal(usb_charger_task_set_event_fake.arg1_val, USB_CHG_EVENT_BC12);
	RESET_FAKE(usb_charger_task_set_event);

	zassert_ok(toggle_pin_rising(gpio_usb_c0_bc12_int_odl));
	zassert_equal(usb_charger_task_set_event_fake.call_count, 0);
	RESET_FAKE(usb_charger_task_set_event);

	zassert_ok(toggle_pin_rising(gpio_usb_c1_bc12_int_odl));
	zassert_equal(usb_charger_task_set_event_fake.call_count, 0);
	RESET_FAKE(usb_charger_task_set_event);

	/* Verify that the fault handler calls pd_handle_overcurrent with the right port. */
	zassert_ok(toggle_pin_falling(ioex_usb_c0_sbu_fault_odl));
	zassert_equal(pd_handle_overcurrent_fake.call_count, 1);
	zassert_equal(pd_handle_overcurrent_fake.arg0_val, 0);
	RESET_FAKE(pd_handle_overcurrent);

	zassert_ok(toggle_pin_falling(ioex_usb_c1_sbu_fault_odl));
	zassert_equal(pd_handle_overcurrent_fake.call_count, 1);
	zassert_equal(pd_handle_overcurrent_fake.arg0_val, 1);
	RESET_FAKE(pd_handle_overcurrent);
}

ZTEST(usbc_config, test_usb_fault_interrupt_init)
{
	const struct gpio_dt_spec *gpio_usb_hub_fault_q_odl =
		GPIO_DT_FROM_NODELABEL(gpio_usb_hub_fault_q_odl);
	const struct gpio_dt_spec *ioex_usb_a0_fault_odl =
		GPIO_DT_FROM_NODELABEL(ioex_usb_a0_fault_odl);
	const struct gpio_dt_spec *ioex_usb_a1_fault_db_odl =
		GPIO_DT_FROM_NODELABEL(ioex_usb_a1_fault_db_odl);

	/* Make sure interrupts are disabled. */
	gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_hub_fault));
	gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_a0_fault));
	gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_a1_fault));

	/* usb_fault_interrupt_init should be called on chipset startup. */
	hook_notify(HOOK_CHIPSET_STARTUP);

	/* Validate that int_usb_hub_fault calls usb_fault_alert. */
	zassert_ok(set_usb_fault_alert_inputs(0, 0, 0));
	zassert_ok(gpio_emul_input_set_dt(gpio_usb_hub_fault_q_odl, 1));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(1, 0, 0));
	zassert_ok(gpio_emul_input_set_dt(gpio_usb_hub_fault_q_odl, 0));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(0, 0, 0));

	zassert_ok(set_usb_fault_alert_inputs(0, 0, 1));
	zassert_ok(gpio_emul_input_set_dt(gpio_usb_hub_fault_q_odl, 1));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(1, 0, 1));
	zassert_ok(gpio_emul_input_set_dt(gpio_usb_hub_fault_q_odl, 0));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(0, 0, 1));

	zassert_ok(set_usb_fault_alert_inputs(0, 1, 0));
	zassert_ok(gpio_emul_input_set_dt(gpio_usb_hub_fault_q_odl, 1));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(1, 1, 0));
	zassert_ok(gpio_emul_input_set_dt(gpio_usb_hub_fault_q_odl, 0));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(0, 1, 0));

	zassert_ok(set_usb_fault_alert_inputs(0, 1, 1));
	zassert_ok(gpio_emul_input_set_dt(gpio_usb_hub_fault_q_odl, 1));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(1, 1, 1));
	zassert_ok(gpio_emul_input_set_dt(gpio_usb_hub_fault_q_odl, 0));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(0, 1, 1));

	/* Validate that int_usb_hub_fault calls usb_fault_alert. */
	zassert_ok(set_usb_fault_alert_inputs(0, 0, 0));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a0_fault_odl, 1));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(0, 1, 0));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a0_fault_odl, 0));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(0, 0, 0));

	zassert_ok(set_usb_fault_alert_inputs(0, 0, 1));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a0_fault_odl, 1));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(0, 1, 1));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a0_fault_odl, 0));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(0, 0, 1));

	zassert_ok(set_usb_fault_alert_inputs(1, 0, 0));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a0_fault_odl, 1));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(1, 1, 0));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a0_fault_odl, 0));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(1, 0, 0));

	zassert_ok(set_usb_fault_alert_inputs(1, 0, 1));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a0_fault_odl, 1));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(1, 1, 1));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a0_fault_odl, 0));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(1, 0, 1));

	/* Validate that int_usb_hub_fault calls usb_fault_alert. */
	zassert_ok(set_usb_fault_alert_inputs(0, 0, 0));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a1_fault_db_odl, 1));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(0, 0, 1));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a1_fault_db_odl, 0));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(0, 0, 0));

	zassert_ok(set_usb_fault_alert_inputs(0, 1, 0));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a1_fault_db_odl, 1));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(0, 1, 0));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a1_fault_db_odl, 0));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(0, 1, 0));

	zassert_ok(set_usb_fault_alert_inputs(1, 0, 0));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a1_fault_db_odl, 1));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(1, 0, 1));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a1_fault_db_odl, 0));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(1, 0, 0));

	zassert_ok(set_usb_fault_alert_inputs(1, 1, 0));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a1_fault_db_odl, 1));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(1, 1, 1));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a1_fault_db_odl, 0));
	k_msleep(100);
	zassert_ok(validate_usb_fault_alert_output(1, 1, 0));

}

ZTEST(usbc_config, test_usb_fault_interrupt_disable)
{
	const struct gpio_dt_spec *gpio_usb_fault_odl =
		GPIO_DT_FROM_NODELABEL(gpio_usb_fault_odl);
	const struct gpio_dt_spec *gpio_usb_hub_fault_q_odl =
		GPIO_DT_FROM_NODELABEL(gpio_usb_hub_fault_q_odl);
	const struct gpio_dt_spec *ioex_usb_a0_fault_odl =
		GPIO_DT_FROM_NODELABEL(ioex_usb_a0_fault_odl);
	const struct gpio_dt_spec *ioex_usb_a1_fault_db_odl =
		GPIO_DT_FROM_NODELABEL(ioex_usb_a1_fault_db_odl);

	/* Make sure interrupts are enabled. */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_hub_fault));
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_a0_fault));
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_a1_fault));

	/* usb_fault_interrupt_disable should be called on chipset startup. */
	hook_notify(HOOK_CHIPSET_SHUTDOWN);

	zassert_ok(set_usb_fault_alert_inputs(0, 1, 1));
	zassert_ok(gpio_emul_input_set_dt(gpio_usb_hub_fault_q_odl, 1));
	k_msleep(100);
	zassert_false(gpio_emul_output_get_dt(gpio_usb_fault_odl));

	zassert_ok(set_usb_fault_alert_inputs(1, 0, 1));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a0_fault_odl, 1));
	k_msleep(100);
	zassert_false(gpio_emul_output_get_dt(gpio_usb_fault_odl));

	zassert_ok(set_usb_fault_alert_inputs(1, 1, 0));
	zassert_ok(gpio_emul_input_set_dt(ioex_usb_a1_fault_db_odl, 1));
	k_msleep(100);
	zassert_false(gpio_emul_output_get_dt(gpio_usb_fault_odl));
}


ZTEST(usbc_config, test_board_is_vbus_too_low)
{
	int voltage;

	charger_get_vbus_voltage_fake.return_val = 1;
	zassert_false(board_is_vbus_too_low(0, &voltage));
	zassert_equal(charger_get_vbus_voltage_fake.arg0_val, 0);
	zassert_false(board_is_vbus_too_low(1, &voltage));
	zassert_equal(charger_get_vbus_voltage_fake.arg0_val, 1);

	charger_get_vbus_voltage_fake.custom_fake = charger_get_vbus_voltage_mock;
	mock_voltage = 0;
	zassert_false(board_is_vbus_too_low(0, &voltage));
	zassert_equal(charger_get_vbus_voltage_fake.arg0_val, 0);
	zassert_false(board_is_vbus_too_low(1, &voltage));
	zassert_equal(charger_get_vbus_voltage_fake.arg0_val, 1);

	mock_voltage = SKYRIM_BC12_MIN_VOLTAGE / 2;
	zassert_true(board_is_vbus_too_low(0, &voltage));
	zassert_equal(charger_get_vbus_voltage_fake.arg0_val, 0);
	zassert_true(board_is_vbus_too_low(1, &voltage));
	zassert_equal(charger_get_vbus_voltage_fake.arg0_val, 1);

	mock_voltage = SKYRIM_BC12_MIN_VOLTAGE;
	zassert_false(board_is_vbus_too_low(0, &voltage));
	zassert_equal(charger_get_vbus_voltage_fake.arg0_val, 0);
	zassert_false(board_is_vbus_too_low(1, &voltage));
	zassert_equal(charger_get_vbus_voltage_fake.arg0_val, 1);
}

ZTEST(usbc_config, test_board_hibernate)
{
	charge_manager_get_active_charge_port_fake.return_val = CHARGE_PORT_NONE;
	board_hibernate();
	zassert_equal(battery_sleep_fuel_gauge_fake.call_count, 1);
	RESET_FAKE(battery_sleep_fuel_gauge);
	RESET_FAKE(pd_request_source_voltage);

	charge_manager_get_active_charge_port_fake.return_val = 0;
	board_hibernate();
	zassert_equal(battery_sleep_fuel_gauge_fake.call_count, 1);
	zassert_equal(pd_request_source_voltage_fake.arg0_val, 0);
	zassert_equal(pd_request_source_voltage_fake.arg1_val, SKYRIM_SAFE_RESET_VBUS_MV);
	RESET_FAKE(battery_sleep_fuel_gauge);
	RESET_FAKE(pd_request_source_voltage);

	charge_manager_get_active_charge_port_fake.return_val = 1;
	board_hibernate();
	zassert_equal(battery_sleep_fuel_gauge_fake.call_count, 1);
	zassert_equal(pd_request_source_voltage_fake.arg0_val, 1);
	zassert_equal(pd_request_source_voltage_fake.arg1_val, SKYRIM_SAFE_RESET_VBUS_MV);
	RESET_FAKE(battery_sleep_fuel_gauge);
	RESET_FAKE(pd_request_source_voltage);
}

ZTEST(usbc_config, test_reset_nct38xx_port)
{
	reset_nct38xx_port(0);
}