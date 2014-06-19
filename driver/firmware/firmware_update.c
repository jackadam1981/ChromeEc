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
#include "firmware_update.h"

static struct ec_firmware_update_status ec_fw_status[NUM_OF_EC_FIRMWARE];

struct ec_firmware_update_status *ec_firmware_update_get_status(int fw_id)
{
	if (fw_id < NUM_OF_EC_FIRMWARE)
		return &ec_fw_status[fw_id];
	else
		return NULL;
}

void ec_firmware_update_state_init(void)
{
	int i;
	struct ec_firmware_update_status *status;
	for (i = 0; i < NUM_OF_EC_FIRMWARE; i++) {
		status = ec_firmware_update_get_status(i);
		memset((void *)status, 0, sizeof(*status));
	}
}

int ec_firmware_update_is_inprogress(void)
{
	int i;
	struct ec_firmware_update_status *status;
	for (i = 0; i < NUM_OF_EC_FIRMWARE; i++) {
		status = ec_firmware_update_get_status(i);
		if (status->hdr.state == EC_CMD_FIRMWARE_UPDATE_BEGIN ||
			status->hdr.state == EC_CMD_FIRMWARE_UPDATE_WRITE)
			return 1;
	}
	return 0;
}

static int ec_firmware_update_list(struct host_cmd_handler_args *args)
{
	int i;
	struct ec_firmware_update_list *list =
		(struct ec_firmware_update_list *)args->response;

	list->num = NUM_OF_EC_FIRMWARE;
	for (i = 0; i < list->num; i++)
		list->fw_status[i] = *ec_firmware_update_get_status(i);

	args->response_size = sizeof(struct ec_firmware_update_list) +
			list->num * sizeof(struct ec_firmware_update_entry);

	return EC_RES_SUCCESS;
}

static int ec_firmware_update_begin(struct host_cmd_handler_args *args)
{
	struct ec_firmware_update_status *begin =
		(struct ec_firmware_update_status *)args->params;

	struct ec_firmware_update_status *status =
		 ec_firmware_update_get_status(begin->hdr.fw_id);

	if (!status)
		return EC_RES_INVALID_PARAM;

	if (status->hdr.state == EC_CMD_FIRMWARE_UPDATE_PROTECT)
		return EC_RES_INVALID_COMMAND;

	/* Reset current fw status */
	*status = *begin;

	args->response_size = 0;

	return EC_RES_SUCCESS;
}

static int ec_firmware_update_end(struct host_cmd_handler_args *args)
{
	struct ec_firmware_update_status *end =
		(struct ec_firmware_update_status *)args->params;

	struct ec_firmware_update_status *status =
		 ec_firmware_update_get_status(end->hdr.fw_id);

	if (!status)
		return EC_RES_INVALID_PARAM;

	if (status->fw.hash_code != end->fw.hash_code)
		return EC_RES_INVALID_PARAM;

	args->response_size = 0;

	return EC_RES_SUCCESS;
}

static int ec_firmware_update_status(struct host_cmd_handler_args *args)
{
	struct ec_firmware_update_status *param =
		(struct ec_firmware_update_status *)args->params;

	struct ec_firmware_update_status *resp =
		(struct ec_firmware_update_status *)args->response;

	struct ec_firmware_update_status *status =
		 ec_firmware_update_get_status(param->hdr.fw_id);

	if (!status)
		return EC_RES_INVALID_PARAM;

	/* return current status of the fw_id */
	*resp = *status;

	args->response_size = sizeof(struct ec_firmware_update_status);

	return EC_RES_SUCCESS;
}

static int ec_firmware_update_protect(struct host_cmd_handler_args *args)
{
	int i, id_s, id_e;
	struct ec_firmware_update_status *param =
		(struct ec_firmware_update_status *)args->params;

	struct ec_firmware_update_status *status;

	if (param->hdr.fw_id == EC_FIRMWARE_ID_ALL) {
		id_s = 0;
		id_e = NUM_OF_EC_FIRMWARE - 1;
	} else {
		id_s = id_e = param->hdr.fw_id;
	}

	for (i = id_s; i <= id_e; i++) {

		status = ec_firmware_update_get_status(i);

		/* Upate firmware state to be protected. */
		if (status)
			status->hdr = param->hdr;
	}

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int ec_firmware_update_write(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;

	struct ec_firmware_update_write *write =
		(struct ec_firmware_update_write *)args->params;

	struct ec_firmware_update_status *status =
		 ec_firmware_update_get_status(write->hdr.fw_id);

	if (!status)
		return EC_RES_INVALID_PARAM;

	if (status->hdr.state == EC_CMD_FIRMWARE_UPDATE_PROTECT)
		return EC_RES_INVALID_COMMAND;

	/* upate state info in hdr */
	status->hdr = write->hdr;

	args->response_size = 0;

	if (write->intf != EC_CMD_FIRMWARE_UPDATE_INTERFACE_SMBUS)
		return EC_RES_INVALID_PARAM;

	if (write->size != 2)
		return EC_RES_INVALID_PARAM;

	rv = i2c_write16(I2C_PORT_BATTERY, BATTERY_ADDR,
		write->offset, write->data[0]);
	if (rv)
		rv = EC_RES_ERROR;

	return rv;
}

typedef int (*ec_firmware_update_func)(struct host_cmd_handler_args *args);

static int ec_firmware_update(struct host_cmd_handler_args *args)
{
	struct ec_firmware_update_header *hdr =
		(struct ec_firmware_update_header *)args->params;

	ec_firmware_update_func ec_firmware_update_tbl[] = {
		ec_firmware_update_list,
		ec_firmware_update_begin,
		ec_firmware_update_write,
		ec_firmware_update_end,
		ec_firmware_update_status,
		ec_firmware_update_protect
	};

	if (hdr->state < EC_CMD_FIRMWARE_UPDATE_MAX)
		return ec_firmware_update_tbl[hdr->state](args);
	else
		return EC_RES_INVALID_COMMAND;
}

DECLARE_HOST_COMMAND(EC_CMD_FIRMWARE_UPDATE,
		     ec_firmware_update,
		     EC_VER_MASK(0));

