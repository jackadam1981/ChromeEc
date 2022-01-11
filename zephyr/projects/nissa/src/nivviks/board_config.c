/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nivviks sub-board hardware configuration */

#include "gpio.h"
#include "hooks.h"
#include "sub_board.h"

static void nivviks_subboard_init(void)
{
	enum nissa_sub_board_type sb = nissa_get_sb_type();

	if (sb != NISSA_SB_C_A && sb != NISSA_SB_HDMI_A) {
		/* Turn off unused USB A1 GPIOs by making them inputs */
		gpio_set_flags(GPIO_SUB_USB_A1_ILIMIT_SDP, GPIO_INPUT);
		gpio_set_flags(GPIO_EN_SUB_USB_A1_VBUS, GPIO_INPUT);
	}
	if (sb == NISSA_SB_C_A || sb == NISSA_SB_C_LTE)
		/* Enable type-C port 1 */
		gpio_set_flags(GPIO_USB_C1_PD_INT_ODL,
			       GPIO_INT_FALLING|GPIO_PULL_UP);
	if (sb == NISSA_SB_HDMI_A) {
		/* Disable I2C_PORT_USB_C1_TCPC */
		/* TODO(b:212490923): Use pinctrl to switch from I2C */
		/* Enable HDMI GPIOs */
		gpio_set_flags(GPIO_EN_SUB_RAILS_ODL, GPIO_ODR_HIGH);
		gpio_set_flags(GPIO_HDMI_EN_SUB_ODL, GPIO_ODR_HIGH);
		gpio_set_flags(GPIO_HDMI_HPD_SUB_ODL, GPIO_INT_FALLING);
	}
}
/*
 * Make sure setup is done after EEPROM is readable.
 */
DECLARE_HOOK(HOOK_INIT, nivviks_subboard_init, HOOK_PRIO_INIT_I2C + 1);
