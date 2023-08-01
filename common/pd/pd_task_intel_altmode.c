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
#include "pd_task_intel_altmode.h"
#include "task.h"
#include "usb_pd.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)

static void process_altmode_pd_data(int port)
{
}

/* Enable interrupt when AP is on */
static void enable_pd_irq(void)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++)
		gpio_enable_interrupt(pd_config[i].alert_signal);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, enable_pd_irq, HOOK_PRIO_DEFAULT);

/* Disable interrupt when AP is down to avoid unnecessary wake of AP */
static void disable_pd_irq(void)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++)
		gpio_disable_interrupt(pd_config[i].alert_signal);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, disable_pd_irq, HOOK_PRIO_DEFAULT);

void pd_altmode_interrupt(enum gpio_signal signal)
{
	task_wake(TASK_ID_PD_TASK_INTEL_ALTMODE);
}

void pd_task_intel_altmode(void *u)
{
	int i;

	while (1) {
		task_wait_event(-1);

		for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
			/* Process data of interrupted port */
			if (!gpio_get_level(pd_config[i].alert_signal))
				process_altmode_pd_data(i);
		}
	}
}
