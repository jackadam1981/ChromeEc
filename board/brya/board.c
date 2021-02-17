/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

#include "cbi_ec_fw_config.h"
#include "charge_ramp.h"
#include "charger.h"
#include "console.h"
#include "power.h"
#include "switch.h"
#include "throttle_ap.h"

#include "gpio_list.h" /* Must come after other header files. */

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

/* Wake up pins */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_ACOK_EC_OD,
	GPIO_EC_RST_ODL,
	GPIO_GSC_EC_PWR_BTN_ODL,
	GPIO_LID_OPEN_OD,
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

/******************************************************************************/
/* USB-A charging control */

const int usb_port_enable[USB_PORT_COUNT] = {
	GPIO_EN_PP5000_USBA_R,
};
BUILD_ASSERT(ARRAY_SIZE(usb_port_enable) == USB_PORT_COUNT);

/******************************************************************************/

/*
 * FW_CONFIG defaults for brya if the CBI.FW_CONFIG data is not
 * initialized.
 */
const union brya_cbi_fw_config fw_config_defaults = {
	.usb_db = DB_USB3_PS8815,
};

__override void board_cbi_init(void)
{
	config_usb_db_type();
}

/*
 * remove when we enable CONFIG_POWER_BUTTON
 */

void power_button_interrupt(enum gpio_signal signal)
{
}

/*
 * remove when we enable CONFIG_VOLUME_BUTTONS
 */

void button_interrupt(enum gpio_signal signal)
{
}

#ifdef CONFIG_CHARGE_RAMP_SW

/*
 * TODO(b/181508008): tune this threshold
 */

#define BC12_MIN_VOLTAGE 4400

/**
 * Return true if VBUS is too low
 */
int board_is_vbus_too_low(int port, enum chg_ramp_vbus_state ramp_state)
{
	int voltage;

	if (charger_get_vbus_voltage(port, &voltage))
		voltage = 0;

	CPRINTS("%s: charger reports VBUS %d on port %d", __func__,
		voltage, port);

	if (voltage == 0) {
		CPRINTS("%s: must be disconnected", __func__);
		return 1;
	}

	if (voltage < BC12_MIN_VOLTAGE) {
		CPRINTS("%s: lower than %d", __func__,
			BC12_MIN_VOLTAGE);
		return 1;
	}

	return 0;

	/*
	 * For legacy BC1.2 charging with CONFIG_CHARGE_RAMP_SW, ramp up input
	 * current until voltage drops to the minimum input voltage of the
	 * charger, 4.096V.
	 */
	/* return voltage < ISL9241_BC12_MIN_VOLTAGE; */
}
#endif /* CONFIG_CHARGE_RAMP_SW */
