/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Source file for PD task to configure USB-C Alternate modes on Intel SoC.
 */

#include "builtin/assert.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usbc/utils.h"
#include "console.h"
#include <stdlib.h>
#include "hooks.h"
#include <zephyr/shell/shell.h>

#include <usbc/pd_task_intel_altmode.h>
#include <drivers/ccg8_pd.h>
#include <usbc/retimer_fw_update.h>

#define USB_PD_RETIMER_FW_UPDATE_RUN BIT(0)
#define USB_PD_RETIMER_FW_UPDATE_LTD_RUN BIT(1)

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

static void hc_retimer_fw_update(void)
{
	cprints(CC_USBPD, "IN hc retimer fw update\n");
	uint8_t data_retimer_cmd[2] = {0x00,0x01};
	pd_write_powmode(pd_pow_config_array[0], PD_ICL_BB_RETIMER_CMD_REG,
			PD_ICL_BB_RETIMER_CMD_REG_LEN, &data_retimer_cmd);
	uint8_t data_fw_update = 0x01;
	int val = pd_write_powmode(pd_pow_config_array[0], PD_ICL_CTRL_REG,
			PD_ICL_CTRL_REG_LEN, &data_fw_update);
	cprints(CC_USBPD, "pd pow i2c write result:%d\n", val);
	resume_pd_task();
	intel_altmode_post_event(INTEL_ALTMODE_EVENT_FORCE);
	//return EC_RES_SUCCESS;
}
DECLARE_DEFERRED(hc_retimer_fw_update);

static void hc_exit_retimer_fw_update(void)
{
	cprints(CC_USBPD, "IN hc exit retimer fw update\n");
	uint8_t data_fw_update = 0x00;
	pd_write_powmode(pd_pow_config_array[0], PD_ICL_CTRL_REG,
			PD_ICL_CTRL_REG_LEN, &data_fw_update);
	/* Clear fw_update_status */
	fw_update_status = 0;
	resume_pd_task();
	intel_altmode_post_event(INTEL_ALTMODE_EVENT_FORCE);
}
DECLARE_DEFERRED(hc_exit_retimer_fw_update);

int usb_retimer_fw_update_get_result(void)
{
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
			last_result = (usb_mux_get(last_port) & USB_PD_MUX_USB_ENABLED) ?
				(usb_mux_get(last_port) & USB_PD_MUX_USB_ENABLED) : 
				USB_RETIMER_FW_UPDATE_INVALID_MUX;
			break;
		case USB_RETIMER_FW_UPDATE_SET_SAFE:
			last_result = (usb_mux_get(last_port) & USB_PD_MUX_SAFE_MODE) ?
				(usb_mux_get(last_port) & USB_PD_MUX_SAFE_MODE) :
				USB_RETIMER_FW_UPDATE_INVALID_MUX;
			break;
		case USB_RETIMER_FW_UPDATE_SET_TBT:
			//LOG_INF("mux state: %d\n", usb_mux_get(last_port));
			last_result = (usb_mux_get(last_port) & USB_PD_MUX_TBT_COMPAT_ENABLED) ?
				(usb_mux_get(last_port) & USB_PD_MUX_TBT_COMPAT_ENABLED) :
				USB_RETIMER_FW_UPDATE_INVALID_MUX;
			break;
		default:
			last_result = usb_mux_get(last_port) & USB_RETIMER_FW_UPDATE_MUX_MASK;
	}
	cprints(CC_USBPD, "last op =%d last result = %d\n", last_op, last_result);
	return last_result;
}

void usb_retimer_fw_update_process_op(int port, int op)
{
	ASSERT(port >= 0 && port < CONFIG_USB_PD_PORT_MAX_COUNT);

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
				suspend_pd_task();
				last_op = op;
				last_port = port;
			}
			break;
		case USB_RETIMER_FW_UPDATE_SET_USB:
			if (retimer_state == RETIMER_OFFLINE) {
				usb_mux_set(port, USB_PD_MUX_USB_ENABLED, USB_SWITCH_CONNECT, pd_get_polarity(port));
				last_op = op;
				last_port = port;
			}
			break;
		case USB_RETIMER_FW_UPDATE_SET_SAFE:
			if (retimer_state == RETIMER_OFFLINE) {
				usb_mux_set(port, USB_PD_MUX_SAFE_MODE, USB_SWITCH_CONNECT, pd_get_polarity(port));
				last_op = op;
				last_port = port;
			}
			break;
		case USB_RETIMER_FW_UPDATE_SET_TBT:
			if (retimer_state == RETIMER_OFFLINE) {
				fw_update_status |= USB_PD_RETIMER_FW_UPDATE_RUN;
				hook_call_deferred(&hc_retimer_fw_update_data, 0);
				last_op = op;
				last_port = port;
			}
			break;
		case USB_RETIMER_FW_UPDATE_DISCONNECT:
			if (retimer_state == RETIMER_OFFLINE) {
				retimer_state = RETIMER_ONLINE_REQUESTED;
				suspend_pd_task();
				usb_mux_set(port, USB_PD_MUX_NONE, USB_SWITCH_DISCONNECT, pd_get_polarity(port));
				last_op = op;
				last_port = port;
			}
			break;
		case USB_RETIMER_FW_UPDATE_RESUME_PD:
			if (retimer_state == RETIMER_ONLINE_REQUESTED) {
					fw_update_status |= USB_PD_RETIMER_FW_UPDATE_LTD_RUN;
					hook_call_deferred(&hc_exit_retimer_fw_update_data,0);
					retimer_state = RETIMER_ONLINE;
					last_op = op;
					last_port = port;
			}
		default:
			break;
	}
	cprints(CC_USBPD, "op:%d last_result:%d port_state:%d\n", op, last_result, retimer_state);
}

static int get_retimer_update_val(const struct shell *sh, char *arg_val, uint8_t *val)
{
	char *e;

	*val = strtoul(arg_val, &e, 0);
	if (*e || *val >= 2) {
		shell_error(sh, "Invalid value");
		return -EINVAL;
	}

	return 0;
}

static int cmd_retimer_update(const struct shell *sh, size_t argc,
		 char ** argv)
{
	uint8_t val;
	get_retimer_update_val(sh, argv[1], &val);
	if (val)
		hc_retimer_fw_update();
	else
		hc_exit_retimer_fw_update();
	return 0;
}

SHELL_CMD_REGISTER(retimer_update, NULL, "PD Retimer fw update mode.\
			usage: retimer_update <value> , 0- exit 1-entry",
			cmd_retimer_update);
