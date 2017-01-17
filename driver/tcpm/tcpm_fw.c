/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "tcpm_fw.h"

static int (*updater[])(struct host_cmd_handler_args *) = {
	anx74xx_fw_update,
	ps8751_fw_update,
};

int tcpc_fw_update(struct host_cmd_handler_args *args)
{
	const struct ec_params_usb_tcpc_fw_update *in = args->params;
	int port = in->port;

	if ((port > 1) || (port < 0))
		return EC_ERROR_INVAL;
	return updater[port](args);
}

DECLARE_HOST_COMMAND(EC_CMD_TCPC_FW_UPDATE, tcpc_fw_update, EC_VER_MASK(0));

