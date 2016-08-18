/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* rei board configuration */

#include "battery.h"
#include "charge_manager.h"
#include "charger.h"
#include "common.h"
#include "console.h"
#include "crystal.h"
#include "driver/tcpm/tcpci.h"
#include "driver/tcpm/tcpm.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "usb_mux.h"
#include "usb_pd_tcpm.h"
#include "util.h"


/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"pd",      3, 400, GPIO_I2C3_SCL, GPIO_I2C3_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_COUNT] = {
	{I2C_PORT_TCPC, CONFIG_TCPC_I2C_BASE_ADDR, &tcpci_tcpm_drv},
};

/* workaround hack to bring crystal out of reset. */
void crystal_war(void)
{
	/* Set MCI_nReset to output and value to 0. */
	REG32(0xEF02221C) = 0x8;
	REG32(0xEF02220C) = 0x8;

	/* Set STRAP_PUMODE to output and value to 1 */
	REG32(0xEF02221C) = 0x1;
	REG32(0xEF022208) = 0x1;

	/* Set AON_INT to input. */
	REG32(0xEF022220) = 0x4;

	/* Deassert reset. */
	REG32(0xEF022208) = 0x8;
	ccprintf("Crystal WAR\n");
}
DECLARE_HOOK(HOOK_INIT, crystal_war, HOOK_PRIO_DEFAULT);

/*
 * Crystal uses the same GPIO to indicate that its USB controller has detected a
 * wake event as it does for TCPC alerts.  Therefore, we'll need to do some
 * checking to determine the source of this interrupt.
 */
void tcpc_alert_event(enum gpio_signal signal)
{
	if (signal == GPIO_TCPC0_ALERT) {
		crystal_check_usb_wake_evt(0);
		tcpc_alert(0);
	} else if (signal == GPIO_TCPC1_ALERT) {
		crystal_check_usb_wake_evt(1);
		tcpc_alert(1);
	}
}

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_COUNT] = {
	{
		.port_addr = 0,
		.driver = &tcpci_tcpm_usb_mux_driver,
	},
};

#include "gpio_list.h"

void board_init(void)
{
	gpio_enable_interrupt(GPIO_TCPC0_ALERT);
	gpio_enable_interrupt(GPIO_TCPC1_ALERT);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* Stubs. */
enum battery_present battery_is_present(void)
{
	return BP_NO;
}

void battery_get_params(struct batt_params *b)
{
	b->temperature = CELSIUS_TO_DECI_KELVIN(20);
	b->state_of_charge = 60;
	b->voltage = 8000;
}

int battery_is_cut_off(void)
{
	return 1;
}

int battery_serial_number(int *serial)
{
	*serial = 0;
	return EC_SUCCESS;
}

int battery_design_capacity(int *capacity)
{
	*capacity = 0;
	return EC_SUCCESS;
}

int battery_design_voltage(int *v)
{
	*v = 0;
	return EC_SUCCESS;
}

int battery_full_charge_capacity(int *c)
{
	*c = 0;
	return EC_SUCCESS;
}

int battery_cycle_count(int *c)
{
	*c = 1;
	return EC_SUCCESS;
}

int battery_manufacturer_name(char *dest, int size)
{
	strzcpy(dest, "NERV", MIN(size, sizeof("NERV")));
	return EC_SUCCESS;
}

int battery_device_name(char *dest, int size)
{
	strzcpy(dest, "BAT0", MIN(size, sizeof("BAT0")));
	return EC_SUCCESS;
}

int battery_device_chemistry(char *dest, int size)
{
	strzcpy(dest, "LCL", MIN(size, sizeof("LCL")));
	return EC_SUCCESS;
}

int battery_time_to_empty(int *min)
{
	*min = 0;
	return EC_SUCCESS;
}

int battery_time_to_full(int *min)
{
	*min = 0;
	return EC_SUCCESS;
}

void print_battery_debug(void)
{
}

int extpower_is_present(void)
{
	return 1;
}

void extpower_interrupt(enum gpio_signal signal)
{
}

int board_set_active_charge_port(int charge_port)
{
	return EC_SUCCESS;
}

void board_set_charge_limit(int port, int supplier, int charge_ma)
{
}

void pd_send_host_event(int mask)
{
}

int charger_set_current(int current)
{
	return EC_SUCCESS;
}

int charger_set_voltage(int voltage)
{
	return EC_SUCCESS;
}

int charger_set_mode(int mode)
{
	return EC_SUCCESS;
}

int charger_closest_voltage(int voltage)
{
	return EC_SUCCESS;
}

int charger_closest_current(int current)
{
	return EC_SUCCESS;
}

int charger_set_input_current(int input_current)
{
	return EC_SUCCESS;
}

int charger_set_option(int option)
{
	return EC_SUCCESS;
}

static struct charger_info charger_info;
const struct charger_info *charger_get_info(void)
{

	return &charger_info;
}

static struct battery_info b_info = {
	.start_charging_max_c = 50,
	.charging_max_c = 50,
	.discharging_max_c = 100,
	.discharging_min_c = 0,
	.voltage_min = 7800,
};
const struct battery_info *battery_get_info(void)
{
	return &b_info;
}

int charger_post_init(void)
{
	return EC_SUCCESS;
}

void charger_get_params(struct charger_params *chg)
{
}

void print_charger_debug(void)
{
}

void pd_set_input_current_limit(int port, uint32_t max_ma,
				uint32_t supply_voltage)
{
}

void typec_set_input_current_limit(int port, uint32_t max_ma,
				   uint32_t supply_voltage)
{
}

void pd_power_supply_reset(int port)
{
}

int pd_snk_is_vbus_provided(int port)
{
	return 0;
}
