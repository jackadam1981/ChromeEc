/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel-RVP family-specific configuration */

#include "console.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "include/gpio.h"
#include "system.h"
#include "tcpm/tcpci.h"
#include "usb_charge.h"
#include "usbc_ppc.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)

void tcpc_alert_event(enum gpio_signal signal)
{
#if 0
	int port;

	switch (signal) {
	case GPIO_USB_C2_TCPC_INT_ODL:
		port = 0;
		break;
	case GPIO_USB_C3_TCPC_INT_ODL:
		port = 1;
		break;
	default:
		return;
	}

	schedule_deferred_pd_interrupt(port);
#endif
}

uint16_t tcpc_get_alert_status(void)
{
#if 0
	uint16_t status = 0;
	int i;

	if (!gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(usbc_tcpc_alrt_p2)))
		status |= PD_STATUS_TCPC_ALERT_0;

	if (!gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(usbc_tcpc_alrt_p3)))
		status |= PD_STATUS_TCPC_ALERT_1;

	return status;
#else
	return 0;
#endif
}

void board_charging_enable(int port, int enable)
{
	int rv;

	rv = tcpc_config[port].drv->set_snk_ctrl(port, enable);

	if (rv) {
		CPRINTS("C%d: sink path %s failed", port,
			enable ? "en" : "dis");
	}
}

void bc12_interrupt(enum gpio_signal signal)
{
	switch (signal) {
	case GPIO_USB_C0_BC12_INT_ODL:
		usb_charger_task_set_event(0, USB_CHG_EVENT_BC12);
		break;
	case GPIO_USB_C1_BC12_INT_ODL:
		usb_charger_task_set_event(1, USB_CHG_EVENT_BC12);
		break;
	default:
		break;
	}
}
