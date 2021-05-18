/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Liara board-specific configuration */

#include "button.h"
#include "driver/led/lm3630a.h"
#include "extpower.h"
#include "hooks.h"
#include "i2c.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "switch.h"

#include "gpio_list.h"

const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_LID_OPEN,
	GPIO_AC_PRESENT,
	GPIO_POWER_BUTTON_L,
	GPIO_EC_RST_ODL,
};
const int hibernate_wake_pins_used =  ARRAY_SIZE(hibernate_wake_pins);

/* I2C port map. */
const struct i2c_port_t i2c_ports[] = {
	{"power",   I2C_PORT_POWER,   100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
	{"tcpc0",   I2C_PORT_TCPC0,   400, GPIO_I2C1_SCL, GPIO_I2C1_SDA},
	{"tcpc1",   I2C_PORT_TCPC1,   400, GPIO_I2C2_SCL, GPIO_I2C2_SDA},
	{"thermal", I2C_PORT_THERMAL_AP, 400, GPIO_I2C3_SCL, GPIO_I2C3_SDA},
	{"kblight", I2C_PORT_KBLIGHT, 100, GPIO_I2C5_SCL, GPIO_I2C5_SDA},
	{"sensor",  I2C_PORT_SENSOR,  400, GPIO_I2C7_SCL, GPIO_I2C7_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
const struct pwm_t pwm_channels[] = {
	[PWM_CH_KBLIGHT] = {
		.channel = 5,
		.flags = PWM_CONFIG_DSLEEP,
		.freq = 100,
	},
	[PWM_CH_LED1_WHITE] = {
		.channel = 0,
		.flags = (PWM_CONFIG_OPEN_DRAIN | PWM_CONFIG_ACTIVE_LOW
			  | PWM_CONFIG_DSLEEP),
		.freq = 100,
	},
	[PWM_CH_LED2_AMBER] =   {
		.channel = 2,
		.flags = (PWM_CONFIG_OPEN_DRAIN | PWM_CONFIG_ACTIVE_LOW
			  | PWM_CONFIG_DSLEEP),
		.freq = 100,
	},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
void board_overcurrent_event(int port, int is_overcurrented)
{
	enum gpio_signal signal = (port == 0) ? GPIO_USB_C0_OC_L
					      : GPIO_USB_C1_OC_L;
	/* Note that the levels are inverted because the pin is active low. */
	int lvl = is_overcurrented ? 0 : 1;

	gpio_set_level(signal, lvl);

	CPRINTS("p%d: overcurrent!", port);
}

void board_tcpc_init(void)
{
	int port;

	/* Only reset TCPC if not sysjump */
	if (!system_jumped_to_this_image())
		board_reset_pd_mcu();

	/* Enable PPC interrupts. */
	gpio_enable_interrupt(GPIO_USB_C0_SWCTL_INT_ODL);
	gpio_enable_interrupt(GPIO_USB_C1_SWCTL_INT_ODL);

	/* Enable TCPC interrupts. */
	gpio_enable_interrupt(GPIO_USB_C0_PD_INT_ODL);
	gpio_enable_interrupt(GPIO_USB_C1_PD_INT_ODL);

#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
	/* Enable CABLE_DET interrupt for ANX3429 wake from standby */
	gpio_enable_interrupt(GPIO_USB_C0_CABLE_DET);
#endif
	/*
	 * Initialize HPD to low; after sysjump SOC needs to see
	 * HPD pulse to enable video path
	 */
	for (port = 0; port < CONFIG_USB_PD_PORT_MAX_COUNT; port++) {
		const struct usb_mux *mux = &usb_muxes[port];

		mux->hpd_update(port, 0, 0);
	}
}
DECLARE_HOOK(HOOK_INIT, board_tcpc_init, HOOK_PRIO_INIT_I2C + 1);

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;

	if (!gpio_get_level(GPIO_USB_C0_PD_INT_ODL)) {
		if (gpio_get_level(GPIO_USB_C0_PD_RST_L))
			status |= PD_STATUS_TCPC_ALERT_0;
	}

	if (!gpio_get_level(GPIO_USB_C1_PD_INT_ODL)) {
		if (gpio_get_level(GPIO_USB_C1_PD_RST_L))
			status |= PD_STATUS_TCPC_ALERT_1;
	}

	return status;
}

/**
 * Power on (or off) a single TCPC.
 * minimum on/off delays are included.
 *
 * @param port	Port number of TCPC.
 * @param mode	0: power off, 1: power on.
 */
void board_set_tcpc_power_mode(int port, int mode)
{
	if (port != USB_PD_PORT_ANX74XX)
		return;

	switch (mode) {
	case ANX74XX_NORMAL_MODE:
		gpio_set_level(GPIO_EN_USB_C0_TCPC_PWR, 1);
		msleep(ANX74XX_PWR_H_RST_H_DELAY_MS);
		gpio_set_level(GPIO_USB_C0_PD_RST_L, 1);
		break;
	case ANX74XX_STANDBY_MODE:
		gpio_set_level(GPIO_USB_C0_PD_RST_L, 0);
		msleep(ANX74XX_RST_L_PWR_L_DELAY_MS);
		gpio_set_level(GPIO_EN_USB_C0_TCPC_PWR, 0);
		msleep(ANX74XX_PWR_L_PWR_H_DELAY_MS);
		break;
	default:
		break;
	}
}

void board_reset_pd_mcu(void)
{
	/* Assert reset to TCPC1 (ps8751) */
	gpio_set_level(GPIO_USB_C1_PD_RST_L, 0);

	/* Assert reset to TCPC0 (anx3429) */
	gpio_set_level(GPIO_USB_C0_PD_RST_L, 0);

	/* TCPC1 (ps8751) requires 1ms reset down assertion */
	msleep(MAX(1, ANX74XX_RST_L_PWR_L_DELAY_MS));

	/* Deassert reset to TCPC1 */
	gpio_set_level(GPIO_USB_C1_PD_RST_L, 1);
	/* Disable TCPC0 power */
	gpio_set_level(GPIO_EN_USB_C0_TCPC_PWR, 0);

	/*
	 * anx3429 requires 10ms reset/power down assertion
	 */
	msleep(ANX74XX_PWR_L_PWR_H_DELAY_MS);
	board_set_tcpc_power_mode(USB_PD_PORT_ANX74XX, 1);
}

=======
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
static void board_kblight_init(void)
{
	/*
	 * Enable keyboard backlight. This needs to be done here because
	 * the chip doesn't have power until PP3300_S0 comes up.
	 */
	gpio_set_level(GPIO_KB_BL_EN, 1);
	lm3630a_poweron();
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_kblight_init, HOOK_PRIO_DEFAULT);
