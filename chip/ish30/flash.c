/* Copyright 2016 The Chromium OS Authors. All rights reserved.
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
#include "hooks.h"


#define FLASH_HOOK_VERSION 1

static int entire_flash_locked;

/* The previous write protect state before sys jump */

struct flash_wp_state {
	int entire_flash_locked;
};

/**
 * Read from physical flash.
 *
 * @param offset        Flash offset to write.
 * @param size          Number of bytes to write.
 * @param data          Destination buffer for data.
 */
int flash_physical_read(int offset, int size, char *data)
{
	return 0;
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
	return 0;
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
	return 0;
}

/**
 * Read physical write protect setting for a flash bank.
 *
 * @param bank    Bank index to check.
 * @return        non-zero if bank is protected until reboot.
 */
int flash_physical_get_protect(int bank)
{
	return 0;
}

/**
 * Protect flash now.
 *
 * This is always successful, and only emulates "now" protection
 *
 * @param all      Protect all (=1) or just read-only
 * @return         non-zero if error.
 */
int flash_physical_protect_now(int all)
{
	if (all)
		entire_flash_locked = 1;

	/*
	 * RO "now" protection is not currently implemented. If needed, it
	 * can be added by splitting the entire_flash_locked variable into
	 * and RO and RW vars, and setting + checking the appropriate var
	 * as required.
	 */
	return EC_SUCCESS;
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

	return ret;
}

/**
 * Enable write protect for the specified range.
 *
 * Once write protect is enabled, it will stay enabled until HW PIN is
 * de-asserted and SRP register is unset.
 *
 * However, this implementation treats FLASH_WP_ALL as FLASH_WP_RO but
 * tries to remember if "all" region is protected.
 *
 * @param range         The range to protect.
 * @return              EC_SUCCESS, or nonzero if error.
 */
int flash_physical_protect_at_boot(enum flash_wp_range range)
{
	int ret = 0;
	return ret;
}

/**
 * Initialize the module.
 *
 * Applies at-boot protection settings if necessary.
 */
int flash_pre_init(void)
{
	flash_physical_restore_state();
	return EC_SUCCESS;
}

int flash_physical_restore_state(void)
{
	return 0;
}

/*****************************************************************************/
/* Hooks */

static void flash_preserve_state(void)
{
}
DECLARE_HOOK(HOOK_SYSJUMP, flash_preserve_state, HOOK_PRIO_DEFAULT);
