/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
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
#include "crc.h"
#include "smbus.h"

#define DEBUG_FW_UPDATE 0

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
	return ec_sb_fw_i2c_access_enable == 1;
}

int ec_sb_fw_update_is_protect(void)
{
	int state = ec_sb_fw_update_get_state();
	if (state == EC_CMD_SB_FW_UPDATE_PROTECT) {
		cprintf(CC_I2C, "firmware update is protected.\n");
		return 1;
	}
	return (ec_sb_fw_i2c_access_enable == 0);
}

static int ec_sb_fw_update_prepare(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	args->response_size = 0;

	ec_sb_fw_i2c_access_enable = 1;

	if (ec_sb_fw_update_is_protect()) {
		cprintf(CC_I2C, "smbus cmd:%x data:%04x protect error\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
		return EC_RES_INVALID_COMMAND;
	}

	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_PREPARE);

	cprintf(CC_I2C, "smbus cmd:%x data:%04x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);

	rv = smbus_write_word(I2C_PORT_BATTERY, BATTERY_ADDR,
		SB_FW_UPDATE_CMD_WRITE_WORD,
		SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
	if (rv) {
		cprintf(CC_I2C, "smbus cmd:%x data:%04x access error\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
		return EC_RES_ERROR;
	}

	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_begin(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	args->response_size = 0;

	if (ec_sb_fw_update_is_protect()) {
		cprintf(CC_I2C, "smbus cmd:%x data:%04x protect error\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
		return EC_RES_INVALID_COMMAND;
	}

	if (!ec_sb_fw_i2c_access_enable)
		return EC_RES_ERROR;

	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_BEGIN);

#if DEBUG_FW_UPDATE
	cprintf(CC_I2C, "firmware update i2c cmd:%x data:%04x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
#endif
	rv = smbus_write_word(I2C_PORT_BATTERY, BATTERY_ADDR,
		SB_FW_UPDATE_CMD_WRITE_WORD,
		SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
	if (rv) {
		cprintf(CC_I2C, "smbus cmd:%x data:%04x access error\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
		return EC_RES_ERROR;
	}

	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_end(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_END);

	args->response_size = 0;
#if DEBUG_FW_UPDATE
	cprintf(CC_I2C, "firmware update i2c cmd:%x data:%x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_END);
#endif
	if (!ec_sb_fw_i2c_access_enable)
		return EC_RES_ERROR;

	rv = smbus_write_word(I2C_PORT_BATTERY, BATTERY_ADDR,
		SB_FW_UPDATE_CMD_WRITE_WORD,
		SB_FW_UPDATE_CMD_WRITE_WORD_END);
	if (rv) {
		cprintf(CC_I2C, "smbus cmd:%x data:%x access error\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_END);
		return EC_RES_ERROR;
	}

	return EC_RES_SUCCESS;
}

static void ec_sb_fw_update_print_info(struct ec_sb_fw_update_info *p)
{
	cprintf(CC_I2C, "\ninfo sz:%d state:0x%X fw_id:0x%X\n",
		(int) sizeof(struct ec_sb_fw_update_info),
		p->hdr.state,
		p->hdr.fw_id);
	cprintf(CC_I2C, "maker_id:0x%X hw_id:0x%X fw_ver:0x%X d_ver:0x%X\n",
		p->info.maker_id,
		p->info.hardware_id,
		p->info.fw_version,
		p->info.data_version);
	return;
}

static int ec_sb_fw_update_info(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	int len = SB_FW_UPDATE_CMD_INFO_SIZE;
	struct ec_sb_fw_update_info *resp =
		(struct ec_sb_fw_update_info *)args->response;

	cprintf(CC_I2C, "smbus cmd:%x read battery info\n",
			SB_FW_UPDATE_CMD_READ_INFO);

	args->response_size = sizeof(struct ec_sb_fw_update_info);

	if (!ec_sb_fw_i2c_access_enable) {
		cprintf(CC_I2C, "smbus cmd:%x rd info - protect error\n",
			SB_FW_UPDATE_CMD_READ_INFO);
		return EC_RES_ERROR;
	}

	rv = smbus_read_block(I2C_PORT_BATTERY, BATTERY_ADDR,
		SB_FW_UPDATE_CMD_READ_INFO, (uint8_t *) &(resp->info), len);
	if (rv) {
		cprintf(CC_I2C, "smbus cmd:%x rd info - access error\n",
			SB_FW_UPDATE_CMD_READ_INFO);
		rv = EC_RES_ERROR;
	}
	ec_sb_fw_update_print_info(resp);
	return EC_RES_SUCCESS;
}

#if DEBUG_FW_UPDATE
static void ec_sb_fw_update_print_status(struct ec_sb_fw_update_status *p)
{
	cprintf(CC_I2C, "\nstatus state:0x%X fw_id:0x%X\n",
		p->hdr.state,
		p->hdr.fw_id);
	cprintf(CC_I2C, "f_maker_id:%d f_hw_id:%d f_fw_ver:%d f_permnent:%d\n",
		p->status.v_fail_maker_id,
		p->status.v_fail_hw_id,
		p->status.v_fail_fw_version,
		p->status.v_fail_permanent);
	cprintf(CC_I2C, "permanent failure:%d abnormal:%d fw_update:%d\n",
		p->status.permanent_failure,
		p->status.abnormal_condition,
		p->status.fw_update_supported);
	cprintf(CC_I2C, "fw_update_mode:%d fw_corrupted:%d cmd_reject:%d\n",
		p->status.fw_update_mode,
		p->status.fw_corrupted,
		p->status.cmd_reject);
	cprintf(CC_I2C, "invliad data:%d fw_fatal_err:%d fec_err:%d busy:%d\n",
		p->status.invalid_data,
		p->status.fw_fatal_error,
		p->status.fec_error,
		p->status.busy);
	return;
}
#endif

static int ec_sb_fw_update_status(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	struct ec_sb_fw_update_status *resp =
		(struct ec_sb_fw_update_status *)args->response;

	ec_sb_fw_i2c_access_enable = 1;

	args->response_size = sizeof(struct ec_sb_fw_update_status);

#if DEBUG_FW_UPDATE
	cprintf(CC_I2C, "firmware update i2c cmd:%x read status16\n",
			SB_FW_UPDATE_CMD_READ_STATUS);
#endif
	rv = smbus_read_word(I2C_PORT_BATTERY, BATTERY_ADDR,
		SB_FW_UPDATE_CMD_READ_STATUS, (uint16_t *)&resp->status);
	if (rv == EC_ERROR_BUSY) {
		*((uint16_t *)&resp->status) = 0;
		resp->status.busy = 1;
		return EC_RES_SUCCESS;
	} else if (rv) {
		cprintf(CC_I2C, "i2c cmd:%x read status error:%d\n",
				SB_FW_UPDATE_CMD_READ_STATUS, rv);
		return EC_RES_ERROR;
	}

#if DEBUG_FW_UPDATE
	ec_sb_fw_update_print_status(resp);
#endif
	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_protect(struct host_cmd_handler_args *args)
{
#if 0
	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_PROTECT);
	ec_sb_fw_i2c_access_enable = 0;
#endif
	cprintf(CC_I2C, "firmware enter protect state !\n");
	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_write(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
#if DEBUG_FW_UPDATE
	int i;
#endif

	struct ec_sb_fw_update_write_block *write =
		(struct ec_sb_fw_update_write_block *)args->params;
	args->response_size = 0;

	if (ec_sb_fw_update_is_protect()) {
		cprintf(CC_I2C, "smbus write block protect error\n");
		return EC_RES_INVALID_COMMAND;
	}

	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_WRITE);

	rv = smbus_write_block(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_WRITE_BLOCK, write->data,
			SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE);
	if (rv) {
		cprintf(CC_I2C, "smbus write block access error\n");
		return EC_RES_ERROR;
	}

#if DEBUG_FW_UPDATE
	cprintf(CC_I2C, "firmware update i2c write off:%x\n",
		write->offset);
	for (i = 0; i < SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE; i++) {
		cprintf(CC_I2C, "%02X ", write->data[i]);
		if (i%16 ==  15)
			cprintf(CC_I2C, "\n");
	}
	cprintf(CC_I2C, "\n");
#endif

	return rv;
}

typedef int (*ec_sb_fw_update_func)(struct host_cmd_handler_args *args);

static int ec_sb_fw_update(struct host_cmd_handler_args *args)
{
	struct ec_sb_fw_update_header *hdr =
		(struct ec_sb_fw_update_header *)args->params;

	ec_sb_fw_update_func ec_sb_fw_update_tbl[] = {
		ec_sb_fw_update_prepare,
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

