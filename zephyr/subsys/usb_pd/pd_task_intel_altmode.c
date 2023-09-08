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

#include <stdlib.h>

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

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

/* Generate device tree for available PDs */
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
			pd_altmode_isr_enable(pd_config_array[i], true);

		/* Set event to forcefully get new PD data */
		intel_altmode_post_event(INTEL_ALTMODE_EVENT_FORCE);
	} else if (data.event == AP_POWER_SUSPEND) {
		/*
		 * Disable interrupt when the AP is down to avoid unnecessary
		 * wake of AP
		 */
		for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++)
			pd_altmode_isr_enable(pd_config_array[i], false);
	} else {
		LOG_ERR("Invalid suspend event");
	}
}

static uint32_t intel_altmode_wait_event(const struct device *dev)
{
	struct intel_altmode_data *const data = dev->data;
	uint32_t events;

	events = k_event_wait(&data->evt, INTEL_ALTMODE_EVENT_MASK, false,
			      Z_FOREVER);
	/* Clear all events posted */
	k_event_clear(&data->evt, events);

	return events & INTEL_ALTMODE_EVENT_MASK;
}

static void process_altmode_pd_data(int port)
{
	int rv;
	union data_status_reg status;
	union data_control_reg control = { .i2c_int_ack = 1 };

	LOG_DBG("Process p% data", port);

	/* Clear the interrupt */
	rv = pd_altmode_write(pd_config_array[port], &control);
	if (rv) {
		LOG_ERR("P%d write Err=%d", port, rv);
		return;
	}

	/* Read the status register */
	rv = pd_altmode_read(pd_config_array[port], &status);
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
			/* Process data of interrupted port */
			for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
				if (pd_altmode_is_interrupted(
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

void intel_altmode_task_start(void)
{
	k_thread_start(intel_altmode_tid);
}

#ifdef CONFIG_CONSOLE_CMD_USBPD_INTEL_ALTMODE
static int console_command_intel_altmode(const struct shell *shell, size_t argc,
					 char **argv)
{
	int port, rv, i;
	char rw, *e;
	uint16_t val1;
	uint32_t val2 = 0;
	union data_status_reg status;
	union data_control_reg control;

	if (argc < 4 || argc > 5)
		return EC_ERROR_PARAM_COUNT;

	/* Get PD port number */
	port = strtol(argv[1], &e, 0);
	if (*e || port > CONFIG_USB_PD_PORT_MAX_COUNT)
		return EC_ERROR_PARAM1;

	/* Validate r/w selection */
	rw = argv[2][0];
	if (rw != 'w' && rw != 'r')
		return EC_ERROR_PARAM2;

	if (rw == 'r') {
		rv = pd_altmode_read(pd_config_array[port], &status);
		if (rv)
			return rv;

		shell_fprintf(shell, SHELL_INFO, "RD_VAL: ");
		for (i = 0; i < DATA_STATUS_REG_LEN; i++)
			shell_fprintf(shell, SHELL_INFO, "[%d]0x%x, ", i,
				      status.raw_value[i]);
		shell_fprintf(shell, SHELL_INFO, "\n");
	} else {
		val1 = strtoull(argv[3], &e, 0);
		if (*e)
			return EC_ERROR_PARAM3;

		if (argc > 4) {
			val2 = strtoull(argv[4], &e, 0);
			if (*e)
				return EC_ERROR_PARAM4;
		}

		memcpy(&control.raw_value[0], &val1, 2);
		memcpy(&control.raw_value[2], &val2, 4);

		rv = pd_altmode_write(pd_config_array[port],
						 &control);
		if (rv)
			return rv;

		shell_fprintf(shell, SHELL_INFO, "WR_VAL: ");
		for (i = 0; i < DATA_CONTROL_REG_LEN; i++)
			shell_fprintf(shell, SHELL_INFO, "[%d]0x%x, ", i,
				      control.raw_value[i]);
		shell_fprintf(shell, SHELL_INFO, "\n");
	}

	return rv;
}

SHELL_CMD_REGISTER(powerinfo, NULL,
		   "<port> r\n"
		   "<port> w <val1> | <val2>\n"
		   "Read or write to PD reg",
		   console_command_intel_altmode);
#endif /* CONFIG_CONSOLE_CMD_USBPD_INTEL_ALTMODE */
