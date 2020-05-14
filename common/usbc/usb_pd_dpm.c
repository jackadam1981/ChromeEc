/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Device Policy Manager implementation
 * Refer to USB PD 3.0 spec, version 2.0, sections 8.2 and 8.3
 */

#include "atomic.h"
#include "compile_time_macros.h"
#include "console.h"
#include "ec_commands.h"
#include "hooks.h"
#include "host_command.h"
#include "task.h"
#include "usb_dp_alt_mode.h"
#include "usb_pd.h"
#include "usb_pd_dpm.h"
#include "tcpm.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#else
#define CPRINTF(format, args...)
#define CPRINTS(format, args...)
#endif

static struct {
	bool mode_entry_done;
} dpm[CONFIG_USB_PD_PORT_MAX_COUNT];

void dpm_init(int port)
{
	dpm[port].mode_entry_done = false;
	CPRINTF("C%d: DPM ready for mode entry\n", port);
}

void dpm_set_mode_entry_done(int port)
{
	/*
	 * TODO: Right now, it seems appropriate for the DPM to have its own
	 * state, but it runs at the behest of and in lock-step with the PE
	 * state machine, so its state doesn't require synchronized access.  If
	 * that changes, e.g. if host commands can change the DPM's state
	 * directly, we'll need to add some kind of synchronization mechanism,
	 * perhaps as simple as the flags of the PE state machine.
	 */
	dpm[port].mode_entry_done = true;
	CPRINTF("C%d: DPM done with mode entry\n", port);
}

#ifdef CONFIG_USB_PD_HOST_CMD
static struct mutex send_svdm_deferred_params_mtx;
static struct ec_params_typec_control send_svdm_deferred_params;

static void send_svdm_deferred(void)
{
	static struct ec_host_response response;
	struct ec_params_typec_control params;
	struct host_cmd_handler_args args = {
		.command = EC_CMD_TYPEC_CONTROL,
		.version = 0,
		.params = &params,
		.params_size = sizeof(params),
		.response = &response,
		.response_size = sizeof(response),
	};

	mutex_lock(&send_svdm_deferred_params_mtx);
	params = send_svdm_deferred_params;
	mutex_unlock(&send_svdm_deferred_params_mtx);

	/*
	 * Normally, the AP would construct SVDMs and send them via the
	 * EC_CMD_TYPEC_CONTROL host command. This simulates that process, and
	 * this code will run when the AP is not taking charge of mode entry and
	 * configuration.
	 */
	host_command_process(&args);
}
DECLARE_DEFERRED(send_svdm_deferred);

void dpm_send_svdm(int port)
{
	mutex_lock(&send_svdm_deferred_params_mtx);

	send_svdm_deferred_params.cmd = TYPEC_CONTROL_SEND_VDM;
	send_svdm_deferred_params.port = port;

	mutex_unlock(&send_svdm_deferred_params_mtx);

	hook_call_deferred(&send_svdm_deferred_data, 0);
}
#endif

void dpm_attempt_mode_entry(int port)
{
	uint32_t vdo_count;
	uint32_t vdm[VDO_MAX_SIZE];

	if (!IS_ENABLED(CONFIG_USB_PD_HOST_CMD))
		return;

	if (dpm[port].mode_entry_done)
		return;

	/*
	 * TODO: Decide where and how to gate this. Only want to send host
	 * commands to myself if
	 * 1) AP has not done it and will not do it, and
	 * 2) the mode to be entered is one that the EC should handle, because
	 *    a) Config option TBD is enabled, or
	 *    b) the alt mode is DP, and the EC is in recovery mode.
	 * Right now, the same check is performed before sending and after
	 * receiving the same DPM request.
	 */
	/*
	 * Check if we even discovered a DisplayPort mode; if not, just
	 * mark discovery done and get out of here.
	 */
	if (!pd_is_mode_discovered_for_svid(port, TCPC_TX_SOP,
				USB_SID_DISPLAYPORT)) {
		CPRINTF("C%d: No DP mode discovered\n", port);
		dpm_set_mode_entry_done(port);
		return;
	}

	vdo_count = dp_setup_next_vdm(port, ARRAY_SIZE(vdm), vdm);
	if (vdo_count < 0) {
		dpm_set_mode_entry_done(port);
		CPRINTF("C%d: Couldn't set up DP VDM\n", port);
		return;
	}

	/* TODO: (maybe) Pipe this through the host command. */
	if (!pd_setup_vdm_request(port, vdm, vdo_count))
		return;

	dpm_send_svdm(port);
}
