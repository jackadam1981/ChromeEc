/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Source file for PD task to configure USB-C Alternate modes on Intel SoC.
 * Elaborate details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */

#include "i2c.h"
#include "i2c/i2c.h"
#include "pd_task_intel_altmode.h"
#include "usbc/utils.h"

#include <zephyr/logging/log.h>

#define INTEL_ALTMODE_COMPAT_PD intel_pd_altmode

#define PD_CHIP_ENTRY(usbc_id, pd_id, config_fn) \
	[USBC_PORT_NEW(usbc_id)] = config_fn(pd_id),

#define CHECK_COMPAT(compat, usbc_id, pd_id, config_fn) \
	COND_CODE_1(DT_NODE_HAS_COMPAT(pd_id, compat),  \
		    (PD_CHIP_ENTRY(usbc_id, pd_id, config_fn)), ())

#define PD_CHIP_FIND(usbc_id, pd_id) \
	CHECK_COMPAT(INTEL_ALTMODE_COMPAT_PD, usbc_id, pd_id, DEVICE_DT_GET)

#define PD_CHIP(usbc_id)                                                      \
	COND_CODE_1(DT_NODE_HAS_PROP(usbc_id, pd_altmode),                    \
		    (PD_CHIP_FIND(usbc_id, DT_PHANDLE(usbc_id, pd_altmode))), \
		    ())

/* Generate PD structure */
static const struct device *pd_config_array[] = { DT_FOREACH_STATUS_OKAY(
	named_usbc_port, PD_CHIP) };

BUILD_ASSERT(ARRAY_SIZE(pd_config_array) == CONFIG_USB_PD_PORT_MAX_COUNT);

LOG_MODULE_DECLARE(usbpd_altmode, CONFIG_USB_PD_ALTMODE_LOG_LEVEL);

/* Store current data of the DATA STATUS register */
static union data_status_reg data_status[CONFIG_USB_PD_PORT_MAX_COUNT];

static struct intel_altmode_data intel_altmode_task_data;

static const struct device *intel_altmode_get_instance(void);

void intel_altmode_post_event(enum intel_altmode_event event)
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
			pd_altmode_driver_api.isr_enable(pd_config_array[i],
							 true);

		/* Set event to forcefully get new PD data */
		intel_altmode_post_event(INTEL_ALTMODE_EVENT_FORCE);
	} else if (data.event == AP_POWER_SUSPEND) {
		/*
		 * Disable interrupt when the AP is down to avoid unnecessary
		 * wake of AP
		 */
		for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++)
			pd_altmode_driver_api.isr_enable(pd_config_array[i],
							 false);
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

static void process_altmode_pd_data(int port)
{
	int rv;
	union data_status_reg status;
	union data_control_reg control = { .i2c_int_ack = 1 };

	LOG_DBG("Process p% data", port);

	/* Clear the interrupt */
	rv = pd_altmode_driver_api.write(pd_config_array[port], &control);
	if (rv) {
		LOG_ERR("P%d write Err=%d", port, rv);
		return;
	}

	/* Read the status register */
	rv = pd_altmode_driver_api.read(pd_config_array[port], &status);
	if (rv) {
		LOG_ERR("P%d read Err=%d", port, rv);
		return;
	}

	/* Nothing to do if the data in the status register has not changed */
	if (!memcmp(&status.raw_value[0], &data_status[port].raw_value[0],
		    sizeof(union data_status_reg)))
		return;

	/* Update the new data */
	memcpy(&data_status[port], &status, sizeof(union data_status_reg));

	/* TODO: Process MUX events */
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
				if (pd_altmode_driver_api.is_interrupted(
					    pd_config_array[i]))
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
