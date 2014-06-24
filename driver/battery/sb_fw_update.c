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
	if (state == EC_CMD_SB_FW_UPDATE_PROTECT) {
		cprintf(CC_I2C, "firmware update is protected.\n");
		return ec_sb_fw_i2c_access_enable;
	}
	return 0;
}

static int ec_sb_fw_update_begin(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;

	if (ec_sb_fw_update_is_protect())
		return EC_RES_INVALID_COMMAND;

	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_BEGIN);

	cprintf(CC_I2C, "firmware update i2c cmd:%x data:%04x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
	if (ec_sb_fw_i2c_access_enable)
		rv = i2c_write16(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
	if (rv)
		rv = EC_RES_ERROR;

	cprintf(CC_I2C, "firmware update i2c cmd:%x data:%04x\n",
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

	cprintf(CC_I2C, "firmware update i2c cmd:%x data:%x\n",
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

static void ec_sb_fw_update_print_info(struct ec_sb_fw_update_info *p)
{
	cprintf(CC_I2C, "\ninfo state:0x%X fw_id:0x%X\n",
		p->hdr.state,
		p->hdr.fw_id);
	cprintf(CC_I2C, "maker_id:0x%X hw_id:0x%X fw_ver:0x%X d_ver:0x%X\n",
		p->info.maker_id,
		p->info.hardware_id,
		p->info.fw_version,
		p->info.data_version);
}

static int ec_sb_fw_update_info(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	struct ec_sb_fw_update_info *resp =
		(struct ec_sb_fw_update_info *)args->response;

	ec_sb_fw_i2c_access_enable = 1;

	cprintf(CC_I2C, "firmware update i2c cmd:%x read battery info\n",
			SB_FW_UPDATE_CMD_READ_INFO);

	if (ec_sb_fw_i2c_access_enable)
		rv = i2c_read_string(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_READ_INFO,
			(uint8_t *)&resp->info,
			SB_FW_UPDATE_CMD_READ_INFO_SIZE);
	if (rv)
		rv = EC_RES_ERROR;

	ec_sb_fw_update_print_info(resp);
	args->response_size = sizeof(struct ec_sb_fw_update_info);

	return EC_RES_SUCCESS;
}

static void ec_sb_fw_update_print_status(struct ec_sb_fw_update_status *p)
{
	cprintf(CC_I2C, "\nstatus state:0x%X fw_id:0x%X\n",
		p->hdr.state,
		p->hdr.fw_id);
	cprintf(CC_I2C, "maker_id:0x%X hw_id:0x%X fw_ver:0x%X permnent:0x%X\n",
		p->status.v_fail_maker_id,
		p->status.v_fail_hw_id,
		p->status.v_fail_fw_version,
		p->status.v_fail_permanent);
	cprintf(CC_I2C, "permanent failure:%X abnormal:%X fw_update:%X\n",
		p->status.permanent_failure,
		p->status.abnormal_condition,
		p->status.fw_update_supported);
	cprintf(CC_I2C, "fw_update_mode:%X fw_corrupted:%X cmd_reject:%X\n",
		p->status.fw_update_mode,
		p->status.fw_corrupted,
		p->status.cmd_reject);
	cprintf(CC_I2C, "invliad data:%X fw_fatal_err:%X fec_err:%X busy:%X\n",
		p->status.invalid_data,
		p->status.fw_fatal_error,
		p->status.fec_error,
		p->status.busy);
}

static int ec_sb_fw_update_status(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	struct ec_sb_fw_update_status *resp =
		(struct ec_sb_fw_update_status *)args->response;

	cprintf(CC_I2C, "firmware update i2c cmd:%x read status\n",
			SB_FW_UPDATE_CMD_READ_STATUS);
	if (ec_sb_fw_i2c_access_enable)
		rv = i2c_read_string(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_READ_STATUS,
			(uint8_t *)&resp->status,
			SB_FW_UPDATE_CMD_READ_STATUS_SIZE);
	if (rv)
		rv = EC_RES_ERROR;

	ec_sb_fw_update_print_status(resp);
	args->response_size = sizeof(struct ec_sb_fw_update_status);

	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_protect(struct host_cmd_handler_args *args)
{
	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_PROTECT);
	ec_sb_fw_i2c_access_enable = 0;
	cprintf(CC_I2C, "firmware enter protect state\n");
	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_write(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	int i;

	struct ec_sb_fw_update_write *write =
		(struct ec_sb_fw_update_write *)args->params;

	if (ec_sb_fw_update_is_protect())
		return EC_RES_INVALID_COMMAND;

	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_WRITE);

	args->response_size = 0;

	if (write->size != SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE) {
		cprintf(CC_I2C, "firmware update i2c write size:%x error.\n",
				write->size);
		return EC_RES_INVALID_PARAM;
	}

	cprintf(CC_I2C, "firmware update i2c write off:%x\n",
		write->offset);
	for (i = 0; i < SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE; i++)
		cprintf(CC_I2C, "%02X ", write->data[i]);
	cprintf(CC_I2C, "\n");

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

