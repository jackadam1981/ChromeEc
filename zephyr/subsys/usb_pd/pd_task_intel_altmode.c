/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Source file for PD task to configure USB-C Alternate modes on Intel SoC.
 * Elaborate details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */

#include "gpio.h"
#include "pd_task_intel_altmode.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(usbpd_altmode, CONFIG_USB_PD_ALTMODE_LOG_LEVEL);

static struct intel_altmode_data intel_altmode_task_data;

static const struct device *intel_altmode_get_instance(void);

const struct pd_config_t pd_config[2];
static void intel_altmode_set_event(enum intel_altmode_event event)
{
	const struct device *dev = intel_altmode_get_instance();
	struct intel_altmode_data *const data = dev->data;

	k_event_post(&data->evt, BIT(event));
}

static void intel_altmode_suspend_handler(struct ap_power_ev_callback *cb,
					  struct ap_power_ev_data data)
{
	int i;

	LOG_DBG("suspend event: 0x%x", data.event);

	if (data.event == AP_POWER_RESUME) {
		/* Enable interrupt when AP is on */
		for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++)
			gpio_enable_interrupt(pd_config[i].alert_signal);

		/* Set event to forcefully get new PD data */
		intel_altmode_set_event(INTEL_ALTMODE_EVENT_FORCE);
	} else if (data.event == AP_POWER_SUSPEND) {
		/*
		 * Disable interrupt when the AP is down to avoid unnecessary
		 * wake of AP
		 */
		for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++)
			gpio_disable_interrupt(pd_config[i].alert_signal);
	} else {
		LOG_ERR("Invalid suspend event");
	}
}

static uint32_t intel_altmode_wait_event(const struct device *dev)
{
	struct intel_altmode_data *const data = dev->data;
	uint32_t events;

	events = k_event_wait(&data->evt, INTEL_ALTMODE_EVENT_COUNT, false,
			      Z_FOREVER);
	/* Clear all events posted */
	k_event_clear(&data->evt, events);

	return events & INTEL_ALTMODE_EVENT_COUNT;
}

void intel_altmode_interrupt(enum gpio_signal signal)
{
	/* PD interrupt event */
	intel_altmode_set_event(INTEL_ALTMODE_EVENT_INTERRUPT);
}

static void process_altmode_pd_data(int port)
{
	LOG_DBG("Process p% data", port);
}

static void intel_altmode_thread(void *arg, void *unused1, void *unused2)
{
	int i;
	uint32_t events;
	struct device *const dev = (struct device *)arg;

	/* Add callbacks for suspend hooks */
	ap_power_ev_init_callback(&intel_altmode_task_data.cb,
				  intel_altmode_suspend_handler,
				  AP_POWER_RESUME | AP_POWER_SUSPEND);
	ap_power_ev_add_callback(&intel_altmode_task_data.cb);

	LOG_INF("Intel Altmode thread start");

	while (1) {
		events = intel_altmode_wait_event(dev);

		if (events & INTEL_ALTMODE_EVENT_INTERRUPT) {
			for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
				/* Process data of interrupted port */
				if (!gpio_get_level(pd_config[i].alert_signal))
					process_altmode_pd_data(i);
			}
		} else if (events & INTEL_ALTMODE_EVENT_FORCE) {
			/* Process data for any wake events on all ports */
			for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++)
				process_altmode_pd_data(i);
		}
	}
}

static int intel_altmode_driver_init(const struct device *dev)
{
	return 0;
}

DEVICE_DEFINE(intel_altmode_dev, "intel_altmode_drv", intel_altmode_driver_init,
	      NULL, &intel_altmode_task_data, NULL, APPLICATION,
	      CONFIG_APPLICATION_INIT_PRIORITY, NULL);

K_THREAD_DEFINE(intel_altmode_tid, CONFIG_TASK_PD_ALTMODE_INTEL_STACK_SIZE,
		intel_altmode_thread, DEVICE_GET(intel_altmode_dev), NULL, NULL,
		CONFIG_USBPD_ALTMODE_INTEL_THREAD_PRIORITY, 0, K_TICKS_FOREVER);

static const struct device *intel_altmode_get_instance(void)
{
	return DEVICE_GET(intel_altmode_dev);
}
