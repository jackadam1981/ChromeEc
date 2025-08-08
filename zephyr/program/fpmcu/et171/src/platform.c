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

int fp_vendor_commad(uint32_t param, uint8_t *buf, size_t buf_size)
{
	printk("DN buf_size: %d\n", buf_size);
	return 0;
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
