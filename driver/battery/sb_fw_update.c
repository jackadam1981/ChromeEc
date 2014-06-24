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

/* Return CRC-8 of the data, using x^8 + x^2 + x + 1 polynomial.  A
 * table-based algorithm would be faster, but for only 15 bytes isn't
 * worth the code size.
 * Copy from coreboot/src/vendorcode/google/chromeos/vbnv_ec.c
 */
static uint8_t crc8(const uint8_t *data, int len)
{
	unsigned crc = 0;
	int i, j;

	for (j = len; j; j--, data++) {
		crc ^= (*data << 8);
		for (i = 8; i; i--) {
			if (crc & 0x8000)
				crc ^= (0x1070 << 3);
			crc <<= 1;
		}
	}

	return (uint8_t) (crc >> 8);
}

/* smbus Write Word
 *   [S][Slave Address][Wr][A][cmd][A][DL][A][DH][A][PEC][A][P]
 */
int smbus_write16(int port, int slave_addr, int offset, int data)
{
	int rv;
	uint8_t buf[4];

	buf[0] = offset & 0xff;

	if (slave_addr & I2C_FLAG_BIG_ENDIAN) {
		buf[1] = (data >> 8) & 0xff;
		buf[2] = data & 0xff;
	} else {
		buf[1] = data & 0xff;
		buf[2] = (data >> 8) & 0xff;
	}
	buf[3] = crc8(buf, 3);

	i2c_lock(port, 1);
	rv = i2c_xfer(port, slave_addr, buf, 4, NULL, 0, I2C_XFER_SINGLE);
	i2c_lock(port, 0);

	return rv;
}

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
		return 1;
	}
	return (ec_sb_fw_i2c_access_enable == 0);
}

static int ec_sb_fw_update_begin(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	args->response_size = 0;
	if (ec_sb_fw_update_is_protect())
		return EC_RES_INVALID_COMMAND;

	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_BEGIN);

	cprintf(CC_I2C, "firmware update i2c cmd:%x data:%04x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
	if (ec_sb_fw_i2c_access_enable) {
		rv = i2c_write16(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
		if (rv)
			return EC_RES_ERROR;
	}

	cprintf(CC_I2C, "firmware update i2c cmd:%x data:%04x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
	if (ec_sb_fw_i2c_access_enable) {
		rv = i2c_write16(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
		if (rv)
			return EC_RES_ERROR;
	}

	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_end(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_END);

	args->response_size = 0;
	cprintf(CC_I2C, "firmware update i2c cmd:%x data:%x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_END);
	if (ec_sb_fw_i2c_access_enable) {
		rv = i2c_write16(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_END);
		if (rv)
			return EC_RES_ERROR;
	}

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
	return;
}

static int ec_sb_fw_update_info(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	struct ec_sb_fw_update_info *resp =
		(struct ec_sb_fw_update_info *)args->response;

	cprintf(CC_I2C, "firmware update i2c cmd:%x read battery info\n",
			SB_FW_UPDATE_CMD_READ_INFO);

	args->response_size = sizeof(struct ec_sb_fw_update_info);
	if (ec_sb_fw_i2c_access_enable) {
		rv = i2c_read_string(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_READ_INFO,
			(uint8_t *)&resp->info,
			SB_FW_UPDATE_CMD_READ_INFO_SIZE);
		if (rv)
			rv = EC_RES_ERROR;
		ec_sb_fw_update_print_info(resp);
	}
	return EC_RES_SUCCESS;
}

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

static int ec_sb_fw_update_status(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	struct ec_sb_fw_update_status *resp =
		(struct ec_sb_fw_update_status *)args->response;

	ec_sb_fw_i2c_access_enable = 1;

	args->response_size = sizeof(struct ec_sb_fw_update_status);
	cprintf(CC_I2C, "firmware update i2c cmd:%x read status16\n",
			SB_FW_UPDATE_CMD_READ_STATUS);

	if (ec_sb_fw_i2c_access_enable) {
		rv = i2c_read_string(I2C_PORT_BATTERY, BATTERY_ADDR,
			SB_FW_UPDATE_CMD_READ_STATUS,
			(uint8_t *)&resp->status,
			2);
		ec_sb_fw_update_print_status(resp);
		if (rv) {
			cprintf(CC_I2C, "i2c cmd:%x read status error:%d\n",
					SB_FW_UPDATE_CMD_READ_STATUS, rv);
			return EC_RES_ERROR;
		}
	}
	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_protect(struct host_cmd_handler_args *args)
{
#if 0
	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_PROTECT);
#endif
	ec_sb_fw_i2c_access_enable = 0;
	cprintf(CC_I2C, "firmware enter protect state !\n");
	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_write(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	int i;

	struct ec_sb_fw_update_write *write =
		(struct ec_sb_fw_update_write *)args->params;
	args->response_size = 0;

	if (ec_sb_fw_update_is_protect())
		return EC_RES_INVALID_COMMAND;

	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_WRITE);

	if (write->size != SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE) {
		cprintf(CC_I2C, "firmware update i2c write size:%x error.\n",
				write->size);
		return EC_RES_INVALID_PARAM;
	}

	cprintf(CC_I2C, "firmware update i2c write off:%x\n",
		write->offset);
	for (i = 0; i < write->size; i++)
		cprintf(CC_I2C, "%02X ", write->data[i]);
	cprintf(CC_I2C, "\n");
	write->data[write->size] =
		crc8(&write->smbus_cmd, SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE+1);

	write->smbus_cmd = SB_FW_UPDATE_CMD_WRITE_BLOCK;
	i2c_lock(I2C_PORT_BATTERY, 1);
	if (ec_sb_fw_i2c_access_enable) {
		rv = i2c_xfer(I2C_PORT_BATTERY, BATTERY_ADDR,
			&write->smbus_cmd,
			1  /* 1 byte address */
			+ SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE /*32 byte data*/
			+ SB_FW_UPDATE_CMD_PEC_SIZE,
			NULL, 0, I2C_XFER_SINGLE);
		if (rv)
			rv = EC_RES_ERROR;
	}
	i2c_lock(I2C_PORT_BATTERY, 0);
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

