/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "host_command.h"
#include "usb_pd.h"
#include "usb_pe_sm.h"

static enum ec_status typec_control(struct host_cmd_handler_args *args)
{
	const struct ec_params_typec_control *params = args->params;

	/* TODO: Validate port number */

	switch (params->cmd) {
	case TYPEC_CONTROL_SEND_VDM:
		/* Only the DFP can send mode entry requests. */
		if (pd_get_data_role(params->port) != PD_ROLE_DFP) {
			ccprintf("C%d: DPM requested to send SVDM, but port is "
					"not DFP\n", params->port);
			return EC_RES_ERROR;
		}
		pe_dpm_request(params->port, DPM_REQUEST_SVDM);
		break;
	default:
		return EC_RES_INVALID_PARAM;
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_TYPEC_CONTROL, typec_control, EC_VER_MASK(0));
