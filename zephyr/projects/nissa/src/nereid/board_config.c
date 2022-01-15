/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nereid sub-board hardware configuration */

#include <init.h>
#include <kernel.h>
#include <sys/printk.h>

#include "gpio/gpio_int.h"
#include "hooks.h"
#include "task.h"

#include "gpios.h"
#include "sub_board.h"

/*
 * Map the gpio signal to an interrupt configuration block.
 * This is required so that legacy code can use the gpio signal
 * enum name to enable or disable interrupts.
 */
__override struct gpio_int_config *
	board_map_gpio_signal_to_interrupt(enum gpio_signal signal)
{
	switch (signal) {
	default:
		return 0;

	case GPIO_WP_L:
		return GPIO_INTERRUPT(int_wp_l);

	case GPIO_POWER_BUTTON_L:
		return GPIO_INTERRUPT(int_power_button);

	case GPIO_LID_OPEN:
		return GPIO_INTERRUPT(int_lid_open);

	case GPIO_CPU_PROCHOT:
		return GPIO_INTERRUPT(int_prochot);
	}
}

/*
 * Temporary interrupt shims for testing.
 * The Zephyr GPIO interrupt handling does not provide any
 * argument to the interrupt handler, so each handler needs
 * a shim function to provide the appropriate GPIO
 * signal name.
 * These should likely be moved into the legacy code.
 */
void switch_interrupt(enum gpio_signal sig);
void lid_interrupt(enum gpio_signal sig);
void throttle_ap_prochot_input_interrupt(enum gpio_signal sig);
void power_button_interrupt(enum gpio_signal sig);

void shim_interrupt_wp(void)
{
	switch_interrupt(GPIO_WP_L);
}

void shim_interrupt_lid(void)
{
	lid_interrupt(GPIO_LID_OPEN);
}

void shim_interrupt_prochot(void)
{
	throttle_ap_prochot_input_interrupt(GPIO_CPU_PROCHOT);
}

void shim_interrupt_power_button(void)
{
	power_button_interrupt(GPIO_POWER_BUTTON_L);
}

static void nereid_subboard_init(void)
{
	enum nissa_sub_board_type sb = nissa_get_sb_type();

	/*
	 * Need to initialise board specific GPIOs since the
	 * common init code does not know about them.
	 * Remove once common code initialises all GPIOs, not just
	 * the ones with enum-names.
	 */
	if (sb != NISSA_SB_C_A && sb != NISSA_SB_HDMI_A) {
		/* Turn off unused USB A1 GPIOs */
		gpio_pin_configure_dt(&gpio_sub_usb_a1_ilimit_sdp,
				      GPIO_DISCONNECTED);
		gpio_pin_configure_dt(&gpio_en_sub_usb_a1_vbus,
				      GPIO_DISCONNECTED);
	}
	if (sb == NISSA_SB_C_A || sb == NISSA_SB_C_LTE) {
		/* Enable type-C port 1 */
		gpio_pin_configure_dt(&gpio_usb_c1_int_odl,
				      GPIO_INPUT |
				      gpio_usb_c1_int_odl.dt_flags);
	}
	if (sb == NISSA_SB_HDMI_A) {
		/* Disable I2C_PORT_USB_C1_TCPC */
		/* TODO(b:212490923): Use pinctrl to switch from I2C */
		/* Enable HDMI GPIOs */
		gpio_pin_configure_dt(&gpio_en_sub_rails_odl,
				      GPIO_OUTPUT |
				      GPIO_OUTPUT_INIT_HIGH |
				      gpio_en_sub_rails_odl.dt_flags);
		gpio_pin_configure_dt(&gpio_hdmi_en_sub_odl,
				      GPIO_OUTPUT |
				      GPIO_OUTPUT_INIT_HIGH |
				      gpio_hdmi_en_sub_odl.dt_flags);
		/* Configure the interrupt separately */
		gpio_pin_configure_dt(&gpio_hpd_sub_odl,
				      GPIO_INPUT | gpio_hpd_sub_odl.dt_flags);
	}
}
DECLARE_HOOK(HOOK_INIT, nereid_subboard_init, HOOK_PRIO_FIRST+1);
