/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zephyr/init.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/flash/andes_flash_xip_api_ex.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#include <zephyr/logging/log.h>

#include <stdarg.h>
#include "console.h"
#include "printf.h"
#include "system.h"
#include "uart.h"
#include <zephyr/linker/linker-defs.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/drivers/cache.h>
#include <zephyr/drivers/flash.h>

#include "util.h"

#include <stdarg.h>

#define flash_ctrl_dev DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller))

static int command_write3(int argc, const char **argv)
{
	uint32_t off = 0;
#ifdef CONFIG_CROS_EC_RW
	uint32_t start = 0x20000;
	const int size = 0x60000;
#else
	uint32_t start = 0x80000;
	const int size = 0x80000;
#endif
	const int chank = 300;
	uint8_t data[chank];
	for (int i = 0; i < chank; i++) {
		data[i] = i+1;
	}

	for (int j = 0; j < 500; j++) {
		printk("DN write %d\n", j);
		flash_erase(flash_ctrl_dev, start, size);
		for (off = 0; off < size; off += chank) {
			flash_write(flash_ctrl_dev, start + off, data, chank);
		}
	}

	// printk("DN cmd write end\n");

	return 0;
};
DECLARE_SAFE_CONSOLE_COMMAND(write3, command_write3,
			     "[ save | restore | <mask> | <name> ]",
			     "Save, restore, get or set console channel mask");

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
			printk("DN error: %d\n", ret);
		} else {
			printk("DN sr1: 0x%02x\n", op_out.regs[0]);
			printk("DN sr2: 0x%02x\n", op_out.regs[1]);
			printk("DN sr3: 0x%02x\n", op_out.regs[2]);
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

void trng_rand_bytes(void *buffer, size_t len);

static int command_rand(int argc, const char **argv)
{
	char *e;
	uint32_t num;

	if (argc == 2) {
		num = strtoi(argv[1], &e, 0);

		if (*e) {
			return EC_ERROR_PARAM1;
		}
	} else {
		return -1;
	}
	uint8_t rand[num];
	trng_rand_bytes(rand, 16);

	for (int i = 0; i < num; i++) {
		printk("%d\n", rand[i]);
	}

	return 0;
};
DECLARE_CONSOLE_COMMAND(rand, command_rand, "", "");

#define flash_ctrl_dev DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller))

static int command_bootrom(int argc, const char **argv)
{
	chip_enter_bootloader(0);

	return 0;
};
DECLARE_CONSOLE_COMMAND(bootrom, command_bootrom, "", "");
