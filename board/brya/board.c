/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "button.h"
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
#include "task.h"
#include "throttle_ap.h"
#include "usb_charge.h"
#include "usb_pd.h"

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

#ifdef CONFIG_CHARGE_RAMP_SW
/**
 * Return if VBUS is too low
 */
int board_is_vbus_too_low(int port, enum chg_ramp_vbus_state ramp_state)
{
	int voltage;

	if (charger_get_vbus_voltage(port, &voltage))
		voltage = 0;

	CPRINTS("%s: charger reports VBUS %d", __func__, voltage);
	CPRINTS("%s: ... pretend that's good", __func__);
	return 0;

	/*
	 * For legacy BC1.2 charging with CONFIG_CHARGE_RAMP_SW, ramp up input
	 * current until voltage drops to the minimum input voltage of the
	 * charger, 4.096V.
	 */
	/* return voltage < ISL9241_BC12_MIN_VOLTAGE; */
}
#endif /* CONFIG_CHARGE_RAMP_SW */
