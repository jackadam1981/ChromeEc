/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nereid sub-board hardware configuration */

#include <init.h>
#include <kernel.h>
#include <sys/printk.h>

#include "hooks.h"
#include "task.h"
#include "throttle_ap.h"

#include "gpios.h"
#include "sub_board.h"

/*
 * Shim for prochot interrupt.
 * The interrupt handler uses the GPIO signal to
 * read and debounce the signal, so pass the signal
 * to the interrupt handler.
 */
void board_interrupt_prochot(void)
{
	throttle_ap_prochot_input_interrupt(GPIO_CPU_PROCHOT);
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
