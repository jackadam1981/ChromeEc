/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdarg.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/flash/andes_flash_xip_api_ex.h>
#include <zephyr/init.h>
#include <zephyr/drivers/syscon.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#include <zephyr/logging/log.h>

#include "fpsensor/fpsensor.h"
#include "spi_flash_reg.h"
#include <stdarg.h>
#include "console.h"
#include "util.h"

#define flash_ctrl_dev DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller))

// TODO move to chip familily code?
// TODO add ifdef extended flash ops and flash driver
// TODO somehow move AE350_SPI1->INTREN = 0; AE350_SPI1->TIMING = 0; here?
static int enable_qspi(void)
{
	int ret;
	struct andes_xip_ex_ops_mem_read_cmd_in rd_cmd = {
		.cmd = FLASH_ANDES_XIP_MEM_RD_CMD_EB,
	};
	struct andes_xip_ex_ops_set_in set_regs = {
		.regs = {0},
		.masks = {0},
	};

	set_regs.regs[1] = SPI_FLASH_SR2_QE;
	set_regs.masks[1] = SPI_FLASH_SR2_QE;
	ret = flash_ex_op(flash_ctrl_dev, FLASH_ANDES_XIP_EX_OP_SET_STATUS_REGS, (uintptr_t)&set_regs, 0);
	if (ret) {
		return ret;
	}

	ret = flash_ex_op(flash_ctrl_dev, FLASH_ANDES_XIP_EX_OP_MEM_READ_CMD, (uintptr_t)&rd_cmd, 0);

	return ret;
}
SYS_INIT(enable_qspi, POST_KERNEL, 51);

static int command_srs(int argc, const char **argv)
{
	char *e;

	if (argc == 3) {
		uint32_t srs = strtoi(argv[1], &e, 0);

		if (*e) {
			return EC_ERROR_PARAM1;
		}

		uint32_t mask = strtoi(argv[2], &e, 0);
		if (*e) {
			return EC_ERROR_PARAM2;
		}
		
		struct andes_xip_ex_ops_set_in op_in;

		op_in.regs[0] = (uint8_t)(srs     & 0xff);
		op_in.regs[1] = (uint8_t)(srs>>8  & 0xff);
		op_in.regs[2] = (uint8_t)(srs>>16 & 0xff);
		op_in.masks[0] = (uint8_t)(mask     & 0xff);
		op_in.masks[1] = (uint8_t)(mask>>8  & 0xff);
		op_in.masks[2] = (uint8_t)(mask>>16 & 0xff);

		return flash_ex_op(flash_ctrl_dev, FLASH_ANDES_XIP_EX_OP_SET_STATUS_REGS, (uintptr_t)&op_in, 0);
	} else if (argc == 1) {
		int ret;
		struct andes_xip_ex_ops_get_out op_out;

		ret = flash_ex_op(flash_ctrl_dev, FLASH_ANDES_XIP_EX_OP_GET_STATUS_REGS, (uintptr_t)NULL, &op_out);

		if (ret) {
			printk("error: %d\n", ret);
		} else {
			printk("sr1: 0x%02x\n", op_out.regs[0]);
			printk("sr2: 0x%02x\n", op_out.regs[1]);
			printk("sr3: 0x%02x\n", op_out.regs[2]);
		}
	} else {
		return -1;
	}

	return EC_SUCCESS;
};
DECLARE_CONSOLE_COMMAND(srs, command_srs, "", "");

static int command_srs_unlock(int argc, const char **argv)
{
	struct andes_xip_ex_ops_lock_in op_in = {
		.enable = false,
	};

	return flash_ex_op(flash_ctrl_dev, FLASH_ANDES_XIP_EX_OP_LOCK, (uintptr_t)&op_in, 0);
};
DECLARE_CONSOLE_COMMAND(srs_unlock, command_srs_unlock, "", "");

int fp_vendor_command(uint32_t param, uint8_t *buf, size_t buf_size)
{
	const size_t mt_data_size = 64;
	memcpy(buf, (uint8_t *)0x800001BD, mt_data_size);

	return mt_data_size;
}

static int wp_custom = 0;
int write_protect_is_asserted_custom(void)
{
	return wp_custom;
}

static int command_wp1(int argc, const char **argv)
{
	wp_custom = 1;

	return 0;
};
DECLARE_CONSOLE_COMMAND(wp1, command_wp1, "", "");

static int command_wp0(int argc, const char **argv)
{
	wp_custom = 0;

	return 0;
};
DECLARE_CONSOLE_COMMAND(wp0, command_wp0, "", "");
