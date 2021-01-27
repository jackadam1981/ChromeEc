/* Copyright 2021 The Chromium OS Authors. All rights reserved.
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

/* Track current port AP requested to update BB retimer firmware */
static int cur_port;
static int last_op; /* Operation received from AP via ACPI_WRITE */
/* MUX value returned to ACPI_READ */
static int last_mux_result = USB_RETIMER_FW_UPDATE_INVALID_MUX;

static void deferred_pd_suspend(void)
{
	pd_set_suspend(cur_port, 1);
}
DECLARE_DEFERRED(deferred_pd_suspend);

/**
 * Query USB-C ports state for USB retimer firmware update.
 * Support up to 8 ports.
 *
 * @return Bits[7:0]: represent PD ports 0-7
 *         1 - This port has retimer;
 *         0 - No retimer.
 */
static int usb_retimer_fw_update_query_port(void)
{
	int i;
	int port_info = 0;
	const struct usb_mux *mux_ptr;

	for (i = 0; i < USBC_PORT_COUNT; i++) {
		mux_ptr = &usb_muxes[i];
		while (mux_ptr && mux_ptr->has_retimer) {
			port_info |= BIT(i);
			mux_ptr = mux_ptr->next_mux;
		}
	}
	return port_info;
}

int usb_retimer_fw_update_get_result(void)
{
	int result = 0;

	switch (last_op) {
	case USB_RETIMER_FW_UPDATE_SUSPEND_PD:
	case USB_RETIMER_FW_UPDATE_RESUME_PD:
		result = pd_is_port_enabled(cur_port);
		break;
	case USB_RETIMER_FW_UPDATE_QUERY_PORT:
		result = usb_retimer_fw_update_query_port();
		break;
	case USB_RETIMER_FW_UPDATE_GET_MUX:
	case USB_RETIMER_FW_UPDATE_SET_USB:
	case USB_RETIMER_FW_UPDATE_SET_SAFE:
	case USB_RETIMER_FW_UPDATE_SET_TBT:
	case USB_RETIMER_FW_UPDATE_DISCONNECT:
		result = last_mux_result;
		break;
	default:
		break;
	}
	return result;
}

void usb_retimer_fw_update_process_mux_op(int port)
{
	switch (last_op) {
	case USB_RETIMER_FW_UPDATE_RESUME_PD:
		pd_set_suspend(port, 0);
		break;
	case USB_RETIMER_FW_UPDATE_GET_MUX:
		last_mux_result = usb_mux_get(port);
		break;
	case USB_RETIMER_FW_UPDATE_SET_USB:
		usb_mux_set(port, USB_PD_MUX_USB_ENABLED,
			USB_SWITCH_CONNECT, pd_get_polarity(port));
		last_mux_result = usb_mux_get(port);
		break;
	case USB_RETIMER_FW_UPDATE_SET_SAFE:
		usb_mux_set_safe_mode(port);
		last_mux_result = usb_mux_get(port);
		break;
	case USB_RETIMER_FW_UPDATE_SET_TBT:
		usb_mux_set(port, USB_PD_MUX_TBT_COMPAT_ENABLED,
			USB_SWITCH_CONNECT, pd_get_polarity(port));
		last_mux_result = usb_mux_get(port);
		break;
	case USB_RETIMER_FW_UPDATE_DISCONNECT:
		usb_mux_set(port, USB_PD_MUX_NONE,
			USB_SWITCH_DISCONNECT, pd_get_polarity(port));
		last_mux_result = usb_mux_get(port);
		break;
	default:
		break;
	}
}

void usb_retimer_fw_update_process_op(int port, int op)
{
	ASSERT(port >= 0 && port < CONFIG_USB_PD_PORT_MAX_COUNT);

	cur_port = port;
	last_op = op;

	switch (op) {
	case USB_RETIMER_FW_UPDATE_SUSPEND_PD:
		hook_call_deferred(&deferred_pd_suspend_data, 1);
		break;
	case USB_RETIMER_FW_UPDATE_QUERY_PORT:
		break;
	/* Operations can't be processed in ISR, defer to later */
	case USB_RETIMER_FW_UPDATE_RESUME_PD:
	case USB_RETIMER_FW_UPDATE_GET_MUX:
	case USB_RETIMER_FW_UPDATE_SET_USB:
	case USB_RETIMER_FW_UPDATE_SET_SAFE:
	case USB_RETIMER_FW_UPDATE_SET_TBT:
	case USB_RETIMER_FW_UPDATE_DISCONNECT:
		last_mux_result = USB_RETIMER_FW_UPDATE_INVALID_MUX;
		tc_usb_firmware_fw_update_set_flag(port);
		break;
	default:
		break;
	}
}
