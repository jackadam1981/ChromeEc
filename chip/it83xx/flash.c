/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Flash module for Chrome EC */

#include "console.h"
#include "flash.h"
#include "registers.h"
#include "rom.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Internal SPI flash commands. */
#define SPI_CMD_WRITE_STATUS_REG        0x01
#define SPI_CMD_READ_STATUS             0x05
#define SPI_STATUS_BUSY                 0x01
#define SPI_CMD_WRITE_DISABLE           0x04
#define SPI_CMD_WRITE_ENABLE            0x06
#define SPI_CMD_READ                    0x0b
#define SPI_CMD_ENABLE_WRITE_STATUS_REG 0x50
#define SPI_CMD_READ_ID                 0xab
#define SPI_CMD_DEVICE_ID               0x9f
#define SPI_CMD_PROGRAM_BYTE            0x02
#define SPI_CMD_PROGRAM_AAI_WORD        0xad
#define SPI_CMD_PROGRAM_AAI             0xaf
#define SPI_CMD_ERASE_SECTOR            0xd7
#define SPI_CMD_ERASE_4K_BYTE           0x20
#define SPI_CMD_ERASE_32K_BYTE          0x52
#define SPI_CMD_ERASE_64K_BYTE          0x58

/* Largest size to write at once. */
#define FLASH_WRITE_CHUNK_SIZE 0x1000

/*****************************************************************************/
/* Physical layer APIs */

int flash_physical_read(int offset, int size)
{
	unsigned char dat[16];
	int i, tmpsize;

	while (size > 0) {
		tmpsize = (size > 16) ? 16 : size;

		interrupt_disable();
		spi_ec_indirect_fast_read(FLASH_INTERNAL, offset, dat, tmpsize);
		interrupt_enable();

		ccprintf("0x%08x: ", offset);

		for (i = 0; i < tmpsize; i++)
			ccprintf("%02x ", dat[i]);

		ccprintf("\n");
		cflush();

#ifdef CONFIG_WATCHDOG
		watchdog_reload();
#endif

		offset += tmpsize;
		size -= tmpsize;
	}

	return EC_SUCCESS;
}

int flash_physical_write(int offset, int size, const char *data)
{
	int tmpsize;

	/*
	 * Write in discrete chunks in order to give a chance to service
	 * interrupts and tickle watchdog.
	 */
	while (size) {
		tmpsize = (size >= FLASH_WRITE_CHUNK_SIZE) ?
				FLASH_WRITE_CHUNK_SIZE : size;

		/*
		 * Enable writes to internal flash. Note this is needed before
		 * each write transaction.
		 */
		interrupt_disable();
		spi_write_enable(FLASH_INTERNAL, 1);
		interrupt_enable();

		/* Write to flash. */
		interrupt_disable();
		spi_write_aai_word(FLASH_INTERNAL, offset, data, tmpsize);
		interrupt_enable();

#ifdef CONFIG_WATCHDOG
		watchdog_reload();
#endif

		/* Update write parameters. */
		size -= tmpsize;
		data += tmpsize;
		offset += tmpsize;

	}

	return EC_SUCCESS;
}

int flash_physical_erase(int offset, int size)
{
	/* Erase flash one page at a time. */
	while (size) {
		/*
		 * Enable writes to internal flash. Note this is needed before
		 * each write transaction.
		 */
		interrupt_disable();
		spi_write_enable(FLASH_INTERNAL, 1);
		interrupt_enable();

		/* Erase page. */
		interrupt_disable();
		spi_erase(FLASH_INTERNAL, SPI_CMD_ERASE_SECTOR, offset);
		interrupt_enable();

#ifdef CONFIG_WATCHDOG
		watchdog_reload();
#endif

		size -= CONFIG_FLASH_ERASE_SIZE;
		offset += CONFIG_FLASH_ERASE_SIZE;
	}

	return EC_SUCCESS;
}

int flash_physical_get_protect(int bank)
{
	return 0;
}

uint32_t flash_physical_get_protect_flags(void)
{
	return EC_SUCCESS;
}

int flash_physical_protect_now(int all)
{
	return EC_SUCCESS;
}

/*****************************************************************************/
/* High-level APIs */

int flash_pre_init(void)
{
	return EC_SUCCESS;
}

/*****************************************************************************/
/* Console commands */

static int command_read_flash(int argc, char **argv)
{
	int off, size;
	char *e;

	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;

	off = strtoi(argv[1], &e, 16);
	if (*e)
		return EC_ERROR_PARAM1;

	size = strtoi(argv[2], &e, 10);
	if (*e)
		return EC_ERROR_PARAM2;

	flash_physical_read(off, size);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(flashread2, command_read_flash,
			"addr size",
			"Read internal flash",
			NULL);

static unsigned char write_data[64];
static int command_write_flash(int argc, char **argv)
{
	int off, size, tmp, i;
	char *e;

	if (argc < 3 || argc > 67)
		return EC_ERROR_PARAM_COUNT;

	off = strtoi(argv[1], &e, 16);
	if (*e)
		return EC_ERROR_PARAM1;

	size = strtoi(argv[2], &e, 10);
	if (*e)
		return EC_ERROR_PARAM2;

	for (i = 3; i < argc; i++) {
		tmp = strtoi(argv[i], &e, 10);
		if (*e)
			return EC_ERROR_PARAM3;

		write_data[i-3] = tmp;
	}

	flash_physical_write(off, size, write_data);

	flash_physical_read(16 * (off / 16), 16*((size / 16) + 1));

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(flashwrite2, command_write_flash,
			"addr size data",
			"Write internal flash",
			NULL);

static int command_erase_flash(int argc, char **argv)
{
	int off, size;
	char *e;

	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;

	off = strtoi(argv[1], &e, 16);
	if (*e)
		return EC_ERROR_PARAM1;

	size = strtoi(argv[2], &e, 10);
	if (*e)
		return EC_ERROR_PARAM2;

	flash_physical_erase(off, size);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(flasherase2, command_erase_flash,
			"addr size",
			"Erase internal flash",
			NULL);

static int command_print_flash_header(int argc, char **argv)
{
	flash_physical_read(0x2ffc8, 56);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(flashheader2, command_print_flash_header,
			"",
			"Print flash header",
			NULL);

static int command_flash_rescan(int argc, char **argv)
{
	eflash_rescan_signature();

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(flashrescan2, command_flash_rescan,
			"",
			"Flash rescan",
			NULL);

static int command_flash_smfi(int argc, char **argv)
{
	int i = 0, j = 0;

	for (i = 0; i < 16; i++) {
		ccprintf("%02x: ", IT83XX_SMFI_BASE + 16*i);

		for (j = 0; j < 16; j++)
			ccprintf("%02x ", REG8(IT83XX_SMFI_BASE + 16*i + j));

		ccprintf("\n");
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(smfi, command_flash_smfi,
			"",
			"SMFI",
			NULL);
