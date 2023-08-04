/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Source file for PD task to configure USB-C Alternate modes on Intel SoC.
 * Elaborate details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "intel_pd_task.h"
#include "task.h"
#include "usb_pd.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)

/* Store current data of the DATA STATUS register */
//static union data_status_reg data_status[CONFIG_USB_PD_PORT_MAX_COUNT];

static void intel_pd_process_data(int port)
{
}

/* Enable interrupt when AP is on */
static void enable_pd_irq(void)
{
	gpio_enable_interrupt(GPIO_USB_PD_INT_ODL);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, enable_pd_irq, HOOK_PRIO_DEFAULT);

/* Disable interrupt when AP is down to avoid unnecessary wake of AP */
static void disable_pd_irq(void)
{
	gpio_enable_interrupt(GPIO_USB_PD_INT_ODL);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, disable_pd_irq, HOOK_PRIO_DEFAULT);

void intel_pd_interrupt(enum gpio_signal signal)
{
	/* task_wake(TASK_ID_INTEL_PD_TASK); */
}

void intel_pd_task(void *u)
{
	int i;
	int pd_ports = board_get_usb_pd_port_count();

	while (1) {
		task_wait_event(-1);

		for (i = 0; i < pd_ports; i++)
			intel_pd_process_data(i);
	}
}
