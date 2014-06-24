/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Smart battery driver.
 */

#include "battery.h"
#include "battery_smart.h"
#include "host_command.h"
#include "i2c.h"
#include "timer.h"
#include "util.h"
#include "ec_commands.h"
#include "sb_fw_update.h"
#include "console.h"

static struct ec_sb_fw_update_status ec_fw_status;

struct ec_sb_fw_update_status *ec_sb_fw_update_get_status(void)
{
	return &ec_fw_status;
}

int ec_sb_fw_update_is_inprogress(void)
{
	struct ec_sb_fw_update_status *status;
	status = ec_sb_fw_update_get_status();
	if (status->hdr.state == EC_CMD_SB_FW_UPDATE_BEGIN ||
		status->hdr.state == EC_CMD_SB_FW_UPDATE_WRITE)
		return 1;
	return 0;
}

int ec_sb_fw_update_is_protect(void)
{
	struct ec_sb_fw_update_status *status;
	status = ec_sb_fw_update_get_status();
	if (status->hdr.state == EC_CMD_SB_FW_UPDATE_PROTECT)
		return 1;
	return 0;
}

static int ec_sb_fw_update_begin(struct host_cmd_handler_args *args)
{
	struct ec_sb_fw_update_status *begin =
		(struct ec_sb_fw_update_status *)args->params;

	struct ec_sb_fw_update_status *status =
		 ec_sb_fw_update_get_status();

	if (!status)
		return EC_RES_INVALID_PARAM;

	if (ec_sb_fw_update_is_protect())
		return EC_RES_INVALID_COMMAND;

	/* Reset current fw status */
	*status = *begin;

	args->response_size = 0;

	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_end(struct host_cmd_handler_args *args)
{
	struct ec_sb_fw_update_status *end =
		(struct ec_sb_fw_update_status *)args->params;

	struct ec_sb_fw_update_status *status =
		 ec_sb_fw_update_get_status();

	if (!status)
		return EC_RES_INVALID_PARAM;

	if (status->fw.hash_code != end->fw.hash_code)
		return EC_RES_INVALID_PARAM;

	/* upate state info in hdr */
	status->hdr = end->hdr;

	args->response_size = 0;

	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_status(struct host_cmd_handler_args *args)
{
	struct ec_sb_fw_update_status *resp =
		(struct ec_sb_fw_update_status *)args->response;

	struct ec_sb_fw_update_status *status =
		 ec_sb_fw_update_get_status();

	if (!status)
		return EC_RES_INVALID_PARAM;

	/* return current firmware update status */
	*resp = *status;

	args->response_size = sizeof(struct ec_sb_fw_update_status);

	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_protect(struct host_cmd_handler_args *args)
{
	struct ec_sb_fw_update_status *param =
		(struct ec_sb_fw_update_status *)args->params;

	struct ec_sb_fw_update_status *status;

	status = ec_sb_fw_update_get_status();

	/* Upate firmware state to be protected. */
	if (status)
		status->hdr = param->hdr;

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_write(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;

	struct ec_sb_fw_update_write *write =
		(struct ec_sb_fw_update_write *)args->params;

	struct ec_sb_fw_update_status *status =
		 ec_sb_fw_update_get_status();

	if (!status)
		return EC_RES_INVALID_PARAM;

	if (ec_sb_fw_update_is_protect())
		return EC_RES_INVALID_COMMAND;

	/* upate state info in hdr */
	status->hdr = write->hdr;

	args->response_size = 0;

	if (write->size != 2)
		return EC_RES_INVALID_PARAM;

	cprintf(CC_I2C, "firmware update i2c write off:%x data:%x\n",
		write->offset, write->data[0]);
	if (0)
		rv = i2c_write16(I2C_PORT_BATTERY, BATTERY_ADDR,
			write->offset, write->data[0]);
	if (rv)
		rv = EC_RES_ERROR;

	return rv;
}

typedef int (*ec_sb_fw_update_func)(struct host_cmd_handler_args *args);

static int ec_sb_fw_update(struct host_cmd_handler_args *args)
{
	struct ec_sb_fw_update_header *hdr =
		(struct ec_sb_fw_update_header *)args->params;

	ec_sb_fw_update_func ec_sb_fw_update_tbl[] = {
		ec_sb_fw_update_begin,
		ec_sb_fw_update_write,
		ec_sb_fw_update_end,
		ec_sb_fw_update_status,
		ec_sb_fw_update_protect
	};

	if (hdr->state < EC_CMD_SB_FW_UPDATE_MAX)
		return ec_sb_fw_update_tbl[hdr->state](args);
	else
		return EC_RES_INVALID_COMMAND;
}

DECLARE_HOST_COMMAND(EC_CMD_SB_FW_UPDATE,
		     ec_sb_fw_update,
		     EC_VER_MASK(0));

