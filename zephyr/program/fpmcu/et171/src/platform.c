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

static const struct device *const syscon_dev =
DEVICE_DT_GET(DT_NODELABEL(syscon));

#define AOSMU_BOOT_STRAPPING 0x8 /* Secure key handling */
#define AOSMU_SECURE_CON 0xc /* Secure key handling */
#define AOSMU_SECURE_CON_BYPASS_BOOTSTRAP BIT(17) /* issue reset to MCU core */
#define AOSMU_SECURE_CON_DFU BIT(16) /* issue reset to MCU core */
#define AOSMU_SECURE_CON_WARM_RESET BIT(2) /* issue reset to whole SoC except for reset_vector(0x10) and dummy(0x1c) */
#define AOSMU_RESET_VECTOR 0x10

// TODO implement this as Host Command. Add a new HC or new cmd to REBOOT_EC command?
// TODO consider when we should accept such command. With only singed token? Always, because we use SDCP anyway?
static int command_bootrom(int argc, const char **argv)
{
	uint32_t syscon;

	syscon_read_reg(syscon_dev, AOSMU_SECURE_CON, &syscon);
	syscon_write_reg(syscon_dev, AOSMU_RESET_VECTOR, 0x70000000);
	syscon_write_reg(syscon_dev, AOSMU_SECURE_CON, syscon | AOSMU_SECURE_CON_DFU);
	syscon_write_reg(syscon_dev, AOSMU_SECURE_CON, syscon | AOSMU_SECURE_CON_WARM_RESET | AOSMU_SECURE_CON_DFU);

	return 0;
};
DECLARE_CONSOLE_COMMAND(bootrom, command_bootrom, "", "");

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

// TODO lack of this causes reboot loop. Debug this? Add to soc code?
static int fence(void)
{
	__asm__ volatile("fence.i" ::: "memory");

	return 0;
}
SYS_INIT(fence, PRE_KERNEL_2, 51);
