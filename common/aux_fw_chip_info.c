/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* AUX FW chip configuration and host command handler */

#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "host_command.h"
#include "i2c.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

extern const struct aux_fw_chip_info board_aux_fw_chip_info[];
extern const unsigned int num_aux_fw_chip;

int host_command_get_aux_fw_chip_list(struct host_cmd_handler_args *args)
{
	struct ec_response_aux_fw_chip_info *r = args->response;
	const struct aux_fw_chip_info *chip_info = board_aux_fw_chip_info;
	int i;

	if (args->response_max < num_aux_fw_chip * sizeof(*chip_info)) {
		CPRINTS("%s Response size too large\n", __func__);
		r->num_aux_fw_chip = args->response_max / sizeof(*chip_info);
	} else {
		r->num_aux_fw_chip = num_aux_fw_chip;
	}

	memcpy(r->chip_info, board_aux_fw_chip_info,
				r->num_aux_fw_chip * sizeof(*chip_info));
	for (i = 0; i < r->num_aux_fw_chip; i++) {
#if defined(CONFIG_I2C_MASTER)
		r->chip_info[i].port_protect_status =
			i2c_passthru_protect_status(chip_info[i].port);
#endif
	}

	args->response_size = sizeof(*r) +
		r->num_aux_fw_chip * sizeof(*chip_info);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_AUX_FW_CHIP_LIST,
		     host_command_get_aux_fw_chip_list,
		     EC_VER_MASK(0));
