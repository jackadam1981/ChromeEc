/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

#include "battery.h"
#include "button.h"
#include "charge_ramp.h"
#include "charger.h"
#include "console.h"
#include "fw_config.h"
#include "hooks.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "power_button.h"
#include "power.h"
#include "pwm.h"
#include "switch.h"
#include "throttle_ap.h"

#include "gpio_list.h" /* Must come after other header files. */

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

#define KBLIGHT_LED_ON_LVL 100
#define KBLIGHT_LED_OFF_LVL 0

static const uint8_t actual_key_mask[KEYBOARD_COLS_MAX] = {
	0x01, 0x68, 0xbd, 0x03, 0x7e, 0xff, 0xff,
	0xff, 0xff, 0x03, 0xfd, 0x48, 0x03, 0xff,
	0xf7, 0x16  /* full set */
};

/******************************************************************************/
/* USB-A charging control */

const int usb_port_enable[USB_PORT_COUNT] = {
	GPIO_EN_PP5000_USBA_R,
};
BUILD_ASSERT(ARRAY_SIZE(usb_port_enable) == USB_PORT_COUNT);

/******************************************************************************/

__override void board_cbi_init(void)
{
	config_usb_db_type();
}

void board_config_pre_init(void)
{
	int i;

	/* override the keyscan key mask */
	for (i = 0; i < KEYBOARD_COLS_MAX; ++i)
		keyscan_config.actual_key_mask[i] = actual_key_mask[i];
}

/* Called on AP S3 -> S0 transition */
static void board_chipset_resume(void)
{
	/* Allow keyboard backlight to be enabled */
	pwm_set_duty(PWM_CH_KBLIGHT, KBLIGHT_LED_ON_LVL);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_chipset_resume, HOOK_PRIO_DEFAULT);

/* Called on AP S0 -> S3 transition */
static void board_chipset_suspend(void)
{
	/* Turn off the keyboard backlight if it's on. */
	pwm_set_duty(PWM_CH_KBLIGHT, KBLIGHT_LED_OFF_LVL);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_chipset_suspend, HOOK_PRIO_DEFAULT);

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

	if (voltage == 0) {
		CPRINTS("%s: must be disconnected", __func__);
		return 1;
	}

	if (voltage < BC12_MIN_VOLTAGE) {
		CPRINTS("%s: port %d: vbus %d lower than %d", __func__,
			port, voltage, BC12_MIN_VOLTAGE);
		return 1;
	}

	return 0;
}

#endif /* CONFIG_CHARGE_RAMP_SW */

enum battery_present battery_hw_present(void)
{
	enum gpio_signal batt_pres;

	batt_pres = GPIO_EC_BATT_PRES_ODL;

	/* The GPIO is low when the battery is physically present */
	return gpio_get_level(batt_pres) ? BP_NO : BP_YES;
}
