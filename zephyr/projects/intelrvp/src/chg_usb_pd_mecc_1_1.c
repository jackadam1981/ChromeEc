/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel-RVP family-specific configuration */

#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "ioexpander.h"
#include "tcpm/tcpci.h"
#include "system.h"
#include "usbc_ppc.h"
#include "intelrvp.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

static void baseboard_tcpc_init(void)
{
	int i;

	/* Only reset TCPC if not sysjump */
	if (!system_jumped_late())
		board_reset_pd_mcu();

	for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
		/* Enable PPC interrupts. */
		if (tcpc_aic_gpios[i].ppc_intr_handler) {
			gpio_enable_interrupt(tcpc_aic_gpios[i].ppc_alert);
		}

		/* Enable TCPC interrupts. */
		if (tcpc_config[i].bus_type != EC_BUS_TYPE_EMBEDDED) {
			gpio_enable_interrupt(tcpc_aic_gpios[i].tcpc_alert);
		}
	}

	gpio_enable_interrupt(GPIO_CCD_MODE_ODL);
}
DECLARE_HOOK(HOOK_INIT, baseboard_tcpc_init, HOOK_PRIO_INIT_I2C + 1);

void tcpc_alert_event(enum gpio_signal signal)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
		/* No alerts for embdeded TCPC */
		if (tcpc_config[i].bus_type == EC_BUS_TYPE_EMBEDDED)
			continue;

		if (signal == tcpc_aic_gpios[i].tcpc_alert) {
			schedule_deferred_pd_interrupt(i);
			break;
		}
	}
}

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;
	int i;

	/* Check which port has the ALERT line set */
	for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
		/* No alerts for embdeded TCPC */
		if (tcpc_config[i].bus_type == EC_BUS_TYPE_EMBEDDED)
			continue;

		if (!gpio_get_level(tcpc_aic_gpios[i].tcpc_alert))
			status |= PD_STATUS_TCPC_ALERT_0 << i;
	}

	return status;
}

int ppc_get_alert_status(int port)
{
	return tcpc_aic_gpios[port].ppc_intr_handler &&
		!gpio_get_level(tcpc_aic_gpios[port].ppc_alert);
}

/* PPC support routines */
void ppc_interrupt(enum gpio_signal signal)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
		if (tcpc_aic_gpios[i].ppc_intr_handler &&
			signal == tcpc_aic_gpios[i].ppc_alert) {
			tcpc_aic_gpios[i].ppc_intr_handler(i);
			break;
		}
	}
}

void board_charging_enable(int port, int enable)
{
	if (tcpc_aic_gpios[port].ppc_intr_handler ?
		ppc_vbus_sink_enable(port, enable) :
		tcpc_config[port].drv->set_snk_ctrl(port, enable))
		CPRINTS("C%d: sink path %s failed",
				port, enable ? "en" : "dis");
}
