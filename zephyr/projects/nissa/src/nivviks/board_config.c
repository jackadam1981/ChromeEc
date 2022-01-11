/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nivviks sub-board hardware configuration */

#include "gpios.h"
#include "hooks.h"
#include "sub_board.h"

static void nivviks_subboard_init(void)
{
	enum nissa_sub_board_type sb = nissa_get_sb_type();

	if (sb != NISSA_SB_C_A && sb != NISSA_SB_HDMI_A) {
		/* Turn off unused USB A1 GPIOs */
		gpio_pin_configure_dt(&gpio_sub_usb_a1_ilimit_sdp,
				      GPIO_DISCONNECTED);
		gpio_pin_configure_dt(&gpio_en_sub_usb_a1_vbus,
				      GPIO_DISCONNECTED);
	}
	if (sb == NISSA_SB_C_A || sb == NISSA_SB_C_LTE)
		/* Enable type-C port 1 */
		gpio_pin_configure_dt(&gpio_usb_c1_int_odl,
				      GPIO_INPUT | GPIO_PULL_UP);
	if (sb == NISSA_SB_HDMI_A) {
		/* Disable I2C_PORT_USB_C1_TCPC */
		/* TODO(b:212490923): Use pinctrl to switch from I2C */
		/* Enable HDMI GPIOs */
		gpio_pin_configure_dt(&gpio_en_sub_rails_odl,
				      GPIO_OUTPUT |
				      GPIO_OPEN_DRAIN |
				      GPIO_OUTPUT_INIT_HIGH);
		gpio_pin_configure_dt(&gpio_hdmi_en_sub_odl,
				      GPIO_OUTPUT |
				      GPIO_OPEN_DRAIN |
				      GPIO_OUTPUT_INIT_HIGH);
		/* Configure the interrupt separately */
		gpio_pin_configure_dt(&gpio_hpd_sub_odl,
				      GPIO_INPUT |
				      GPIO_PULL_UP);
	}
}
/*
 * Make sure setup is done after EEPROM is readable.
 */
DECLARE_HOOK(HOOK_INIT, nivviks_subboard_init, HOOK_PRIO_INIT_I2C + 1);
