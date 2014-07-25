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
#include "crc8.h"
#include "smbus.h"

#define CPRINTF(fmt, args...) cprintf(CC_I2C, fmt, ## args)

static struct sb_fw_update_header sb_fw_hdr;

static int i2c_access_enable;

static int get_state(void)
{
	return sb_fw_hdr.state;
}

static void set_state(int state)
{
	sb_fw_hdr.state = state;
}

int sb_fw_update_in_progress(void)
{
	return i2c_access_enable;
}

int sb_fw_update_is_protected(void)
{
	int state = get_state();
	if (state == EC_CMD_SB_FW_UPDATE_PROTECT) {
		CPRINTF("firmware update is protected.\n");
		return 1;
	}
	return !i2c_access_enable;
}

static int prepare_update(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	args->response_size = 0;

	if (sb_fw_update_is_protected()) {
		CPRINTF("smbus cmd:%x data:%04x protect error\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
		return EC_RES_INVALID_COMMAND;
	}

	set_state(EC_CMD_SB_FW_UPDATE_PREPARE);

	CPRINTF("smbus cmd:%x data:%04x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);

	rv = smbus_write_word(I2C_PORT_BATTERY, BATTERY_ADDR,
		SB_FW_UPDATE_CMD_WRITE_WORD,
		SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
	if (rv) {
		CPRINTF("smbus cmd:%x data:%04x access error\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
		return EC_RES_ERROR;
	}

	return EC_RES_SUCCESS;
}

static int begin_update(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	args->response_size = 0;

	if (sb_fw_update_is_protected()) {
		CPRINTF("smbus cmd:%x data:%04x protect error\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
		return EC_RES_INVALID_COMMAND;
	}

	if (!i2c_access_enable)
		return EC_RES_ERROR;

	set_state(EC_CMD_SB_FW_UPDATE_BEGIN);

#ifdef CONFIG_SMBUS_DEBUG_FW_UPDATE
	CPRINTF("firmware update i2c cmd:%x data:%04x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
#endif
	rv = smbus_write_word(I2C_PORT_BATTERY, BATTERY_ADDR,
		SB_FW_UPDATE_CMD_WRITE_WORD,
		SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
	if (rv) {
		CPRINTF("smbus cmd:%x data:%04x access error\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
		return EC_RES_ERROR;
	}

	return EC_RES_SUCCESS;
}

static int end_update(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	set_state(EC_CMD_SB_FW_UPDATE_END);

	args->response_size = 0;
#ifdef CONFIG_SMBUS_DEBUG_FW_UPDATE
	CPRINTF("firmware update i2c cmd:%x data:%x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_END);
#endif
	if (!i2c_access_enable)
		return EC_RES_ERROR;

	rv = smbus_write_word(I2C_PORT_BATTERY, BATTERY_ADDR,
		SB_FW_UPDATE_CMD_WRITE_WORD,
		SB_FW_UPDATE_CMD_WRITE_WORD_END);
	if (rv) {
		CPRINTF("smbus cmd:%x data:%x access error\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_END);
		return EC_RES_ERROR;
	}

	return EC_RES_SUCCESS;
}

static void print_info(struct ec_sb_fw_update_info *p)
{
	CPRINTF("\ninfo sz:%d state:0x%X fw_id:0x%X\n",
		(int) sizeof(struct ec_sb_fw_update_info),
		p->hdr.state,
		p->hdr.fw_id);
	CPRINTF("maker_id:0x%X hw_id:0x%X fw_ver:0x%X d_ver:0x%X\n",
		p->info.maker_id,
		p->info.hardware_id,
		p->info.fw_version,
		p->info.data_version);
	return;
}

static int get_info(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	uint8_t len = SB_FW_UPDATE_CMD_INFO_SIZE;
	struct ec_sb_fw_update_info *resp =
		(struct ec_sb_fw_update_info *)args->response;

	CPRINTF("smbus cmd:%x read battery info\n",
			SB_FW_UPDATE_CMD_READ_INFO);

	args->response_size = sizeof(struct ec_sb_fw_update_info);

	if (!i2c_access_enable) {
		CPRINTF("smbus cmd:%x rd info - protect error\n",
			SB_FW_UPDATE_CMD_READ_INFO);
		return EC_RES_ERROR;
	}

	rv = smbus_read_block(I2C_PORT_BATTERY, BATTERY_ADDR,
		SB_FW_UPDATE_CMD_READ_INFO, (uint8_t *) &(resp->info), &len);
	if (rv) {
		CPRINTF("smbus cmd:%x rd info - access error\n",
			SB_FW_UPDATE_CMD_READ_INFO);
		rv = EC_RES_ERROR;
	}
	print_info(resp);
	return EC_RES_SUCCESS;
}

#ifdef CONFIG_SMBUS_DEBUG_FW_UPDATE
static void print_status(struct ec_sb_fw_update_status *p)
{
	CPRINTF("\nstatus state:0x%X fw_id:0x%X\n",
		p->hdr.state,
		p->hdr.fw_id);
	CPRINTF("f_maker_id:%d f_hw_id:%d f_fw_ver:%d f_permnent:%d\n",
		p->status.v_fail_maker_id,
		p->status.v_fail_hw_id,
		p->status.v_fail_fw_version,
		p->status.v_fail_permanent);
	CPRINTF("permanent failure:%d abnormal:%d fw_update:%d\n",
		p->status.permanent_failure,
		p->status.abnormal_condition,
		p->status.fw_update_supported);
	CPRINTF("fw_update_mode:%d fw_corrupted:%d cmd_reject:%d\n",
		p->status.fw_update_mode,
		p->status.fw_corrupted,
		p->status.cmd_reject);
	CPRINTF("invliad data:%d fw_fatal_err:%d fec_err:%d busy:%d\n",
		p->status.invalid_data,
		p->status.fw_fatal_error,
		p->status.fec_error,
		p->status.busy);
	return;
}
#endif

static int get_status(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	struct ec_sb_fw_update_status *resp =
		(struct ec_sb_fw_update_status *)args->response;

	/* Enable smart battery i2c access */
	i2c_access_enable = 1;

	args->response_size = sizeof(struct ec_sb_fw_update_status);

#ifdef CONFIG_SMBUS_DEBUG_FW_UPDATE
	CPRINTF("firmware update i2c cmd:%x read status16\n",
			SB_FW_UPDATE_CMD_READ_STATUS);
#endif
	rv = smbus_read_word(I2C_PORT_BATTERY, BATTERY_ADDR,
		SB_FW_UPDATE_CMD_READ_STATUS, (uint16_t *)&resp->status);
	if (rv == EC_ERROR_BUSY) {
		*((uint16_t *)&resp->status) = 0;
		resp->status.busy = 1;
		return EC_RES_SUCCESS;
	} else if (rv) {
		CPRINTF("i2c cmd:%x read status error:0x%X\n",
				SB_FW_UPDATE_CMD_READ_STATUS, rv);
		return EC_RES_ERROR;
	}

#ifdef CONFIG_SMBUS_DEBUG_FW_UPDATE
	print_status(resp);
#endif
	return EC_RES_SUCCESS;
}

static int set_protect(struct host_cmd_handler_args *args)
{
	set_state(EC_CMD_SB_FW_UPDATE_PROTECT);
	i2c_access_enable = 0;
	CPRINTF("firmware enter protect state !\n");
	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int write_block(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
#ifdef CONFIG_SMBUS_DEBUG_FW_UPDATE
	int i;
#endif

	struct ec_sb_fw_update_write_block *write =
		(struct ec_sb_fw_update_write_block *)args->params;
	args->response_size = 0;

	if (sb_fw_update_is_protected()) {
		CPRINTF("smbus write block protect error\n");
		return EC_RES_INVALID_COMMAND;
	}

	set_state(EC_CMD_SB_FW_UPDATE_WRITE);

	rv = smbus_write_block(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_WRITE_BLOCK, write->data,
			SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE);
	if (rv) {
		CPRINTF("smbus write block access error\n");
		return EC_RES_ERROR;
	}

#ifdef CONFIG_SMBUS_DEBUG_FW_UPDATE
	CPRINTF("firmware update i2c write off:%x\n",
		write->offset);
	for (i = 0; i < SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE; i++) {
		CPRINTF("%02X ", write->data[i]);
		if (i%16 ==  15)
			CPRINTF("\n");
	}
	CPRINTF("\n");
#endif

	return rv;
}

typedef int (*sb_fw_update_func)(struct host_cmd_handler_args *args);

static int sb_fw_update(struct host_cmd_handler_args *args)
{
	struct sb_fw_update_header *hdr =
		(struct sb_fw_update_header *)args->params;

	sb_fw_update_func sb_fw_update_tbl[] = {
		prepare_update,
		get_info,
		begin_update,
		write_block,
		end_update,
		get_status,
		set_protect
	};

	if (hdr->state < EC_CMD_SB_FW_UPDATE_MAX)
		return sb_fw_update_tbl[hdr->state](args);
	else
		return EC_RES_INVALID_PARAM;
}

DECLARE_HOST_COMMAND(EC_CMD_SB_FW_UPDATE,
		     sb_fw_update,
		     EC_VER_MASK(0));

