/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "button.h"
#include "cbi_ec_fw_config.h"
#include "charge_ramp.h"
#include "charger.h"
#include "common.h"
#include "console.h"
#include "driver/ppc/nx20p348x.h"
#include "driver/ppc/syv682x_public.h"
#include "extpower.h"
#include "power_button.h"
#include "power.h"
#include "registers.h"
#include "switch.h"
#include "throttle_ap.h"
#include "usbc_config.h"

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

/*
 * FW_CONFIG defaults for brya if the CBI.FW_CONFIG data is not
 * initialized.
 */
const union brya_cbi_fw_config fw_config_defaults = {
	.usb_db = DB_USB3_PS8815,
};

__override void board_cbi_init(void)
{
	config_usb_db_type(ec_cfg_usb_db_type());
}

#ifdef CONFIG_CHARGE_RAMP_SW

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
