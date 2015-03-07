/* Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "flash.h"
#include "host_command.h"
#include "shared_mem.h"
#include "spi.h"
#include "spi_flash.h"
#include "system.h"
#include "util.h"
#include "watchdog.h"

#define PAGE_SIZE 256

/**
 * Read from physical flash.
 *
 * @param offset        Flash offset to write.
 * @param size          Number of bytes to write.
 * @param data          Destination buffer for data.  Must be 32-bit aligned.
 */
int flash_physical_read(int offset, int size, char *data)
{
	int ret;

	/* Fail if offset, size, and data aren't at least word-aligned */
	if ((offset | size | (uint32_t)(uintptr_t)data) & 3)
		return EC_ERROR_INVAL;

	spi_enable(1);

	ret = spi_flash_read((uint8_t *)data, offset, size);

	spi_enable(0);
	return ret;
}

/**
 * Write to physical flash.
 *
 * Offset and size must be a multiple of CONFIG_FLASH_WRITE_SIZE.
 *
 * @param offset        Flash offset to write.
 * @param size          Number of bytes to write.
 * @param data          Data to write to flash.  Must be 32-bit aligned.
 */
int flash_physical_write(int offset, int size, const char *data)
{
	int ret, i, write_size;

	/* Fail if offset, size, and data aren't at least word-aligned */
	if ((offset | size | (uint32_t)(uintptr_t)data) & 3)
		return EC_ERROR_INVAL;

	spi_enable(1);

	for (i = 0; i < size; i += write_size) {
		write_size = MIN(size, SPI_FLASH_MAX_WRITE_SIZE);
		ret = spi_flash_write(offset + i,
				      write_size,
				      (uint8_t *)data + i);
		if (ret != EC_SUCCESS)
			break;
		/* BUG: Multi-page writes fail if no delay */
		msleep(1);
	}

	spi_enable(0);
	return ret;
}

/**
 * Erase physical flash.
 *
 * Offset and size must be a multiple of CONFIG_FLASH_ERASE_SIZE.
 *
 * @param offset        Flash offset to erase.
 * @param size          Number of bytes to erase.
 */
int flash_physical_erase(int offset, int size)
{
	int ret;

	/* Fail if offset and size aren't at least word-aligned */
	if ((offset | size ) & 3)
		return EC_ERROR_INVAL;

	spi_enable(1);

	ret = spi_flash_erase(offset, CONFIG_FLASH_ERASE_SIZE);

	spi_enable(0);
	return ret;
}

/**
 * Read physical write protect setting for a flash bank.
 *
 * @param bank    Bank index to check.
 * @return        non-zero if bank is protected until reboot.
 */
int flash_physical_get_protect(int bank)
{
	uint32_t addr = bank * CONFIG_FLASH_BANK_SIZE;
	int ret;

	spi_enable(1);
	ret = spi_flash_check_protect(addr, CONFIG_FLASH_BANK_SIZE);
	spi_enable(0);
	return ret;
}

/**
 * Protect flash now.
 *
 * @param all      Protect all (=1) or just read-only and pstate (=0).
 * @return         non-zero if error.
 */
int flash_physical_protect_now(int all)
{
	int offset, size, ret;

	if (all) {
		offset = 0;
		size = CONFIG_FLASH_PHYSICAL_SIZE;
	} else {
		offset = CONFIG_FW_RO_OFF;
		size = CONFIG_FW_RO_SIZE;
	}

	spi_enable(1);
	ret = spi_flash_set_protect(offset, size);
	spi_enable(0);
	return ret;
}

/**
 * Return flash protect state flags from the physical layer.
 *
 * This should only be called by flash_get_protect().
 *
 * Uses the EC_FLASH_PROTECT_* flags from ec_commands.h
 */
uint32_t flash_physical_get_protect_flags(void)
{
	uint32_t flags = 0;

	spi_enable(1);
	if (spi_flash_check_protect(CONFIG_FW_RO_OFF, CONFIG_FW_RO_SIZE))
		flags |= EC_FLASH_PROTECT_RO_AT_BOOT;

	if (spi_flash_check_protect(CONFIG_FW_RW_OFF, CONFIG_FW_RW_SIZE))
		flags |= EC_FLASH_PROTECT_ALL_NOW;
	spi_enable(0);

	return flags;
}

/**
 * Return the valid flash protect flags.
 *
 * @return   A combination of EC_FLASH_PROTECT_* flags from ec_commands.h
 */
uint32_t flash_physical_get_valid_flags(void)
{
	return EC_FLASH_PROTECT_RO_AT_BOOT |
	       EC_FLASH_PROTECT_RO_NOW |
	       EC_FLASH_PROTECT_ALL_NOW;
}

/**
 * Return the writable flash protect flags.
 *
 * @param    cur_flags The current flash protect flags.
 * @return   A combination of EC_FLASH_PROTECT_* flags from ec_commands.h
 */
uint32_t flash_physical_get_writable_flags(uint32_t cur_flags)
{
	uint32_t ret = 0;

	/* If RO protection isn't enabled, its at-boot state can be changed. */
	if (!(cur_flags & EC_FLASH_PROTECT_RO_NOW))
		ret |= EC_FLASH_PROTECT_RO_AT_BOOT;

	/*
	 * If entire flash isn't protected at this boot, it can be enabled if
	 * the WP GPIO is asserted.
	 */
	if (!(cur_flags & EC_FLASH_PROTECT_ALL_NOW) &&
			(cur_flags & EC_FLASH_PROTECT_GPIO_ASSERTED))
		ret |= EC_FLASH_PROTECT_ALL_NOW;

	return ret;
}

/**
 * Enable write protect for the specified range.
 *
 * Once write protect is enabled, it will STAY enabled until the system is
 * hard-rebooted with the hardware write protect pin deasserted.  If the write
 * protect pin is deasserted, the protect setting is ignored, and the entire
 * flash will be writable.
 *
 * @param range         The range to protect.
 * @return              EC_SUCCESS, or nonzero if error.
 */
int flash_physical_protect_at_boot(enum flash_wp_range range)
{
	int offset, size, ret;

	switch(range) {
	case FLASH_WP_NONE:
		offset = size = 0;
		break;
	case FLASH_WP_RO:
		offset = CONFIG_FW_RO_OFF;
		size = CONFIG_FW_RO_SIZE;
		break;
	case FLASH_WP_ALL:
		offset = 0;
		size = CONFIG_FLASH_PHYSICAL_SIZE;
		break;
	}

	spi_enable(1);
	ret = spi_flash_set_protect(offset, size);
	spi_enable(0);
	return ret;
}

/**
 * Initialize the module.
 *
 * Applies at-boot protection settings if necessary.
 */
int flash_pre_init(void)
{
	return EC_SUCCESS;
}

#ifdef CONFIG_CMD_FLASH
static int flash_get_image_used_spi(enum system_image_copy_t copy)
{
	char *flash_data;
	uint32_t image_offset, image_size, read_offset;
	int i;
	int ret = 0;

	/*
	 * BUG: Don't use shared_mem_acquire if we ever need to use this
	 * function outside of debug console commands.
	 */
	if (shared_mem_acquire(PAGE_SIZE, &flash_data) != EC_SUCCESS)
		return -1;

	if (copy == SYSTEM_IMAGE_RO) {
		image_offset = CONFIG_FW_RO_OFF;
		image_size = CONFIG_FW_RO_SIZE;
	} else {
		image_offset = CONFIG_FW_RW_OFF;
		image_size = CONFIG_FW_RW_SIZE;
	}

	ASSERT(image_size % PAGE_SIZE == 0);

	/* Scan backwards looking for a non-0xff byte */
	for (read_offset = image_offset + image_size;
	     read_offset > 0;
	     read_offset -= PAGE_SIZE) {
		watchdog_reload();

		flash_physical_read(
			image_offset + read_offset - PAGE_SIZE,
			PAGE_SIZE,
			(uint8_t *)flash_data);

		for (i = PAGE_SIZE - 1; i >= 0; i--)
			if (flash_data[i] != 0xff) {
				ret = read_offset - PAGE_SIZE + i + 1;
				goto done;
			}
	}

done:
	shared_mem_release(flash_data);
	return ret;
}

static int command_flash_test(int argc, char **argv)
{
	int ret;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (strcasecmp(argv[1], "used") == 0) {
		ccprintf("USED %x\n",
			flash_get_image_used_spi(SYSTEM_IMAGE_RW));
		ret = EC_SUCCESS;
	} else if (strcasecmp(argv[1], "protect") == 0) {
		spi_enable(1);
		ret = spi_flash_set_protect(0, 0x1000);
		spi_enable(0);
	} else if (strcasecmp(argv[1], "unprotect") == 0) {
		spi_enable(1);
		ret = spi_flash_set_protect(0, 0);
		spi_enable(0);
	}

	return ret;
}
DECLARE_CONSOLE_COMMAND(mecflashtest,
			command_flash_test,
			"[used | protect | unprotect]",
			"Test mec1322 SPI flash commands",
			NULL);
#endif
