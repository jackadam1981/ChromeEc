/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charger.h"
#include "chipset.h"
#include "driver/charger/sm5803.h"
#include "extpower.h"
#include "hooks.h"
#include "usb_pd.h"
#include "sub_board.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

const struct charger_config_t chg_chips[] = {
	{
		.i2c_port = I2C_PORT_USB_C0_TCPC,
		.i2c_addr_flags = SM5803_ADDR_CHARGER_FLAGS,
		.drv = &sm5803_drv,
	},
	/* Sub-board */
	{
		.i2c_port = I2C_PORT_USB_C1_TCPC,
		.i2c_addr_flags = SM5803_ADDR_CHARGER_FLAGS,
		.drv = &sm5803_drv,
	},
};

int extpower_is_present(void)
{
	int port;
	int rv;
	bool acok;

	for (port = 0; port < board_get_usb_pd_port_count(); port++) {
		rv = sm5803_is_acok(port, &acok);
		if ((rv == EC_SUCCESS) && acok)
			return 1;
	}

	return 0;
}

/*
 * Nereid does not have a GPIO indicating whether extpower is present,
 * so detect using the charger(s).
 */
__override void board_check_extpower(void)
{
	static int last_extpower_present;
	int extpower_present = extpower_is_present();

	if (last_extpower_present ^ extpower_present)
		extpower_handle_update(extpower_present);

	last_extpower_present = extpower_present;
}

__override void typec_set_source_current_limit(int port, enum tcpc_rp_value rp)
{
	int current;

	if (port < 0 || port > CONFIG_USB_PD_PORT_MAX_COUNT)
		return;

	current = (rp == TYPEC_RP_3A0) ? 3000 : 1500;

	charger_set_otg_current_voltage(port, current, 5000);
}

__override void board_power_5v_enable(int enable)
{
	/*
	 * Motherboard has a GPIO to turn on the 5V regulator, but the sub-board
	 * sets it through the charger GPIO.
	 */
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_pp5000_s5), !!enable);
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_usb_a0_vbus), !!enable);
	if (sm5803_set_gpio0_level(1, !!enable))
		CPRINTS("Failed to %sable sub rails!", enable ? "en" : "dis");
}

void board_sm5803_init(void)
{
	int on;

	/* Charger on the MB will be outputting PROCHOT_ODL and OD CHG_DET */
	sm5803_configure_gpio0(CHARGER_PRIMARY, GPIO0_MODE_PROCHOT, 1);
	sm5803_configure_chg_det_od(CHARGER_PRIMARY, 1);

	/* Charger on the sub-board will be a push-pull GPIO */
	sm5803_configure_gpio0(CHARGER_SECONDARY, GPIO0_MODE_OUTPUT, 0);

	/* Turn on 5V if the system is on, otherwise turn it off */
	on = chipset_in_state(CHIPSET_STATE_ON | CHIPSET_STATE_ANY_SUSPEND |
			      CHIPSET_STATE_SOFT_OFF);
	board_power_5v_enable(on);
}
DECLARE_HOOK(HOOK_INIT, board_sm5803_init, HOOK_PRIO_DEFAULT);
