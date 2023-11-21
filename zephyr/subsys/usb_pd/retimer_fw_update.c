/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Source file for PD retimer firmware update using infineon CCG8.
 */

#include "builtin/assert.h"
#include "console.h"
#include "hooks.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usbc/utils.h"

#include <stdlib.h>

#include <zephyr/shell/shell.h>

#include <drivers/ccg8_pd.h>
#include <usbc/pd_task_intel_altmode.h>
#include <usbc/retimer_fw_update.h>

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

LOG_MODULE_REGISTER(RETIMER_FWUPD, LOG_LEVEL_DBG);

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

static void enter_retimer_fw_update(void)
{
	/*
	 * Write to Port 0 I2C config. PD goes to retimer firmware
	 * update mode and asserts FORCE_PWR pin to all the retimers
	 * connected to the PD. Mostly, retimers share NVM flash.
	 */
	uint8_t data_fw_update = 0x01;
	int val = pd_write_powmode(pd_pow_config_array[0], PD_ICL_CTRL_REG,
				   PD_ICL_CTRL_REG_LEN, &data_fw_update);
	if (val != 0)
		LOG_INF("Enter Retimer firmware update mode failed\n");
	resume_pd_task();

	/*
	 * Retimer firmware update mode in the PD triggers altmode
	 * changes when the PD altmode task is suspended, PD altmode
	 * task misses the interrupts. Therefore, explicitly post event
	 * so PD altmode task updates the mux status after resuming.
	 */
	intel_altmode_post_event(INTEL_ALTMODE_EVENT_FORCE);
}
DECLARE_DEFERRED(enter_retimer_fw_update);

static void exit_retimer_fw_update(void)
{
	uint8_t data_fw_update = 0x00;

	/* PD exits retimer firmware update mode */
	int val = pd_write_powmode(pd_pow_config_array[0], PD_ICL_CTRL_REG,
				   PD_ICL_CTRL_REG_LEN, &data_fw_update);
	if (val != 0)
		LOG_INF("Enter Retimer firmware update mode failed\n");
	/* Clear fw_update_status */
	fw_update_status = 0;
	resume_pd_task();

	/*
	 * Exit retimer firmware update mode causes altmode changes
	 * when PD altmode task is suspended. Thus, explicitly post
	 * event for it to update mux status.
	 */
	intel_altmode_post_event(INTEL_ALTMODE_EVENT_FORCE);
}
DECLARE_DEFERRED(exit_retimer_fw_update);

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
	LOG_INF("last op =%d last result = %d\n", last_op, last_result);
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
			suspend_pd_task();
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
			hook_call_deferred(&enter_retimer_fw_update_data, 0);
			last_op = op;
			last_port = port;
		}
		break;
	case USB_RETIMER_FW_UPDATE_DISCONNECT:
		if (retimer_state == RETIMER_OFFLINE) {
			retimer_state = RETIMER_ONLINE_REQUESTED;
			/* Suspend PD altmode task to ignore altmode events */
			suspend_pd_task();
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
			hook_call_deferred(&exit_retimer_fw_update_data, 0);
			retimer_state = RETIMER_ONLINE;
			last_op = op;
			last_port = port;
		}
	default:
		break;
	}
	LOG_INF("op:%d last_result:%d port_state:%d\n", op, last_result,
		retimer_state);
}

#ifdef CONFIG_CONSOLE_CMD_PD_RETIMER_UPDATE
SHELL_STATIC_SUBCMD_SET_CREATE(sub_retimer_update,
			       SHELL_CMD_ARG(entry, NULL,
					     "Enter retimer firmware update\n",
					     enter_retimer_fw_update, 0, 0),
			       SHELL_CMD_ARG(exit, NULL,
					     "Exit retimer firmware update\n",
					     exit_retimer_fw_update, 0, 0),
			       SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(retimer_update, &sub_retimer_update, "PD Retimer fw update \
		mode", NULL);
#endif
