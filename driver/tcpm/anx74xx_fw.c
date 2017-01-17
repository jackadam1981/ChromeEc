/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "util.h"
#include "timer.h"
#include "gpio.h"
#include "watchdog.h"
#include "chipset.h"
#include "registers.h"
#include "system.h"

#define OTP_UPDATE_MAX  16

static unsigned char InactiveWord[9] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static unsigned char BlankWord[9] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static unsigned char HammingTable[] = {
	0x0b, 0x3b, 0x37, 0x07, 0x19, 0x29, 0x49, 0x89,
	0x16, 0x26, 0x46, 0x86, 0x13, 0x23, 0x43, 0x83,
	0x1c, 0x2c, 0x4c, 0x8c, 0x15, 0x25, 0x45, 0x85,
	0x1a, 0x2a, 0x4a, 0x8a, 0x0d, 0xcd, 0xce, 0x0e,
	0x70, 0x73, 0xb3, 0xb0, 0x51, 0x52, 0x54, 0x58,
	0xa1, 0xa2, 0xa4, 0xa8, 0x31, 0x32, 0x34, 0x38,
	0xc1, 0xc2, 0xc4, 0xc8, 0x61, 0x62, 0x64, 0x68,
	0x91, 0x92, 0x94, 0x98, 0xe0, 0xec, 0xdc, 0xd0,
};

static unsigned char hamming_checksum(const uint8_t *data)
{
	unsigned char i, j;
	unsigned char result;
	unsigned char c;

	for (result = 0, i = 0; i < 8; i++) {
		c = *data;
		for (j = 0; j < 8; j++) {
			if (c & 0x1)
				result ^= HammingTable[(i << 3) + j];
			c >>= 1;
		}
		data++;
	}
	return result;
}

static void reset(void)
{
	board_set_tcpc_power_mode(0, 0);
	msleep(500);

	board_set_tcpc_power_mode(0, 1);
	msleep(10);
}

static int read_register(unsigned char reg_addr, unsigned char *data)
{
	int val;
	int ret = EC_SUCCESS;

	watchdog_reload();
	ret = i2c_read8(0, 0x50, reg_addr, &val);
	if (ret == EC_SUCCESS)
		*data = (uint8_t) val;
	return ret;
}

static int read_block_register(unsigned char reg_addr, int datalen,
			unsigned char *data)
{
	int i;
	int ret = EC_SUCCESS;

	for (i = 0; i < datalen; i++) {
		ret = read_register(reg_addr + i, data + i);
		if (ret != EC_SUCCESS)
			break;
	}
	return ret;
}

static int write_register(unsigned char reg_addr, unsigned char data)
{
	int val = data;
	int rc;

	watchdog_reload();
	rc = i2c_write8(0, 0x50, reg_addr, val);
	return rc;
}

static int write_word_register(unsigned char reg_addr, unsigned int data)
{
	int rc;

	rc = write_register(reg_addr, (uint8_t) (data >> 8));
	if (rc != EC_SUCCESS)
		return rc;
	return write_register(reg_addr + 1, (uint8_t) (data & 0x00ff));
}

static int write_block_register(unsigned char reg_addr, int datalen,
			const uint8_t *data)
{
	int i;
	int ret = EC_SUCCESS;

	for (i = 0; i < datalen; i++) {
		ret = write_register(reg_addr + i, data[i]);
		if (ret != EC_SUCCESS)
			break;
	}
	return ret;
}

static int OTP_Init(void)
{
	int rc;

	write_register(0x0d, 0x02);
	write_register(0xe5, 0xa8);
	rc = write_register(0xef, 0x7a);
	return rc;
}

static int OTP_UnInit(void)
{
	return write_register(0xef, 0x00);
}

static int OTP_Read(unsigned int addr, unsigned char *data)
{
	int ret = EC_SUCCESS;
	unsigned char status;

	write_register(0xe5, 0xa8);
	write_word_register(0xd0, addr);
	write_register(0xe5, 0xa9);
	do {
		ret = read_register(0xed, &status);
		if (ret != EC_SUCCESS)
			return ret;
	} while ((status & 0x30) != 0);

	ret = read_block_register(0xdc, 8, data);
	return ret;
}

static int OTP_ReadEx(unsigned int addr, unsigned char *data)
{
	int ret = EC_SUCCESS;
	unsigned char status;

	write_register(0xe5, 0xa0);
	write_word_register(0xd0, addr);
	write_register(0xe5, 0xa1);
	do {
		ret = read_register(0xed, &status);
		if (ret != EC_SUCCESS)
			return ret;
	} while ((status & 0x30) != 0);

	ret = read_block_register(0xdc, 9, data);
	return ret;
}

static int OTP_ReadAndCompare(unsigned char addr, const uint8_t *data)
{
	unsigned char rdbuf[8];

	OTP_Read(addr, rdbuf);
	if (memcmp(rdbuf, data, 9) != 0) {
		OTP_ReadEx(addr, rdbuf);
		if (memcmp(rdbuf, data, 9) != 0)
			return EC_RES_ERROR;
	}
	return EC_SUCCESS;
}

static int OTP_Program(unsigned int addr, const uint8_t data[9])
{
	unsigned char status;
	unsigned int csum = hamming_checksum(data);

	write_word_register(0xd0, addr);
	write_block_register(0xd2, 8, data);
	write_register(0xdb, csum);

	write_register(0xe5, 0xaa);

	do {
		read_register(0xed, &status);
	} while ((status & 0x0f) != 0);

	if (OTP_ReadAndCompare(addr, data) == EC_SUCCESS)
		return EC_SUCCESS;

	return EC_RES_ERROR;
}

static int OTP_find_active_fw(unsigned int *start, unsigned int *size)
{
	int i;
	unsigned char data[9];

	for (i = 2; i < OTP_UPDATE_MAX; i++) {
		OTP_ReadEx(i, data);
		if (memcmp(data, InactiveWord, sizeof(InactiveWord)) != 0)
			break;
	}
	if (i == OTP_UPDATE_MAX)
		return -1;

	*start = (data[1] * 256) + data[0];
	*size  = (data[3] * 256) + data[2];

	ccprintf("%x %x\n", *(uint16_t *) start, *(uint16_t *)size);

	return i;
}

static int complete_update(int index, unsigned int start, unsigned int size)
{
	uint8_t data[8] = {0};
	int rc;

	rc = OTP_ReadAndCompare(index + 1, BlankWord);
	if (rc != EC_SUCCESS)
		return rc;

	rc = OTP_Program(index, InactiveWord);
	if (rc != EC_SUCCESS)
		return rc;

	data[0] = start & 0x00ff;
	data[1] = (start >> 8) & 0x00ff;
	data[2] = size & 0x00ff;
	data[3] = (size >> 8) & 0x00ff;

	rc = OTP_Program(index + 1, data);
	if (rc != EC_SUCCESS) {
		OTP_Program(index + 1, InactiveWord);
		return rc;
	}

	return OTP_UnInit();
}

int anx74xx_fw_update(struct host_cmd_handler_args *args)
{
	const struct ec_params_usb_tcpc_fw_update *in = args->params;
	struct ec_response_usb_tcpc_fw_update *out = args->response;

	static uint16_t active_fw_index;
	static uint16_t flash_start_address;
	static uint16_t flash_prog_address;

	unsigned int active_fw_addr;
	unsigned int active_fw_size;

	int rc = EC_SUCCESS;

	switch (in->cmd) {
	case TCPC_FWU_IDENTIFY:
		args->response_size = sizeof(*out);
		reset();
		out->ROM_ID[0] = 0;
		rc = read_register(0x44, (char *) &out->ROM_ID[0]);
		args->result = EC_RES_SUCCESS;
		ccprintf("Version: %d\n", out->ROM_ID[0]);
		break;
	case TCPC_FWU_DUMP:
		args->response_size = sizeof(*out);
		reset();
		OTP_Init();
		rc = OTP_Read(in->address, out->buffer);
		OTP_UnInit();
		break;
	case TCPC_FWU_PREPARE:
		OTP_Init();
		OTP_ReadAndCompare(0, BlankWord);
		active_fw_index = OTP_find_active_fw(&active_fw_addr,
					&active_fw_size);
		if (active_fw_index == -1)
			return EC_RES_ERROR;

		flash_start_address = active_fw_addr + active_fw_size;
		flash_prog_address = flash_start_address;
		ccprintf("FWI: %d program at %x\n", active_fw_index,
				flash_prog_address);
		break;
	case TCPC_FWU_PROGRAM:
		rc = OTP_Program(flash_prog_address, in->data);
		flash_prog_address += 1;
		break;
	case TCPC_FWU_FINALIZE:
		rc = complete_update(active_fw_index, flash_start_address,
				flash_prog_address - flash_start_address);
	default:
		rc = EC_RES_INVALID_PARAM;
		break;
	}
	return rc;
}

