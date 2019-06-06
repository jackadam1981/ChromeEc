/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MAX32660 Flash Memory Module for Chrome EC */

#include "flash.h"
#include "switch.h"
#include "system.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"
#include "registers.h"
#include "mxc_errors.h"
#include "icc_regs.h"
#include "flc_regs.h"

#define CPUTS(outstr) cputs(CC_SYSTEM, outstr)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

/***** Definitions *****/

/// Bit mask that can be used to find the starting address of a page in flash
#define MXC_FLASH_PAGE_MASK ~(MXC_FLASH_PAGE_SIZE - 1)

/// Calculate the address of a page in flash from the page number
#define MXC_FLASH_PAGE_ADDR(page)                                              \
	(MXC_FLASH_MEM_BASE + ((unsigned long)page * MXC_FLASH_PAGE_SIZE))

#pragma GCC push_options /* Save current optimization level */
#pragma GCC optimize(                                                          \
	"O0") /* Set optimization level to none for this function */
void flash_operation(void)
{
	volatile uint32_t *line_addr;
	volatile uint32_t __attribute__((unused)) line;

	// Clear the cache
	MXC_ICC->cache_ctrl ^= MXC_F_ICC_CACHE_CTRL_CACHE_EN;
	MXC_ICC->cache_ctrl ^= MXC_F_ICC_CACHE_CTRL_CACHE_EN;

	// Clear the line fill buffer
	line_addr = (uint32_t *)(MXC_FLASH_MEM_BASE);
	line = *line_addr;

	line_addr = (uint32_t *)(MXC_FLASH_MEM_BASE + MXC_FLASH_PAGE_SIZE);
	line = *line_addr;
}

static int flash_busy(void)
{
	return (MXC_FLC->cn &
		(MXC_F_FLC_CN_WR | MXC_F_FLC_CN_ME | MXC_F_FLC_CN_PGE));
}

static int flash_init_controller(void)
{
	// Set flash clock divider to generate a 1MHz clock from the APB clock
	MXC_FLC->clkdiv = SystemCoreClock / 1000000;

	/* Check if the flash controller is busy */
	if (flash_busy()) {
		return E_BUSY;
	}

	/* Clear stale errors */
	if (MXC_FLC->intr & MXC_F_FLC_INTR_AF) {
		MXC_FLC->intr &= ~MXC_F_FLC_INTR_AF;
	}

	/* Unlock flash */
	MXC_FLC->cn = (MXC_FLC->cn & ~MXC_F_FLC_CN_UNLOCK) |
		      MXC_S_FLC_CN_UNLOCK_UNLOCKED;

	return E_NO_ERROR;
}

static int flash_device_page_erase(uint32_t address)
{
	int err;

	if ((err = flash_init_controller()) != E_NO_ERROR)
		return err;

	// Align address on page boundary
	address = address - (address % MXC_FLASH_PAGE_SIZE);

	/* Write paflash_init_controllerde */
	MXC_FLC->cn = (MXC_FLC->cn & ~MXC_F_FLC_CN_ERASE_CODE) |
		      MXC_S_FLC_CN_ERASE_CODE_ERASEPAGE;
	/* Issue page erase command */
	MXC_FLC->addr = address;
	MXC_FLC->cn |= MXC_F_FLC_CN_PGE;

	/* Wait until flash operation is complete */
	while (flash_busy())
		;

	/* Lock flash */
	MXC_FLC->cn &= ~MXC_F_FLC_CN_UNLOCK;

	/* Check access violations */
	if (MXC_FLC->intr & MXC_F_FLC_INTR_AF) {
		MXC_FLC->intr &= ~MXC_F_FLC_INTR_AF;
		return E_BAD_STATE;
	}

	flash_operation();

	return E_NO_ERROR;
}

static int flash_device_write(uint32_t address, uint32_t length,
			      uint8_t *buffer)
{
	int err;
	uint32_t bytes_written;
	uint8_t current_data[4];

	if ((err = flash_init_controller()) != E_NO_ERROR)
		return err;

	// write in 32-bit units until we are 128-bit aligned
	MXC_FLC->cn &= ~MXC_F_FLC_CN_BRST;
	MXC_FLC->cn |= MXC_F_FLC_CN_WDTH;

	// Align the address and read/write if we have to
	if (address & 0x3) {

		// Figure out how many bytes we have to write to round up the
		// address
		bytes_written = 4 - (address & 0x3);

		// Save the data currently in the flash
		memcpy(current_data, (void *)(address & (~0x3)), 4);

		// Modify current_data to insert the data from buffer
		memcpy(&current_data[4 - bytes_written], buffer, bytes_written);

		// Write the modified data
		MXC_FLC->addr = address - (address % 4);
		memcpy((void *)&MXC_FLC->data[0], &current_data, 4);
		MXC_FLC->cn |= MXC_F_FLC_CN_WR;

		/* Wait until flash operation is complete */
		while (flash_busy())
			;

		address += bytes_written;
		length -= bytes_written;
		buffer += bytes_written;
	}

	while ((length >= 4) && ((address & 0x1F) != 0)) {
		MXC_FLC->addr = address;
		memcpy((void *)&MXC_FLC->data[0], buffer, 4);
		MXC_FLC->cn |= MXC_F_FLC_CN_WR;

		/* Wait until flash operation is complete */
		while (flash_busy())
			;

		address += 4;
		length -= 4;
		buffer += 4;
	}

	if (length >= 16) {

		// write in 128-bit bursts while we can
		MXC_FLC->cn &= ~MXC_F_FLC_CN_WDTH;

		while (length >= 16) {
			MXC_FLC->addr = address;
			memcpy((void *)&MXC_FLC->data[0], buffer, 16);
			MXC_FLC->cn |= MXC_F_FLC_CN_WR;

			/* Wait until flash operation is complete */
			while (flash_busy())
				;

			address += 16;
			length -= 16;
			buffer += 16;
		}

		// Return to 32-bit writes.
		MXC_FLC->cn |= MXC_F_FLC_CN_WDTH;
	}

	while (length >= 4) {
		MXC_FLC->addr = address;
		memcpy((void *)&MXC_FLC->data[0], buffer, 4);
		MXC_FLC->cn |= MXC_F_FLC_CN_WR;

		/* Wait until flash operation is complete */
		while (flash_busy())
			;

		address += 4;
		length -= 4;
		buffer += 4;
	}

	if (length > 0) {
		// Save the data currently in the flash
		memcpy(current_data, (void *)(address), 4);

		// Modify current_data to insert the data from buffer
		memcpy(current_data, buffer, length);

		MXC_FLC->addr = address;
		memcpy((void *)&MXC_FLC->data[0], current_data, 4);
		MXC_FLC->cn |= MXC_F_FLC_CN_WR;

		/* Wait until flash operation is complete */
		while (flash_busy())
			;
	}

	/* Lock flash */
	MXC_FLC->cn &= ~MXC_F_FLC_CN_UNLOCK;

	/* Check access violations */
	if (MXC_FLC->intr & MXC_F_FLC_INTR_AF) {
		MXC_FLC->intr &= ~MXC_F_FLC_INTR_AF;
		return E_BAD_STATE;
	}

	flash_operation();

	return E_NO_ERROR;
}

/*****************************************************************************/
/* Physical layer APIs */

int flash_physical_write(int offset, int size, const char *data)
{
	int error_status;

	/* write 'size' number of bytes to address 'offset' */
	error_status = flash_device_write(offset, size, (uint8_t *)data);
	if (error_status != E_NO_ERROR) {
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

int flash_physical_erase(int offset, int size)
{
	int i;
	int pages;
	int error_status;

	/*
	 * erase 'size' number of bytes starting at address 'offset'
	 */
	/* calculate the number of pages */
	pages = size / CONFIG_FLASH_ERASE_SIZE;
	/* iterate over the number of pages */
	for (i = 0; i < pages; i++) {
		/* erase the page after calculating the start address */
		error_status = flash_device_page_erase(
			offset + (i * CONFIG_FLASH_ERASE_SIZE));
		if (error_status != E_NO_ERROR) {
			return EC_ERROR_UNKNOWN;
		}
	}
	return EC_SUCCESS;
}

int flash_physical_get_protect(int bank)
{
	/* Not protected */
	return 0;
}

uint32_t flash_physical_get_protect_flags(void)
{
	/* no flags set */
	return 0;
}

uint32_t flash_physical_get_valid_flags(void)
{
	/* These are the flags we're going to pay attention to */
	return EC_FLASH_PROTECT_RO_AT_BOOT | EC_FLASH_PROTECT_RO_NOW |
	       EC_FLASH_PROTECT_ALL_NOW;
}

uint32_t flash_physical_get_writable_flags(uint32_t cur_flags)
{
	/* no flags writable */
	return 0;
}

int flash_physical_protect_at_boot(uint32_t new_flags)
{
	/* nothing to do here */
	return EC_SUCCESS;
}

int flash_physical_protect_now(int all)
{
	/* nothing to do here */
	return EC_SUCCESS;
}

/*****************************************************************************/
/* High-level APIs */

int flash_pre_init(void)
{
	return EC_SUCCESS;
}

/*****************************************************************************/
/* Test Commands */

/*
 * Read, Write, and Erase a page of flash memory using chip routines
 * NOTE: This is a DESTRUCTIVE test for the range of flash pages tested
 *       make sure that PAGE_START is beyond your flash code.
 */
static int command_flash_test1(int argc, char **argv)
{
	int i;
	uint8_t *ptr;
	const uint32_t PAGE_START = 9;
	const uint32_t PAGE_END = 32;
	uint32_t page;
	int error_status;
	uint32_t flash_address;
	const int BUFFER_SIZE = 32;
	uint8_t buffer[BUFFER_SIZE];

	/*
	 * As a test, write unique data to each page in this for loop, later
	 * verify data in pages
	 */
	for (page = PAGE_START; page < PAGE_END; page++) {
		flash_address = page * CONFIG_FLASH_ERASE_SIZE;

		/*
		 * erase page
		 */
		error_status = flash_physical_erase(flash_address,
						    CONFIG_FLASH_ERASE_SIZE);
		if (error_status != EC_SUCCESS) {
			CPRINTS("Error with flash_physical_erase\n");
			return EC_ERROR_UNKNOWN;
		}

		/*
		 * verify page was erased
		 */
		// CPRINTS("read flash page %d, address %x, ", page,
		// flash_address);
		ptr = (uint8_t *)flash_address;
		for (i = 0; i < CONFIG_FLASH_ERASE_SIZE; i++) {
			if (*ptr++ != 0xff) {
				CPRINTS("Error with verifying page erase\n");
				return EC_ERROR_UNKNOWN;
			}
		}

		/*
		 * write pattern to page, just write BUFFER_SIZE worth of data
		 */
		for (i = 0; i < BUFFER_SIZE; i++) {
			buffer[i] = i + page;
		}
		error_status = flash_physical_write(flash_address, BUFFER_SIZE,
						    buffer);
		if (error_status != EC_SUCCESS) {
			CPRINTS("Error with flash_physical_write\n");
			return EC_ERROR_UNKNOWN;
		}
	}

	/*
	 * Verify data in pages
	 */
	for (page = PAGE_START; page < PAGE_END; page++) {
		flash_address = page * CONFIG_FLASH_ERASE_SIZE;

		/*
		 * read a portion of flash memory
		 */
		ptr = (uint8_t *)flash_address;
		for (i = 0; i < BUFFER_SIZE; i++) {
			if (*ptr++ != (i + page)) {
				CPRINTS("Error with verifing written test "
					"data\n");
				return EC_ERROR_UNKNOWN;
			}
		}
		CPRINTS("Verified Erase, Write, Read page %d", page);
	}

	/*
	 * Clean up after tests
	 */
	for (page = PAGE_START; page <= PAGE_END; page++) {
		flash_address = page * CONFIG_FLASH_ERASE_SIZE;
		error_status = flash_physical_erase(flash_address,
						    CONFIG_FLASH_ERASE_SIZE);
		if (error_status != EC_SUCCESS) {
			CPRINTS("Error with flash_physical_erase\n");
			return EC_ERROR_UNKNOWN;
		}
	}

	CPRINTS("done command_flash_test1.");
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(flashtest1, command_flash_test1, "flashtest1",
			"Flash chip routine tests");
