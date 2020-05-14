/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "ec_commands.h"
#include "hooks.h"
#include "host_command.h"
#include "shared_mem.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#else
#define CPRINTF(format, args...)
#define CPRINTS(format, args...)
#endif

static struct {
	int port;
} send_svdm_deferred_params;

static void send_svdm_deferred(void)
{
	static char response[EC_PROTO2_MAX_PARAM_SIZE];
	const int port = send_svdm_deferred_params.port;
	const struct ec_params_typec_control params = {
		.port = port,
		.cmd = TYPEC_CONTROL_SEND_VDM,
	};
	struct host_cmd_handler_args args = {
		.command = EC_CMD_TYPEC_CONTROL,
		.version = 0,
		.params = &params,
		.params_size = sizeof(params),
		.response = response,
		.response_size = sizeof(response),
	};

	CPRINTF("C%d: Sending VDM host command\n", port);
	host_command_process(&args);
}
DECLARE_DEFERRED(send_svdm_deferred);

void dpm_send_svdm(int port)
{
	send_svdm_deferred_params.port = port;
	hook_call_deferred(&send_svdm_deferred_data, 0);
}
