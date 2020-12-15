/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include <stdbool.h>
#include <stdint.h>
#include "compile_time_macros.h"
#include "console.h"
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
	CPRINTS("overridable is called");
	return 0;
}

void bb_retimer_fw_update_set_mode(int port, int mode)
{
	ASSERT(port >= 0 && port < CONFIG_USB_PD_PORT_MAX_COUNT);

	CPRINTS("retimer fw update: port %d, mode %d", port, mode);

	switch (mode) {
	case EC_ACPI_MEM_BB_RETIMER_DISCONNECT:
		/* Disconnect */
		usb_mux_set(port, USB_PD_MUX_NONE,
			USB_SWITCH_DISCONNECT, pd_get_polarity(port));
		pd_set_suspend(port, 0);
		break;
	case EC_ACPI_MEM_BB_RETIMER_USB:
		pd_set_suspend(port, 1);
		usb_mux_set(port, USB_PD_MUX_USB_ENABLED,
			USB_SWITCH_CONNECT, pd_get_polarity(port));
		break;
	case EC_ACPI_MEM_BB_RETIMER_SAFE:
		usb_mux_set_safe_mode(port);
		break;
	case EC_ACPI_MEM_BB_RETIMER_TBT:
		usb_mux_set(port, USB_PD_MUX_TBT_COMPAT_ENABLED,
			USB_SWITCH_CONNECT, pd_get_polarity(port));
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
		if (*e || op > EC_ACPI_MEM_BB_RETIMER_TBT)
			return EC_ERROR_PARAM2;

		bb_retimer_fw_update_set_mode(port, op);
		return EC_SUCCESS;
	}
	return EC_ERROR_PARAM_COUNT;
}

DECLARE_CONSOLE_COMMAND(bbupdate, command_bb_update,
			"[port][op]",
			"BB retimer fw update query/set mux");

