/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Guybrush board-specific configuration */

#include "button.h"
#include "common.h"
#include "extpower.h"
#include "fw_config.h"
#include "gpio.h"
#include "hooks.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "switch.h"
#include "tablet_mode.h"

#include "gpio_list.h" /* Must come after other header files. */

static void board_init(void)
{
	/* TODO */
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

bool board_has_kb_bl(void)
{
	return is_fw_config_value(FW_CONFIG_KB_BL_ENABLED);
}

enum usb_a1_retimer board_usb_a1_retimer(void)
{
	if (is_fw_config_value(FW_CONFIG_USB_DB_A1_ANX7491_C1_ANX7451))
		return USB_A1_RETIMER_ANX7491;
	else if (is_fw_config_value(FW_CONFIG_USB_DB_A1_PS8811_C1_PS8818))
		return USB_A1_RETIMER_PS8811;
	return USB_A1_RETIMER_UNKNOWN;
}

enum usb_c1_mux board_usb_c1_mux(void)
{
	if (is_fw_config_value(FW_CONFIG_USB_DB_A1_ANX7491_C1_ANX7451))
		return USB_C1_MUX_ANX7451;
	else if (is_fw_config_value(FW_CONFIG_USB_DB_A1_PS8811_C1_PS8818))
		return USB_C1_MUX_PS8818;
	return USB_C1_MUX_UNKNOWN;
}

enum form_factor board_form_factor(void)
{
	if (is_fw_config_value(FW_CONFIG_FORM_FACTOR_CONVERTIBLE))
		return FORM_FACTOR_CONVERTIBLE;
	if (is_fw_config_value(FW_CONFIG_FORM_FACTOR_CLAMSHELL))
		return FORM_FACTOR_CLAMSHELL;
	return FORM_FACTOR_UNKNOWN;
}
