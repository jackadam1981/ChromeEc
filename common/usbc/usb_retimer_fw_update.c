/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdbool.h>
#include <stdint.h>
#include "atomic.h"
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

/*
 * Update retimer firmware of no device attached (NDA) ports
 *
 * https://docs.kernel.org/admin-guide/thunderbolt.html#
 * upgrading-on-board-retimer-nvm-when-there-is-no-cable-connected
 *
 * On EC side:
 * Retimer firmware update is initiated by AP.
 * The operations requested by AP are:
 * 0 - USB_RETIMER_FW_UPDATE_QUERY_PORT
 * 1 - USB_RETIMER_FW_UPDATE_SUSPEND_PD
 * 2 - USB_RETIMER_FW_UPDATE_RESUME_PD
 * 3 - USB_RETIMER_FW_UPDATE_GET_MUX
 * 4 - USB_RETIMER_FW_UPDATE_SET_USB
 * 5 - USB_RETIMER_FW_UPDATE_SET_SAFE
 * 6 - USB_RETIMER_FW_UPDATE_SET_TBT
 * 7 - USB_RETIMER_FW_UPDATE_DISCONNECT
 *
 * Operation 0 is processed immediately.
 * Operations 1 to 7 are deferred and processed inside tc_run().
 * Operations 1/2/3 can be processed any time; while 4/5/6/7 have
 * to be processed when PD task is suspended.
 * Two TC flags are created for this situation.
 * If Op 1/2/3 is received, TC_FLAGS_USB_RETIMER_FW_UPDATE_RUN
 * is set, PD task will be waken up and process it.
 * If 4/5/6/7 is received, TC_FLAGS_USB_RETIMER_FW_UPDATE_LTD_RUN is
 * set, PD task should be in suspended mode and process it.
 *
 * On host side:
 * 1. Put USB4 ports into offline mode.
 *    This forces retimer to power on, then requests EC to suspend
 *    PD port, set USB mux to USB, Safe then TBT.
 * 2. Scan for retimers
 * 3. Update retimer NVM firmware.
 * 4. Authenticate.
 * 5. Wait 5 or more seconds for retimer to come back.
 * 6. Put USB4 ports into online mode, the functional state.
 *    This requestes EC to disconnect(set USB mux to 0), resume PD port.
 *
 */

#define SUSPEND 1
#define RESUME  0
/*
 * Two seconds buffer is added on top of required 5 seconds;
 * to cover the time to disconnect and resume.
 */
#define RETIMTER_ONLINE_DELAY (7 * SECOND)
#define RETIMTER_FW_UPDATE_TIMEOUT (20 * MINUTE)

/* Track current port AP requested to update retimer firmware */
static int cur_port;
static int last_op; /* Operation received from AP via ACPI_WRITE */
/* Operation result returned to ACPI_READ */
static int last_result;
/* Ports to be put online */
static atomic_t ports_online_requested;
/*
 * Track port state: SUSPEND or RESUME
 * Each bit stores the state of one port.
 * bit 0 is the state of port 0;
 * ...
 * bit n is the state of port n.
 */
static atomic_t port_state;

int usb_retimer_fw_update_get_result(void)
{
	int result = 0;

	switch (last_op) {
	case USB_RETIMER_FW_UPDATE_SUSPEND_PD:
		if (last_result == USB_RETIMER_FW_UPDATE_ERR) {
			result = last_result;
			break;
		}
		/* fall through */
	case USB_RETIMER_FW_UPDATE_RESUME_PD:
		result = pd_is_port_enabled(cur_port);
		break;
	case USB_RETIMER_FW_UPDATE_QUERY_PORT:
		result = usb_mux_retimer_fw_update_port_info();
		break;
	case USB_RETIMER_FW_UPDATE_GET_MUX:
	case USB_RETIMER_FW_UPDATE_SET_USB:
	case USB_RETIMER_FW_UPDATE_SET_SAFE:
	case USB_RETIMER_FW_UPDATE_SET_TBT:
	case USB_RETIMER_FW_UPDATE_DISCONNECT:
		result = last_result;
		break;
	default:
		break;
	}

	return result;
}

static void retimer_fw_update_set_port_state(int port, int state)
{
	if (state)
		atomic_or(&port_state, BIT(port));
	else
		atomic_clear_bits(&port_state, BIT(port));
}

static int retimer_fw_update_get_port_state(int port)
{
	return !!(port_state & BIT(port));
}

/**
 * @brief Suspend or resume PD task and update the state of the port.
 *
 * @param port PD port
 * @param state
 * SUSPEND: suspend PD task for firmware update; and set state to SUSPEND
 * RESUME: resume PD task after firmware update is done; and set state
 * to RESUME.
 *
 */
static void retimer_fw_update_port_handler(int port, int state)
{
	pd_set_suspend(port, state);
	retimer_fw_update_set_port_state(port, state);
	if (state == RESUME)
		atomic_clear_bits(&ports_online_requested, BIT(port));
}

static void deferred_pd_suspend(void)
{
	retimer_fw_update_port_handler(cur_port, SUSPEND);
}
DECLARE_DEFERRED(deferred_pd_suspend);

static inline mux_state_t retimer_fw_update_usb_mux_get(int port)
{
	return usb_mux_get(port) & USB_RETIMER_FW_UPDATE_MUX_MASK;
}

static void retry_online(int port)
{
	usb_mux_set(port, USB_PD_MUX_NONE,
		USB_SWITCH_DISCONNECT, pd_get_polarity(port));
	if (!usb_mux_set_completed(port))
		msleep(25);
	CPRINTS("Retry online: mux 0x%x",
		retimer_fw_update_usb_mux_get(port));
	retimer_fw_update_port_handler(port, RESUME);
}

/*
 * After NVM update, if AP skips step 5, not wait 5+ seconds for retimer
 * to come back; then do step 6 immediately, requesting EC to put
 * retimer online. Step 6 will fail; port is still offline afterwards.
 *
 * This deferred function monitors if any port has this problem and retry
 * online one more time.
 */
static void retimer_check_online(void);
DECLARE_DEFERRED(retimer_check_online);

static void retimer_check_online(void)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
		if (ports_online_requested & BIT(i)) {
			/*
			 * Now the time has passed RETIMTER_ONLINE_DELAY;
			 * retry online.
			 * The port is suspended; if the port is not
			 * suspended, DISCONNECT request won't go through,
			 * we couldn't be here.
			 */
			retry_online(i);
			/* PD port is resumed */
		}
	}
}

/* Allow mux results to be filled in during HOOKS if needed */
static void last_result_mux_get(void);
DECLARE_DEFERRED(last_result_mux_get);

static void last_result_mux_get(void)
{
	if (!usb_mux_set_completed(cur_port)) {
		hook_call_deferred(&last_result_mux_get_data, 20 * MSEC);
		return;
	}

	last_result = retimer_fw_update_usb_mux_get(cur_port);
}

static void restore_port(void)
{
	int port;

	if (port_state == 0)
		return;

	for  (port = 0; port < CONFIG_USB_PD_PORT_MAX_COUNT; port++) {
		if (retimer_fw_update_get_port_state(port))
			retimer_fw_update_port_handler(port, RESUME);
	}
}

static void retimer_fw_update_timeout_handler(void);
DECLARE_DEFERRED(retimer_fw_update_timeout_handler);

static void retimer_fw_update_timeout_handler(void)
{
	CPRINTS("%s: port_state(0x%x)", __func__, (int)port_state);
	restore_port();
}

void usb_retimer_fw_update_process_op_cb(int port)
{
	bool result_mux_get = false;

	if (port != cur_port) {
		CPRINTS("Unexpected FW op: port %d, cur %d", port, cur_port);
		return;
	}

	switch (last_op) {
	case USB_RETIMER_FW_UPDATE_SUSPEND_PD:
		last_result = 0;
		/*
		 * Do not perform retimer firmware update process
		 * if battery is not present, or battery level is low.
		 */
		if (!pd_firmware_upgrade_check_power_readiness(port)) {
			last_result = USB_RETIMER_FW_UPDATE_ERR;
			break;
		}

		/*
		 * If the port has entered low power mode, the PD task
		 * is paused and will not complete processing of
		 * pd_set_suspend(). Move pd_set_suspend() into a deferred
		 * call so that it runs from the HOOKS task and can generate
		 * a wake event to the PD task and enter suspended mode.
		 */
		hook_call_deferred(&deferred_pd_suspend_data, 0);
		hook_call_deferred(&retimer_fw_update_timeout_handler_data,
			RETIMTER_FW_UPDATE_TIMEOUT);
		break;
	case USB_RETIMER_FW_UPDATE_RESUME_PD:
		retimer_fw_update_port_handler(port, RESUME);
		break;
	case USB_RETIMER_FW_UPDATE_GET_MUX:
		result_mux_get = true;
		break;
	case USB_RETIMER_FW_UPDATE_SET_USB:
		usb_mux_set(port, USB_PD_MUX_USB_ENABLED,
			USB_SWITCH_CONNECT, pd_get_polarity(port));
		result_mux_get = true;
		break;
	case USB_RETIMER_FW_UPDATE_SET_SAFE:
		usb_mux_set_safe_mode(port);
		result_mux_get = true;
		break;
	case USB_RETIMER_FW_UPDATE_SET_TBT:
		usb_mux_set(port, USB_PD_MUX_TBT_COMPAT_ENABLED,
			USB_SWITCH_CONNECT, pd_get_polarity(port));
		result_mux_get = true;
		break;
	case USB_RETIMER_FW_UPDATE_DISCONNECT:
		usb_mux_set(port, USB_PD_MUX_NONE,
			USB_SWITCH_DISCONNECT, pd_get_polarity(port));
		result_mux_get = true;
		atomic_or(&ports_online_requested, BIT(port));
		hook_call_deferred(&retimer_check_online_data,
			RETIMTER_ONLINE_DELAY);
		break;
	default:
		break;
	}

	/*
	 * Fill in our mux result if available, or set up a deferred retrieval
	 * if the set is still pending.
	 */
	if (result_mux_get)
		last_result_mux_get();
}

void usb_retimer_fw_update_process_op(int port, int op)
{
	ASSERT(port >= 0 && port < CONFIG_USB_PD_PORT_MAX_COUNT);

	/*
	 * TODO(b/179220036): check not overlapping requests;
	 * not change cur_port if retimer scan is in progress
	 */
	last_op = op;
	cur_port = port;

	switch (op) {
	case USB_RETIMER_FW_UPDATE_QUERY_PORT:
		break;
	/* Operations can't be processed in ISR, defer to later */
	case USB_RETIMER_FW_UPDATE_GET_MUX:
		last_result = USB_RETIMER_FW_UPDATE_INVALID_MUX;
		tc_usb_firmware_fw_update_run(port);
		break;
	case USB_RETIMER_FW_UPDATE_SUSPEND_PD:
	case USB_RETIMER_FW_UPDATE_RESUME_PD:
		tc_usb_firmware_fw_update_run(port);
		break;
	case USB_RETIMER_FW_UPDATE_SET_USB:
	case USB_RETIMER_FW_UPDATE_SET_SAFE:
	case USB_RETIMER_FW_UPDATE_SET_TBT:
	case USB_RETIMER_FW_UPDATE_DISCONNECT:
		if (pd_is_port_enabled(port)) {
			last_result = USB_RETIMER_FW_UPDATE_ERR;
		} else {
			last_result = USB_RETIMER_FW_UPDATE_INVALID_MUX;
			tc_usb_firmware_fw_update_limited_run(port);
		}
		break;
	default:
		break;
	}
}

/*
 * If due to any reason system shuts down during firmware update, resume
 * the PD port; otherwise, PD port is suspended even system powers up again.
 * In normal case, system should not allow shutdown during firmware update.
 */
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, restore_port, HOOK_PRIO_DEFAULT);
