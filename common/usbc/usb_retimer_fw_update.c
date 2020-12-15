/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include <stdbool.h>
#include <stdint.h>
#include "compile_time_macros.h"
#include "console.h"
#include "timer.h"
#include "usb_common.h"
#include "usb_mux.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#else
#define CPRINTS(format, args...)
#define CPRINTF(format, args...)
#endif

__overridable int bb_retimer_fw_update_query_port(void)
{
	return 0;
}

void bb_retimer_fw_update_set_mode(int port, int mode)
{
	mux_state_t mux;

	ASSERT(port >= 0 && port < CONFIG_USB_PD_PORT_MAX_COUNT);

	CPRINTS("C%d retimer firmware update, set mode %d", port, mode);

	switch (mode) {
	/* Host starts retimer update sequence */
	case EC_ACPI_MEM_BB_RETIMER_USB:
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
	                 * host can udpate retimer firmware directly without
			 * going through USB->Safe->TBT.
			 */
			break;
		}

		usb_mux_set(port, USB_PD_MUX_USB_ENABLED,
			USB_SWITCH_CONNECT, pd_get_polarity(port));

		break;
	case EC_ACPI_MEM_BB_RETIMER_SAFE:
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
	case EC_ACPI_MEM_BB_RETIMER_TBT:
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
	case EC_ACPI_MEM_BB_RETIMER_DISCONNECT:
		/* Disconnect */
		usb_mux_set(port, USB_PD_MUX_NONE,
			USB_SWITCH_DISCONNECT, pd_get_polarity(port));
		pd_set_suspend(port, 0);
		break;
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
		if (*e || op > EC_ACPI_MEM_BB_RETIMER_TBT + 4)
			return EC_ERROR_PARAM2;

		if (op < 4)
			bb_retimer_fw_update_set_mode(port, op);
		else if (op == 4) {
			CPRINTS("set usb mux");
			usb_mux_set(port, USB_PD_MUX_USB_ENABLED,
				USB_SWITCH_CONNECT, pd_get_polarity(port));
		} else if (op == 5) {
			CPRINTS("set safe mux");
			usb_mux_set(port, USB_PD_MUX_SAFE_MODE,
				USB_SWITCH_CONNECT, pd_get_polarity(port));
		} else if (op == 6) {
			CPRINTS("set TBT mux");
			usb_mux_set(port, USB_PD_MUX_TBT_COMPAT_ENABLED,
				USB_SWITCH_CONNECT, pd_get_polarity(port));
		} else if (op == 7) {
			CPRINTS("diconnect mux");
			usb_mux_set(port, USB_PD_MUX_NONE,
				USB_SWITCH_DISCONNECT, pd_get_polarity(port));
		}
		return EC_SUCCESS;
	}
	return EC_ERROR_PARAM_COUNT;
}

DECLARE_CONSOLE_COMMAND(bbupdate, command_bb_update,
			"[port][op]",
			"BB retimer fw update query/set mux");

