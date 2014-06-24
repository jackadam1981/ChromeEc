/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Smart battery Firmware Update driver.
 * Ref: Common Smart Battery System Interface Specification v8.0.
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

static struct ec_sb_fw_update_header ec_fw_hdr;
static int ec_sb_fw_i2c_access_enable;

static int ec_sb_fw_update_get_state(void)
{
	return ec_fw_hdr.state;
}

static void ec_sb_fw_update_set_state(int state)
{
	ec_fw_hdr.state = state;
}

int ec_sb_fw_update_is_inprogress(void)
{
	int state = ec_sb_fw_update_get_state();
	if (state == EC_CMD_SB_FW_UPDATE_BEGIN ||
		state == EC_CMD_SB_FW_UPDATE_WRITE)
		return 1;
	return 0;
}

int ec_sb_fw_update_is_protect(void)
{
	int state = ec_sb_fw_update_get_state();
	if (state == EC_CMD_SB_FW_UPDATE_PROTECT)
		return 1;
	return 0;
}

static int ec_sb_fw_update_begin(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	if (ec_sb_fw_update_is_protect())
		return EC_RES_INVALID_COMMAND;

	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_BEGIN);

	cprintf(CC_I2C, "firmware update i2c write cmd:%x data:%x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
	if (ec_sb_fw_i2c_access_enable)
		rv = i2c_write16(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
	if (rv)
		rv = EC_RES_ERROR;

	cprintf(CC_I2C, "firmware update i2c write cmd:%x data:%x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
	if (ec_sb_fw_i2c_access_enable)
		rv = i2c_write16(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
	if (rv)
		rv = EC_RES_ERROR;

	args->response_size = 0;

	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_end(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_END);

	cprintf(CC_I2C, "firmware update i2c write cmd:%x data:%x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_END);
	if (ec_sb_fw_i2c_access_enable)
		rv = i2c_write16(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_END);
	if (rv)
		rv = EC_RES_ERROR;

	args->response_size = 0;

	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_info(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	struct ec_sb_fw_update_info *resp =
		(struct ec_sb_fw_update_info *)args->response;

	cprintf(CC_I2C, "firmware update i2c write cmd:%x read battery info\n",
			SB_FW_UPDATE_CMD_READ_INFO);
	if (ec_sb_fw_i2c_access_enable)
		rv = i2c_read_string(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_READ_INFO,
			(uint8_t *)&resp->info,
			SB_FW_UPDATE_CMD_READ_INFO_SIZE);
	if (rv)
		rv = EC_RES_ERROR;

	args->response_size = sizeof(struct ec_sb_fw_update_info);

	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_status(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	struct ec_sb_fw_update_status *resp =
		(struct ec_sb_fw_update_status *)args->response;

	cprintf(CC_I2C, "firmware update i2c write cmd:%x read status\n",
			SB_FW_UPDATE_CMD_READ_STATUS);
	if (ec_sb_fw_i2c_access_enable)
		rv = i2c_read_string(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_READ_STATUS,
			(uint8_t *)&resp->status,
			SB_FW_UPDATE_CMD_READ_STATUS_SIZE);
	if (rv)
		rv = EC_RES_ERROR;

	args->response_size = sizeof(struct ec_sb_fw_update_status);

	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_protect(struct host_cmd_handler_args *args)
{
	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_PROTECT);
	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_write(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;

	struct ec_sb_fw_update_write *write =
		(struct ec_sb_fw_update_write *)args->params;

	if (ec_sb_fw_update_is_protect())
		return EC_RES_INVALID_COMMAND;

	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_WRITE);

	args->response_size = 0;

	if (write->size != SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE)
		return EC_RES_INVALID_PARAM;

	cprintf(CC_I2C, "firmware update i2c write off:%x\n",
		write->offset);

	write->smbus_cmd = SB_FW_UPDATE_CMD_WRITE_BLOCK;
	i2c_lock(I2C_PORT_BATTERY, 1);
	if (ec_sb_fw_i2c_access_enable)
		rv = i2c_xfer(I2C_PORT_BATTERY, BATTERY_ADDR,
			&write->smbus_cmd,
			SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE+1,
			NULL, 0, I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_BATTERY, 0);

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
		ec_sb_fw_update_info,
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

