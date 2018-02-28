/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Coral board-specific configuration */

#include "adc.h"
#include "adc_chip.h"
#include "button.h"
#include "charge_manager.h"
#include "charge_ramp.h"
#include "charge_state.h"
#include "charger.h"
#include "chipset.h"
#include "console.h"
#include "driver/als_opt3001.h"
#include "driver/accel_kionix.h"
#include "driver/accel_kx022.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/charger/bd9995x.h"
#include "driver/tcpm/anx74xx.h"
#include "driver/tcpm/ps8xxx.h"
#include "driver/tcpm/tcpci.h"
#include "driver/tcpm/tcpm.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "keyboard_scan.h"
#include "lid_angle.h"
#include "lid_switch.h"
#include "math_util.h"
#include "motion_sense.h"
#include "motion_lid.h"
#include "power.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "sku.h"
#include "spi.h"
#include "switch.h"
#include "system.h"
#include "tablet_mode.h"
#include "task.h"
#include "temp_sensor.h"
#include "thermistor.h"
#include "timer.h"
#include "uart.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)


/* Called by APL power state machine when transitioning from G3 to S5 */
static void chipset_pre_init(void)
{
	/*
	 * No need to re-init PMIC since settings are sticky across sysjump.
	 * However, be sure to check that PMIC is already enabled. If it is
	 * then there's no need to re-sequence the PMIC.
	 */
	if (system_jumped_to_this_image() && gpio_get_level(GPIO_PMIC_EN))
		return;

	/* Enable PP5000 before PP3300 due to NFC: chrome-os-partner:50807 */
	gpio_set_level(GPIO_EN_PP5000, 1);
	while (!gpio_get_level(GPIO_PP5000_PG))
		;

	/*
	 * To prevent SLP glitches, PMIC_EN (V5A_EN) should be enabled
	 * at the same time as PP3300 (chrome-os-partner:51323).
	 */
	/* Enable 3.3V rail */
	gpio_set_level(GPIO_EN_PP3300, 1);
	while (!gpio_get_level(GPIO_PP3300_PG))
		;

	/* Enable PMIC */
	gpio_set_level(GPIO_PMIC_EN, 1);
}
DECLARE_HOOK(HOOK_CHIPSET_PRE_INIT, chipset_pre_init, HOOK_PRIO_DEFAULT);

/* FIXME(dhendrix): Add CHIPSET_RESUME and CHIPSET_SUSPEND
   hooks to enable/disable sensors? */
/* Called on AP S3 -> S0 transition */
static void board_chipset_resume(void)
{
	gpio_set_level(GPIO_ENABLE_BACKLIGHT, 1);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_chipset_resume, HOOK_PRIO_DEFAULT);

/* Called on AP S0 -> S3 transition */
static void board_chipset_suspend(void)
{
	gpio_set_level(GPIO_ENABLE_BACKLIGHT, 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_chipset_suspend, HOOK_PRIO_DEFAULT);

/*
 * FIXME(dhendrix): Weak symbol hack until we can get a better solution for
 * both Amenia and Coral.
 */
void chipset_do_shutdown(void)
{
	/* Disable PMIC */
	gpio_set_level(GPIO_PMIC_EN, 0);

	/*Disable 3.3V rail */
	gpio_set_level(GPIO_EN_PP3300, 0);
	while (gpio_get_level(GPIO_PP3300_PG))
		;

	/*Disable 5V rail */
	gpio_set_level(GPIO_EN_PP5000, 0);
	while (gpio_get_level(GPIO_PP5000_PG))
		;
}

void board_hibernate_late(void)
{
	int i;
	const uint32_t hibernate_pins[][2] = {
		/* Turn off LEDs in hibernate */
		{GPIO_BAT_LED_BLUE, GPIO_INPUT | GPIO_PULL_UP},
		{GPIO_BAT_LED_AMBER, GPIO_INPUT | GPIO_PULL_UP},
		{GPIO_LID_OPEN, GPIO_INT_RISING | GPIO_PULL_DOWN},

		/*
		 * BD99956 handles charge input automatically. We'll disable
		 * charge output in hibernate. Charger will assert ACOK_OD
		 * when VBUS or VCC are plugged in.
		 */
		{GPIO_USB_C0_5V_EN,       GPIO_INPUT  | GPIO_PULL_DOWN},
		{GPIO_USB_C1_5V_EN,       GPIO_INPUT  | GPIO_PULL_DOWN},
	};

	/* Change GPIOs' state in hibernate for better power consumption */
	for (i = 0; i < ARRAY_SIZE(hibernate_pins); ++i)
		gpio_set_flags(hibernate_pins[i][0], hibernate_pins[i][1]);

	gpio_config_module(MODULE_KEYBOARD_SCAN, 0);

	/*
	 * Calling gpio_config_module sets disabled alternate function pins to
	 * GPIO_INPUT.  But to prevent keypresses causing leakage currents
	 * while hibernating we want to enable GPIO_PULL_UP as well.
	 */
	gpio_set_flags_by_mask(0x2, 0x03, GPIO_INPUT | GPIO_PULL_UP);
	gpio_set_flags_by_mask(0x1, 0x7F, GPIO_INPUT | GPIO_PULL_UP);
	gpio_set_flags_by_mask(0x0, 0xE0, GPIO_INPUT | GPIO_PULL_UP);
	/* KBD_KSO2 needs to have a pull-down enabled instead of pull-up */
	gpio_set_flags_by_mask(0x1, 0x80, GPIO_INPUT | GPIO_PULL_DOWN);
}

void board_hibernate(void)
{
	/*
	 * To support hibernate called from console commands, ectool commands
	 * and key sequence, shutdown the AP before hibernating.
	 */
	chipset_do_shutdown();

	/* Added delay to allow AP to settle down */
	msleep(100);

	/* Enable both the VBUS & VCC ports before entering PG3 */
	bd9995x_select_input_port(BD9995X_CHARGE_PORT_BOTH, 1);

	/* Turn BGATE OFF for saving the power */
	bd9995x_set_power_save_mode(BD9995X_PWR_SAVE_MAX);
}
