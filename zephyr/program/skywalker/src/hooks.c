/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger.h"
#include "driver/charger/bq257x0_regs.h"
#include "fan.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "rauru_sub_board.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "usb_tc_sm.h"

#include <zephyr/drivers/gpio.h>

static void rauru_common_init(void)
{
	gpio_enable_dt_interrupt(
		GPIO_INT_FROM_NODELABEL(int_ap_xhci_init_done));

	/* b/353712228:
	 * Rauru's HW sets external current limit (ILIM_HIZ) to 0.9A.
	 * FW need to disable EXTILIM on boot to allow larger current.
	 */
	i2c_update16(chg_chips[CHARGER_SOLO].i2c_port,
		     chg_chips[CHARGER_SOLO].i2c_addr_flags,
		     BQ25710_REG_CHARGE_OPTION_2,
		     1 << BQ257X0_CHARGE_OPTION_2_EN_EXTILIM_SHIFT, 0);
}
DECLARE_HOOK(HOOK_INIT, rauru_common_init, HOOK_PRIO_PRE_DEFAULT);

/* USB-A */
void xhci_interrupt(enum gpio_signal signal)
{
#ifdef USB_PORT_ENABLE_COUNT
	enum usb_charge_mode mode = gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(
					    gpio_ap_xhci_init_done_r)) ?
					    USB_CHARGE_MODE_ENABLED :
					    USB_CHARGE_MODE_DISABLED;

	for (int i = 0; i < USB_PORT_ENABLE_COUNT; i++) {
		usb_charge_set_mode(i, mode, USB_ALLOW_SUSPEND_CHARGE);
	}
#endif
}
