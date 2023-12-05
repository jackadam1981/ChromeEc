/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Source file for PD retimer firmware update using infineon CCG8.
 */

#include "builtin/assert.h"
#include "console.h"
#include "usb_mux.h"
#include <stdlib.h>
#include <zephyr/shell/shell.h>
#include <drivers/pdc_ccg8.h>
#include <usbc/pd_task_intel_altmode.h>
#include <usbc/retimer_fw_update.h>
#include "hooks.h"
#include "usbc/utils.h"
#include <zephyr/kernel.h>

#define USB_PD_RETIMER_FW_UPDATE_RUN BIT(0)
#define USB_PD_RETIMER_FW_UPDATE_LTD_RUN BIT(1)

#define PD_RETIMER_PRESENT(usbc_id, val) \
	[USBC_PORT_NEW(usbc_id)] = val,

#define PD_RETIMER(usbc_id)                                \
	COND_CODE_1(DT_NODE_HAS_PROP(usbc_id, pd_retimer), \
		    (PD_RETIMER_PRESENT(usbc_id, true)),   \
		    (PD_RETIMER_PRESENT(usbc_id, false)))

static const int pd_retimer_ports[] = { DT_FOREACH_STATUS_OKAY(named_usbc_port,
							       PD_RETIMER) };

LOG_MODULE_REGISTER(RETIMER_FWUPD, LOG_LEVEL_ERR);

/*
#define RETIMER_WORKQ_STACK_SIZE 512
#define RETIMER_WORKQ_PRIORITY 8

K_THREAD_STACK_DEFINE(retimer_workq_stack_area, RETIMER_WORKQ_STACK_SIZE);
struct k_work_q retimer_workq;
k_work_queue_init(&retimer_workq);
k_work_queue_start(&retimer_workq, retimer_workq_stack_area,
		   K_WORK_QUEUE_STACK_SIZEOF(retimer_workq_stack_area),
		   RETIMER_WORKQ_PRIORITY, NULL);
*/
static int last_op;
static int last_result;
static int last_port;
static int fw_update_status;

/* Retimer state before, while or after firmware update*/
enum retimer_states {
	RETIMER_ONLINE,
	RETIMER_OFFLINE,
	RETIMER_ONLINE_REQUESTED
};

static int retimer_state = RETIMER_ONLINE;

/* Check from dts if retimer connected to pd */
static bool pd_retimer_present(void)
{
	for (int port = 0; port < ARRAY_SIZE(pd_retimer_ports); port++)
		if (pd_retimer_ports[port])
			return true;
	return false;
}

static void enter_retimer_fw_update(struct k_work *work_item)
{
	/*
	 * Write to Port 0 I2C config. PD goes to retimer firmware
	 * update mode and asserts FORCE_PWR pin to all the retimers
	 * connected to the PD. Mostly, retimers share NVM flash.
	 */
	int val = pd_intel_retimer_fw_update(pd_pow_config_array[0], true);
	if (val != 0)
		LOG_ERR("Enter Retimer firmware update mode failed");
	resume_pd_intel_altmode_task();
}
static struct k_work retimer_update_enter;

static void exit_retimer_fw_update(struct k_work *work_item)
{
	/* PD exits retimer firmware update mode */
	int val = pd_intel_retimer_fw_update(pd_pow_config_array[0], false);
	if (val != 0)
		LOG_ERR("Enter Retimer firmware update mode failed");
	/* Clear fw_update_status */
	fw_update_status = 0;
	resume_pd_intel_altmode_task();
}
static struct k_work retimer_update_exit;

int usb_retimer_fw_update_get_result(void)
{
	if (!pd_retimer_present())
		return USB_RETIMER_FW_UPDATE_ERR;

	switch (last_op) {
	case USB_RETIMER_FW_UPDATE_RESUME_PD:
		if (!(fw_update_status & USB_PD_RETIMER_FW_UPDATE_LTD_RUN)) {
			last_result = 1;
			last_op = 0;
		} else {
			last_result = USB_RETIMER_FW_UPDATE_INVALID_MUX;
		}
		break;
	case USB_RETIMER_FW_UPDATE_SET_USB:
		last_result =
			(usb_mux_get(last_port) & USB_PD_MUX_USB_ENABLED) ?
				(usb_mux_get(last_port) &
				 USB_PD_MUX_USB_ENABLED) :
				USB_RETIMER_FW_UPDATE_INVALID_MUX;
		break;
	case USB_RETIMER_FW_UPDATE_SET_SAFE:
		last_result = (usb_mux_get(last_port) & USB_PD_MUX_SAFE_MODE) ?
				      (usb_mux_get(last_port) &
				       USB_PD_MUX_SAFE_MODE) :
				      USB_RETIMER_FW_UPDATE_INVALID_MUX;
		break;
	case USB_RETIMER_FW_UPDATE_SET_TBT:
		last_result = (usb_mux_get(last_port) &
			       USB_PD_MUX_TBT_COMPAT_ENABLED) ?
				      (usb_mux_get(last_port) &
				       USB_PD_MUX_TBT_COMPAT_ENABLED) :
				      USB_RETIMER_FW_UPDATE_INVALID_MUX;
		break;
	default:
		last_result = usb_mux_get(last_port) &
			      USB_RETIMER_FW_UPDATE_MUX_MASK;
	}
	return last_result;
}

void usb_retimer_fw_update_process_op(int port, int op)
{
	ASSERT(port >= 0 && port < CONFIG_USB_PD_PORT_MAX_COUNT);

	if (!pd_retimer_present())
		return;

	switch (op) {
	case USB_RETIMER_FW_UPDATE_QUERY_PORT:
		break;
	case USB_RETIMER_FW_UPDATE_GET_MUX:
		if (retimer_state == RETIMER_ONLINE) {
			last_op = op;
			last_port = port;
		}
		break;
	case USB_RETIMER_FW_UPDATE_SUSPEND_PD:
		if (retimer_state == RETIMER_ONLINE) {
			retimer_state = RETIMER_OFFLINE;
			/* Suspend PD altmode task to ignore altmode events */
			suspend_pd_intel_altmode_task();
			last_op = op;
			last_port = port;
		}
		break;
	case USB_RETIMER_FW_UPDATE_SET_USB:
		if (retimer_state == RETIMER_OFFLINE) {
			usb_mux_set(port, USB_PD_MUX_USB_ENABLED,
				    USB_SWITCH_CONNECT, pd_get_polarity(port));
			last_op = op;
			last_port = port;
		}
		break;
	case USB_RETIMER_FW_UPDATE_SET_SAFE:
		if (retimer_state == RETIMER_OFFLINE) {
			usb_mux_set(port, USB_PD_MUX_SAFE_MODE,
				    USB_SWITCH_CONNECT, pd_get_polarity(port));
			last_op = op;
			last_port = port;
		}
		break;
	case USB_RETIMER_FW_UPDATE_SET_TBT:
		if (retimer_state == RETIMER_OFFLINE) {
			fw_update_status |= USB_PD_RETIMER_FW_UPDATE_RUN;
			k_work_init(&retimer_update_enter, enter_retimer_fw_update);
			k_work_submit(&retimer_update_enter);
			last_op = op;
			last_port = port;
		}
		break;
	case USB_RETIMER_FW_UPDATE_DISCONNECT:
		if (retimer_state == RETIMER_OFFLINE) {
			retimer_state = RETIMER_ONLINE_REQUESTED;
			/* Suspend PD altmode task to ignore altmode events */
			suspend_pd_intel_altmode_task();
			usb_mux_set(port, USB_PD_MUX_NONE,
				    USB_SWITCH_DISCONNECT,
				    pd_get_polarity(port));
			last_op = op;
			last_port = port;
		}
		break;
	case USB_RETIMER_FW_UPDATE_RESUME_PD:
		if (retimer_state == RETIMER_ONLINE_REQUESTED) {
			fw_update_status |= USB_PD_RETIMER_FW_UPDATE_LTD_RUN;
			k_work_init(&retimer_update_exit, exit_retimer_fw_update);
			k_work_submit(&retimer_update_exit);
			retimer_state = RETIMER_ONLINE;
			last_op = op;
			last_port = port;
		}
	default:
		break;
	}
}
