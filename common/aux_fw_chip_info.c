/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* AUX FW chip configuration and host command handler */

#include "aux_fw_chip_info.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "host_command.h"
#include "i2c.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

__attribute__((weak))
int board_get_aux_fw_chip_list(const struct aux_fw_chip_info **chip_info)
{
	return -EC_ERROR_UNIMPLEMENTED;
}

int host_command_get_aux_fw_chip_list(struct host_cmd_handler_args *args)
{
	const struct ec_params_aux_fw_chip_info *p = args->params;
	struct ec_response_aux_fw_chip_info *r = args->response;
	const struct aux_fw_chip_info *chip_info;
	int i, ret;

	ret = board_get_aux_fw_chip_list(&chip_info);
	if (ret < 0) {
		CPRINTS("Error %d getting aux fw chip list", ret);
		return EC_RES_ERROR;
	}

	r->num_aux_fw_chip = MIN(p->num_aux_fw_chip, ret);
	for (i = 0; i < r->num_aux_fw_chip; i++) {
		strncpy(r->chip_info[i].chip_string, chip_info[i].chip_string,
					sizeof(chip_info[i].chip_string));
		r->chip_info[i].i2c_bus = chip_info[i].i2c_bus;
		r->chip_info[i].port = chip_info[i].port;
#if defined(CONFIG_I2C_MASTER)
		r->chip_info[i].port_protect_status =
			i2c_passthru_protect_status(chip_info[i].port);
#endif
	}

	args->response_size = sizeof(*r) +
		p->num_aux_fw_chip * sizeof(*chip_info);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_AUX_FW_CHIP_LIST,
		     host_command_get_aux_fw_chip_list,
		     EC_VER_MASK(0));
