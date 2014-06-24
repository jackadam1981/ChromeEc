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

/* Return CRC-8 of the data, using x^8 + x^2 + x + 1 polynomial.
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

/* smbus Write n bytes
 *   [S][Slave Address][Wr=0][A][cmd][A] ...[Di][Ai]... [PEC][A][P]
 *   case 1:  n-1 byte data, 1 byte PEC
 *   case 2:  1 byte data-size, n -2 byte data, 1 byte PEC
 *   Note: slave_addr = ( i2c_addr << 1 ) | (1 bit write mode = 0)
 */
int smbus_write(int i2c_port, struct ec_smbus_if *intf, int n)
{
	int rv;
	intf->data[n-1] = crc8(&(intf->slave_addr),
				n - 1 + sizeof(struct ec_smbus_if));
	i2c_lock(i2c_port, 1);
	rv = i2c_xfer(i2c_port, intf->slave_addr,
		&intf->smbus_cmd, n+1, NULL, 0, I2C_XFER_SINGLE);
	i2c_lock(i2c_port, 0);
	return rv;
}

/* smbus Read n bytes
 *   tx 8-bit smbus cmd, and read n bytes
 *   [S][Slave Address][Rd=1][A][cmd][A] ...[Di][Ai]... [PEC][A][P]
 *   case 1:  n-1 byte data, 1 byte PEC
 *   case 2:  1 byte data-size, n -2 byte data, 1 byte PEC
 *   Note: slave_addr = ( i2c_addr << 1 ) | (1 bit read mode = 1)
 */
int smbus_read(int i2c_port, struct ec_smbus_rd_if *intf, int n)
{
	int rv;
	uint8_t pec;

	i2c_lock(i2c_port, 1);
	rv = i2c_xfer(i2c_port, intf->slave_addr, &intf->smbus_cmd, 1,
		intf->data, n, I2C_XFER_SINGLE);
	i2c_lock(i2c_port, 0);

	if (rv) {
		cprintf(CC_I2C, "smbus i2c_xfer error:%d\n", rv);
		return rv;
	}

	intf->slave_addr_rd = intf->slave_addr | 0x01;
	pec = crc8(&intf->slave_addr, n-1+sizeof(struct ec_smbus_rd_if));
	if (pec != intf->data[n-1]) {
		cprintf(CC_I2C, "smbus read[%02X] PEC %02X != %02X\n",
			intf->smbus_cmd, intf->data[n-1], pec);
		return EC_RES_ERROR;
	}
	return EC_SUCCESS;
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

static int ec_sb_write16(int smbus_cmd, uint16_t d16)
{
	int rv;
	struct ec_sb_fw_update_write16 s;
	s.intf.slave_addr = BATTERY_ADDR;
	s.intf.smbus_cmd = smbus_cmd;
	s.intf.data[0] = d16 & 0xFF;
	s.intf.data[1] = (d16 >> 8) & 0xFF;
	rv = smbus_write(I2C_PORT_BATTERY, &s.intf,
		2 + SB_FW_UPDATE_CMD_PEC_SIZE);
	return rv;
}

static int ec_sb_fw_update_prepare(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	args->response_size = 0;

	ec_sb_fw_i2c_access_enable = 1;

	if (ec_sb_fw_update_is_protect()) {
		cprintf(CC_I2C, "firmware update i2c cmd:%x data:%04x error\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
		return EC_RES_INVALID_COMMAND;
	}

	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_PREPARE);

	cprintf(CC_I2C, "firmware update i2c cmd:%x data:%04x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
	rv = ec_sb_write16(SB_FW_UPDATE_CMD_WRITE_WORD,
		SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE);
	if (rv)
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}

static int ec_sb_fw_update_begin(struct host_cmd_handler_args *args)
{
	int rv = EC_RES_SUCCESS;
	args->response_size = 0;
	if (ec_sb_fw_update_is_protect())
		return EC_RES_INVALID_COMMAND;

	if (!ec_sb_fw_i2c_access_enable)
		return EC_RES_ERROR;

	ec_sb_fw_update_set_state(EC_CMD_SB_FW_UPDATE_BEGIN);

	cprintf(CC_I2C, "firmware update i2c cmd:%x data:%04x\n",
			SB_FW_UPDATE_CMD_WRITE_WORD,
			SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
	rv = ec_sb_write16(SB_FW_UPDATE_CMD_WRITE_WORD,
		SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE);
	if (rv)
		return EC_RES_ERROR;

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

	if (!ec_sb_fw_i2c_access_enable)
		return EC_RES_ERROR;

	rv = ec_sb_write16(SB_FW_UPDATE_CMD_WRITE_WORD,
		SB_FW_UPDATE_CMD_WRITE_WORD_END);
	if (rv)
		return EC_RES_ERROR;

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

	if (!ec_sb_fw_i2c_access_enable)
		return EC_RES_ERROR;

	resp->intf.slave_addr = BATTERY_ADDR;
	resp->intf.smbus_cmd = SB_FW_UPDATE_CMD_READ_INFO;
	resp->size = 8;
	rv = smbus_read(I2C_PORT_BATTERY, &resp->intf,
		SB_FW_UPDATE_CMD_INFO_SIZE +
		SB_FW_UPDATE_CMD_LEN_SIZE +
		SB_FW_UPDATE_CMD_PEC_SIZE);
	if (rv)
		rv = EC_RES_ERROR;
	ec_sb_fw_update_print_info(resp);
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


	args->response_size = sizeof(struct ec_sb_fw_update_status);
	cprintf(CC_I2C, "firmware update i2c cmd:%x read status16\n",
			SB_FW_UPDATE_CMD_READ_STATUS);

	if (!ec_sb_fw_i2c_access_enable)
		return EC_RES_ERROR;

	resp->intf.slave_addr = BATTERY_ADDR;
	resp->intf.smbus_cmd = SB_FW_UPDATE_CMD_READ_STATUS;
	rv = smbus_read(I2C_PORT_BATTERY, &resp->intf,
		SB_FW_UPDATE_CMD_STATUS_SIZE + SB_FW_UPDATE_CMD_PEC_SIZE);

	ec_sb_fw_update_print_status(resp);
	if (rv) {
		cprintf(CC_I2C, "i2c cmd:%x read status error:%d\n",
				SB_FW_UPDATE_CMD_READ_STATUS, rv);
		return EC_RES_ERROR;
	}
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
#if 0
	int i;
#endif

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

	write->intf.slave_addr = BATTERY_ADDR /* I2C_WRITE_ADDR 0xB << 1 */;
	write->intf.smbus_cmd  = SB_FW_UPDATE_CMD_WRITE_BLOCK;
	rv = smbus_write(I2C_PORT_BATTERY, &write->intf,
		SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE +
		SB_FW_UPDATE_CMD_LEN_SIZE +
		SB_FW_UPDATE_CMD_PEC_SIZE);

#if 0
	cprintf(CC_I2C, "firmware update i2c write off:%x\n",
		write->offset);
	for (i = 0; i < write->size; i++)
		cprintf(CC_I2C, "%02X ", write->data[i]);
	cprintf(CC_I2C, "PEC:%02X\n", write->data[write->size]);
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

