/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include <stdbool.h>
#include <stdint.h>
#include "compile_time_macros.h"
#include "console.h"
#include "hooks.h"
#include "timer.h"
#include "usb_common.h"
#include "usb_mux.h"
#include "usb_tc_sm.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#else
#define CPRINTS(format, args...)
#define CPRINTF(format, args...)
#endif

static int last_op;
static int cur_port;
static int last_mux_result;

#define BB_RETIMER_FW_UPDATE_PORT_INFO		0
#define BB_RETIMER_FW_UPDATE_PD_SUSPEND		1
#define BB_RETIMER_FW_UPDATE_PD_RESUME		2
#define BB_RETIMER_FW_UPDATE_GET_MUX            3
#define BB_RETIMER_FW_UPDATE_SET_USB		4
#define BB_RETIMER_FW_UPDATE_SET_SAFE		5
#define BB_RETIMER_FW_UPDATE_SET_TBT		6
#define BB_RETIMER_FW_UPDATE_DISCONNECT		7

static void deferred_pd_suspend(void)
{
	pd_set_suspend(cur_port, 1);
}
DECLARE_DEFERRED(deferred_pd_suspend);

__overridable int bb_retimer_fw_update_query_port(void)
{
	return 0;
}

int bb_retimer_fw_update_get_result(void)
{
	int result = 0;

	switch (last_op) {
	case BB_RETIMER_FW_UPDATE_PD_SUSPEND:
	case BB_RETIMER_FW_UPDATE_PD_RESUME:
		result = pd_is_port_enabled(cur_port);
		CPRINTS(" C%d----port is enabled = %d", cur_port, result);
		break;
	case BB_RETIMER_FW_UPDATE_PORT_INFO:
		result = bb_retimer_fw_update_query_port();
		break;
	case BB_RETIMER_FW_UPDATE_GET_MUX:
	case BB_RETIMER_FW_UPDATE_SET_USB:
	case BB_RETIMER_FW_UPDATE_SET_SAFE:
	case BB_RETIMER_FW_UPDATE_SET_TBT:
	case BB_RETIMER_FW_UPDATE_DISCONNECT:
		result = last_mux_result;
		break;
	default:
		break;
	}
	return result;
}

void bb_retimer_fw_update_process_mux_op(int port)
{
	switch (last_op) {
	case BB_RETIMER_FW_UPDATE_GET_MUX:
		last_mux_result = usb_mux_get(port);
		break;
	case BB_RETIMER_FW_UPDATE_SET_USB:
		CPRINTS("C%d-------set mux to USB", port);
		usb_mux_set(cur_port, USB_PD_MUX_USB_ENABLED,
			USB_SWITCH_CONNECT, pd_get_polarity(port));
		last_mux_result = usb_mux_get(port);
		break;
	case BB_RETIMER_FW_UPDATE_SET_SAFE:
		CPRINTS("C%d-------set mux to Safe", port);
		usb_mux_set_safe_mode(port);
		last_mux_result = usb_mux_get(port);
		break;
	case BB_RETIMER_FW_UPDATE_SET_TBT:
		CPRINTS("C%d-------set mux to TBT", port);
		usb_mux_set(cur_port, USB_PD_MUX_TBT_COMPAT_ENABLED,
			USB_SWITCH_CONNECT, pd_get_polarity(port));
		last_mux_result = usb_mux_get(port);
		break;
	case BB_RETIMER_FW_UPDATE_DISCONNECT:
		CPRINTS("C%d-------disconnect mux", port);
		usb_mux_set(cur_port, USB_PD_MUX_NONE,
			USB_SWITCH_DISCONNECT, pd_get_polarity(port));
		last_mux_result = usb_mux_get(port);
		break;
	default:
		break;
	}
}



void bb_retimer_fw_update_process_op(int port, int op)
{
	ASSERT(port >= 0 && port < CONFIG_USB_PD_PORT_MAX_COUNT);

	cur_port = port;
	last_op = op;
	CPRINTS("C%d retimer firmware update, set op %d", port, op);

	switch (op) {
	case BB_RETIMER_FW_UPDATE_PD_SUSPEND:
		hook_call_deferred(&deferred_pd_suspend_data, 1);
		break;
	case BB_RETIMER_FW_UPDATE_PD_RESUME:
		pd_set_suspend(port, 0);
		break;
	case BB_RETIMER_FW_UPDATE_PORT_INFO:
		break;
	case BB_RETIMER_FW_UPDATE_GET_MUX:
	case BB_RETIMER_FW_UPDATE_SET_USB:
	case BB_RETIMER_FW_UPDATE_SET_SAFE:
	case BB_RETIMER_FW_UPDATE_SET_TBT:
	case BB_RETIMER_FW_UPDATE_DISCONNECT:
		tc_bb_firmware_fw_update_set_flag(port);
		break;



#if 0
	/* Host starts retimer update sequence */
	case BB_RETIMER_FW_UPDATE_SET_USB: /*EC_ACPI_MEM_BB_RETIMER_USB:*/
		tc_bb_firmware_fw_update_set_flag(port);

		pd_set_suspend(port, 1);
		mux = usb_mux_get(port);

		/* If DP=1, disconnect then set USB mux */
		if (mux & USB_PD_MUX_DP_ENABLED) {
			usb_mux_set(port, USB_PD_MUX_NONE,
				USB_SWITCH_DISCONNECT, pd_get_polarity(port));
		} else if ((mux & USB_PD_MUX_USB_ENABLED) ||
			(mux & USB_PD_MUX_TBT_COMPAT_ENABLED) ||
			(mux & USB_PD_MUX_USB4_ENABLED)) {

			/* Mux is USB, skip setting USB.
			 * Mux is TBT/USB4, skip setting USB, Safe, and TBT,
			 * host can update retimer firmware directly without
			 * going through USB->Safe->TBT.
			 */
			break;
		}

		usb_mux_set(port, USB_PD_MUX_USB_ENABLED,
			USB_SWITCH_CONNECT, pd_get_polarity(port));
		break;
	case BB_RETIMER_FW_UPDATE_SET_SAFE: /*EC_ACPI_MEM_BB_RETIMER_SAFE:*/

		if (pd_is_port_enabled(port)) {
			CPRINTS("C%d enabled. Can't set safe mode",
				port);
			break;
		}

		mux = usb_mux_get(port);
		if (mux & USB_PD_MUX_USB_ENABLED)
			usb_mux_set_safe_mode(port);
		else if (!(mux & USB_PD_MUX_TBT_COMPAT_ENABLED) &&
				!(mux & USB_PD_MUX_USB4_ENABLED))
			CPRINTS("C%d Can't set safe mode, current mux 0x%x)",
				port, mux);

		break;
	case BB_RETIMER_FW_UPDATE_SET_TBT: /*EC_ACPI_MEM_BB_RETIMER_TBT:*/

		if (pd_is_port_enabled(port)) {
			CPRINTS("C%d enabled. Can't set TBT mode", port);
			break;
		}

		mux = usb_mux_get(port);
		if (mux & USB_PD_MUX_SAFE_MODE) {
			usb_mux_set(port, USB_PD_MUX_TBT_COMPAT_ENABLED,
				USB_SWITCH_CONNECT, pd_get_polarity(port));
		} else if (!(mux & USB_PD_MUX_TBT_COMPAT_ENABLED) &&
				!(mux & USB_PD_MUX_USB4_ENABLED)) {
			CPRINTS("C%d BB fw update failed", mux);
		}

		break;
	case BB_RETIMER_FW_UPDATE_DISCONNECT:

		/* Disconnect */
		usb_mux_set(port, USB_PD_MUX_NONE,
			USB_SWITCH_DISCONNECT, pd_get_polarity(port));
		/*pd_set_suspend(port, 0);*/

		break;
#endif
	default:
		break;
	}
}

static int command_bb_update(int argc, char **argv)
{
	int result;
	int port;
	int op;
	char *e;

	if (argc == 1) {
		result = bb_retimer_fw_update_query_port();
		CPRINTS("query result: 0x%x", result);
		return EC_SUCCESS;
	} else if (argc == 3) {
		port = strtoi(argv[1], &e, 10);
		if (*e || port >= board_get_usb_pd_port_count())
			return EC_ERROR_PARAM1;
		op = strtoi(argv[2], &e, 10);
		if (*e || op > BB_RETIMER_FW_UPDATE_DISCONNECT + 1)
			return EC_ERROR_PARAM2;

		if (op < 8)
			bb_retimer_fw_update_process_op(port, op);
		else if (op == 8) {
			CPRINTS("---------reslut: 0x%x",
				bb_retimer_fw_update_get_result());

		}
		return EC_SUCCESS;
	}
	return EC_ERROR_PARAM_COUNT;
}

DECLARE_CONSOLE_COMMAND(bbupdate, command_bb_update,
			"[port][op]",
			"BB retimer fw update query/set mux");
