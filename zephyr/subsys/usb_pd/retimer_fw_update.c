/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Source file for retimer firmware update using Power Delivery chip.
 */

#include "builtin/assert.h"
#include "usb_mux.h"
#include "usbc/utils.h"

#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

#include <drivers/pd_chip.h>
#include <usbc/pd_task_intel_altmode.h>
#include <usbc/retimer_fw_update.h>

#define PD_CHIP_ENTRY(usbc_id, pd_id) \
	[USBC_PORT_NEW(usbc_id)] = DEVICE_DT_GET(pd_id),

#define CHECK_PD_CHIP(usbc_id)                                          \
	COND_CODE_1(DT_NODE_HAS_PROP(usbc_id, pdc),                     \
		    (PD_CHIP_ENTRY(usbc_id, DT_PHANDLE(usbc_id, pdc))), \
		    (NULL, ))

#define CHECK_RETIMER(usbc_id)                                              \
	COND_CODE_1(DT_PROP(usbc_id, pd_retimer), (CHECK_PD_CHIP(usbc_id)), \
		    (NULL, ))

#define PD_RETIMER(usbc_id)                                \
	COND_CODE_1(DT_NODE_HAS_PROP(usbc_id, pd_retimer), \
		    (CHECK_RETIMER(usbc_id)), ())

static const struct device *pd_retimer_ports[] = { DT_FOREACH_STATUS_OKAY(
	named_usbc_port, PD_RETIMER) };

#define USB_PD_RETIMER_FW_UPDATE_RUN BIT(0)
#define USB_PD_RETIMER_FW_UPDATE_LTD_RUN BIT(1)

LOG_MODULE_REGISTER(RETIMER_FWUPD, LOG_LEVEL_ERR);

static int last_op[ARRAY_SIZE(pd_retimer_ports)];
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

struct retimer_update_workq_info {
	struct k_work retimer_update_workq;
	int port;
};

/* Check from dts if retimer connected to pd */
static bool pd_retimer_present(int port)
{
	return pd_retimer_ports[port] == NULL ? false : true;
}

/* TODO: To be handled in PD_TASK */
static void enter_retimer_fw_update(struct k_work *work_item)
{
	struct retimer_update_workq_info *workq_info =
		CONTAINER_OF(work_item, struct retimer_update_workq_info,
			     retimer_update_workq);
	/*
	 * Write to Port 0 I2C config. PD goes to retimer firmware
	 * update mode and asserts FORCE_PWR pin to all the retimers
	 * connected to the PD. Mostly, retimers share NVM flash.
	 */
	int val = pd_intel_retimer_fw_update(pd_retimer_ports[workq_info->port],
					     true);
	if (val != 0)
		LOG_ERR("Enter Retimer firmware update mode failed");
	resume_pd_intel_altmode_task();
}
static struct retimer_update_workq_info enter_workq_info;

static void exit_retimer_fw_update(struct k_work *work_item)
{
	struct retimer_update_workq_info *workq_info =
		CONTAINER_OF(work_item, struct retimer_update_workq_info,
			     retimer_update_workq);

	/* PD exits retimer firmware update mode */
	int val = pd_intel_retimer_fw_update(pd_retimer_ports[workq_info->port],
					     false);
	if (val != 0)
		LOG_ERR("Enter Retimer firmware update mode failed");
	/* Clear fw_update_status */
	fw_update_status = 0;
	resume_pd_intel_altmode_task();
}
static struct retimer_update_workq_info exit_workq_info;

int usb_retimer_fw_update_get_result(void)
{
	if (last_port < 0 && last_port >= ARRAY_SIZE(pd_retimer_ports))
		return USB_RETIMER_FW_UPDATE_ERR;

	/* Check if any retimer present */
	if (!pd_retimer_present(last_port))
		return USB_RETIMER_FW_UPDATE_ERR;

	switch (last_op[last_port]) {
	case USB_RETIMER_FW_UPDATE_RESUME_PD:
		if (!(fw_update_status & USB_PD_RETIMER_FW_UPDATE_LTD_RUN)) {
			last_result = 1;
			last_op[last_port] = 0;
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
	case USB_RETIMER_FW_UPDATE_GET_MUX:
		last_result = (usb_mux_get(last_port) &
			       USB_RETIMER_FW_UPDATE_MUX_MASK) ==
					      USB_PD_MUX_NONE ?
				      USB_PD_MUX_NONE :
				      -1;
		break;
	default:
		last_result = usb_mux_get(last_port) &
			      USB_RETIMER_FW_UPDATE_MUX_MASK;
	}
	return last_result;
}

void usb_retimer_fw_update_process_op(int port, int op)
{
	ASSERT(port >= 0 && port < ARRAY_SIZE(pd_retimer_ports));

	if (!pd_retimer_present(port)) {
		last_port = port;
		last_op[last_port] = op;
		return;
	}

	switch (op) {
	case USB_RETIMER_FW_UPDATE_QUERY_PORT:
		break;
	case USB_RETIMER_FW_UPDATE_GET_MUX:
		if (retimer_state == RETIMER_ONLINE) {
			last_op[port] = op;
			last_port = port;
		}
		break;
	case USB_RETIMER_FW_UPDATE_SUSPEND_PD:
		if (retimer_state == RETIMER_ONLINE) {
			retimer_state = RETIMER_OFFLINE;
			/* Suspend PD altmode task to ignore altmode events */
			suspend_pd_intel_altmode_task();
			last_op[port] = op;
			last_port = port;
		}
		break;
	case USB_RETIMER_FW_UPDATE_SET_USB:
		if (retimer_state == RETIMER_OFFLINE) {
			usb_mux_set(port, USB_PD_MUX_USB_ENABLED,
				    USB_SWITCH_CONNECT, pd_get_polarity(port));
			last_op[port] = op;
			last_port = port;
		}
		break;
	case USB_RETIMER_FW_UPDATE_SET_SAFE:
		if (retimer_state == RETIMER_OFFLINE) {
			usb_mux_set(port, USB_PD_MUX_SAFE_MODE,
				    USB_SWITCH_CONNECT, pd_get_polarity(port));
			last_op[port] = op;
			last_port = port;
		}
		break;
	case USB_RETIMER_FW_UPDATE_SET_TBT:
		if (retimer_state == RETIMER_OFFLINE) {
			fw_update_status |= USB_PD_RETIMER_FW_UPDATE_RUN;
			enter_workq_info.port = port;
			k_work_init(&enter_workq_info.retimer_update_workq,
				    enter_retimer_fw_update);
			k_work_submit(&enter_workq_info.retimer_update_workq);
			last_op[port] = op;
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
			last_op[port] = op;
			last_port = port;
		}
		break;
	case USB_RETIMER_FW_UPDATE_RESUME_PD:
		if (retimer_state == RETIMER_ONLINE_REQUESTED) {
			fw_update_status |= USB_PD_RETIMER_FW_UPDATE_LTD_RUN;
			exit_workq_info.port = port;
			k_work_init(&exit_workq_info.retimer_update_workq,
				    exit_retimer_fw_update);
			k_work_submit(&exit_workq_info.retimer_update_workq);
			retimer_state = RETIMER_ONLINE;
			last_op[port] = op;
			last_port = port;
		}
	default:
		break;
	}
}
