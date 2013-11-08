/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ITE83xx SoC in-system programming tool
 */

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "itecommon.h"

/* Embedded flash page size */
#define PAGE_SIZE		256

/* Embedded flash block write size */
#define BLOCK_WRITE_SIZE	65536

/* Embedded flash number of pages in a sector erase */
#define SECTOR_ERASE_PAGES	4

/* JEDEC SPI Flash commands */
#define SPI_CMD_PAGE_PROGRAM	0x02
#define SPI_CMD_WRITE_DISABLE	0x04
#define SPI_CMD_READ_STATUS	0x05
#define SPI_CMD_WRITE_ENABLE	0x06
#define SPI_CMD_FAST_READ	0x0B
#define SPI_CMD_CHIP_ERASE	0xC7
#define SPI_CMD_SECTOR_ERASE	0xD7
#define SPI_CMD_WORD_PROGRAM	0xAD

/* store custom parameters */
const char *input_filename;
const char *output_filename;
static int flash_size;

/* optional command flags */
enum {
	FLAG_UNPROTECT      = 0x01,
	FLAG_ERASE          = 0x02,
};

/* SPI Flash generic command */
static int spi_flash_command(void *ftdi, uint8_t cmd)
{
	int ret = 0;

	ret |= i2c_write_byte(ftdi, 0x07, 0x7f);
	ret |= i2c_write_byte(ftdi, 0x06, 0xff);
	ret |= i2c_write_byte(ftdi, 0x05, 0xfe);
	ret |= i2c_write_byte(ftdi, 0x04, 0x00);
	ret |= i2c_write_byte(ftdi, 0x08, 0x00);
	ret |= i2c_write_byte(ftdi, 0x05, 0xfd);
	ret |= i2c_write_byte(ftdi, 0x08, cmd);

	return ret ? -EIO : 0;
}

/* SPI Flash generic command, short version */
static int spi_flash_command_short(void *ftdi, uint8_t cmd)
{
	int ret = 0;

	ret |= i2c_write_byte(ftdi, 0x05, 0xfe);
	ret |= i2c_write_byte(ftdi, 0x08, 0x00);
	ret |= i2c_write_byte(ftdi, 0x05, 0xfd);
	ret |= i2c_write_byte(ftdi, 0x08, cmd);

	return ret ? -EIO : 0;
}

/* SPI Flash erase preamble. What is this for? Why is it needed? */
static int spi_flash_erase_preamble(void *ftdi)
{
	int ret = 0;

	/* What do these do? */
	ret |= spi_flash_command(ftdi, 0x50);
	ret |= spi_flash_command_short(ftdi, 0x01);
	ret |= i2c_write_byte(ftdi, 0x08, 0x00);

	return ret ? -EIO : 0;
}

/* SPI Flash set erase page */
static int spi_flash_set_erase_page(void *ftdi, int page)
{
	int ret = 0;

	ret |= i2c_write_byte(ftdi, 0x08, page >> 8);
	ret |= i2c_write_byte(ftdi, 0x08, page & 0xff);
	ret |= i2c_write_byte(ftdi, 0x08, 0);

	return ret ? -EIO : 0;
}

/* Poll SPI Flash Read Status register until BUSY is reset */
static int spi_poll_busy(void *ftdi)
{
	uint8_t reg = 0xff;
	int ret;

	ret = spi_flash_command_short(ftdi, SPI_CMD_READ_STATUS);
	if (ret < 0)
		return ret;

	while (1) {
		ret = i2c_byte_transfer(ftdi, I2C_DATA_ADDR, &reg, 0, 1);
		if (ret < 0)
			return ret;

		if ((reg & 0x01) == 0)
			break;
	}
	return 0;
}

static int windex;
static const char wheel[] = {'|', '/', '-', '\\' };
static void draw_spinner(uint32_t remaining, uint32_t size)
{
	int percent = (size - remaining)*100/size;
	printf("\r%c%3d%%", wheel[windex++], percent);
	windex %= sizeof(wheel);
}

int command_read_pages(void *ftdi, uint32_t address,
		       uint32_t size, uint8_t *buffer)
{
	int res;
	uint32_t remaining = size;
	int cnt;
	uint16_t page;

	while (remaining) {
		uint8_t cmd = 0x9;

		cnt = (remaining > PAGE_SIZE) ? PAGE_SIZE : remaining;
		page = address / PAGE_SIZE;

		draw_spinner(remaining, size);
		/* Fast Read command */
		res = spi_flash_command(ftdi, SPI_CMD_FAST_READ);
		if (res < 0)
			goto failed_read;
		res = i2c_write_byte(ftdi, 0x08, page >> 8);
		res += i2c_write_byte(ftdi, 0x08, page & 0xff);
		res += i2c_write_byte(ftdi, 0x08, 0x00);
		res += i2c_write_byte(ftdi, 0x08, 0x00);
		if (res < 0) {
			fprintf(stderr, "page address set failed\n");
			goto failed_read;
		}

		/* read page data */
		res = i2c_byte_transfer(ftdi, I2C_CMD_ADDR, &cmd, 1, 1);
		res = i2c_byte_transfer(ftdi, I2C_BLOCK_ADDR, buffer, 0, cnt);
		if (res < 0) {
			fprintf(stderr, "page data read failed\n");
			goto failed_read;
		}

		address += cnt;
		remaining -= cnt;
		buffer += cnt;
	}
	res = size;

failed_read:

	return res;
}

int command_write_pages(void *ftdi, uint32_t address,
			uint32_t size, uint8_t *buffer)
{
	int res;
	uint32_t remaining = size;
	int cnt;
	uint8_t page;
	uint8_t cmd;

	while (remaining) {
		cnt = (remaining > BLOCK_WRITE_SIZE) ?
				BLOCK_WRITE_SIZE : remaining;
		page = address / BLOCK_WRITE_SIZE;

		draw_spinner(remaining, size);

		/* Preamble */
		res = spi_flash_erase_preamble(ftdi);
		if (res < 0) {
			fprintf(stderr, "Flash erase preamble FAILED (%d)\n",
					res);
			goto failed_write;
		}

		/* Write enable */
		res = spi_flash_command_short(ftdi, SPI_CMD_WRITE_ENABLE);
		if (res < 0) {
			fprintf(stderr, "Flash write enable FAILED (%d)\n",
					res);
			goto failed_write;
		}

		/* Setup write */
		res = spi_flash_command_short(ftdi, SPI_CMD_WORD_PROGRAM);
		if (res < 0) {
			fprintf(stderr, "Flash setup write FAILED (%d)\n",
					res);
			goto failed_write;
		}

		/* Set page */
		cmd = 0;
		res = i2c_byte_transfer(ftdi, I2C_DATA_ADDR, &page, 1, 1);
		res |= i2c_byte_transfer(ftdi, I2C_DATA_ADDR, &cmd, 1, 1);
		res |= i2c_byte_transfer(ftdi, I2C_DATA_ADDR, &cmd, 1, 1);
		if (res < 0) {
			fprintf(stderr, "Flash write set page FAILED (%d)\n",
					res);
			goto failed_write;
		}

		/* Wait until not busy */
		res = spi_poll_busy(ftdi);
		if (res < 0) {
			fprintf(stderr, "Flash write polling FAILED (%d)\n",
					res);
			goto failed_write;
		}

		/* Write up to BLOCK_WRITE_SIZE data */
		res = i2c_write_byte(ftdi, 0x10, 0x20);
		res = i2c_byte_transfer(ftdi, I2C_BLOCK_ADDR, buffer, 1, cnt);
		buffer += cnt;

		if (res < 0) {
			fprintf(stderr, "Flash data write failed\n");
			goto failed_write;
		}

		cmd = 0xff;
		res = i2c_byte_transfer(ftdi, I2C_DATA_ADDR, &cmd, 1, 1);
		res |= i2c_write_byte(ftdi, 0x10, 0x00);
		if (res < 0) {
			fprintf(stderr, "Flash end data write FAILED (%d)\n",
					res);
			goto failed_write;
		}

		/* Write disable */
		res = spi_flash_command_short(ftdi, SPI_CMD_WRITE_DISABLE);
		if (res < 0) {
			fprintf(stderr, "Flash write disable FAILED (%d)\n",
					res);
			goto failed_write;
		}

		/* Wait until available */
		res = spi_poll_busy(ftdi);
		if (res < 0) {
			fprintf(stderr, "Flash write polling FAILED (%d)\n",
					res);
			goto failed_write;
		}

		address += cnt;
		remaining -= cnt;
	}

	res = size;

failed_write:
	if (spi_flash_command_short(ftdi, SPI_CMD_WRITE_DISABLE) < 0)
		fprintf(stderr, "Flash write disable FAILED\n");

	return res;
}

int command_write_unprotect(void *ftdi)
{
	/* TODO(http://crosbug.com/p/23576): implement me */
	return 0;
}

int command_erase(void *ftdi, uint32_t len, uint32_t off)
{
	int res = 0;
	int page = SECTOR_ERASE_PAGES - 1;
	uint32_t remaining = len;

	printf("Erasing chip...\n");

	if (off != 0 || len != flash_size) {
		fprintf(stderr, "Only full chip erase is supported\n");
		return -EINVAL;
	}

	while (remaining) {
		draw_spinner(remaining, len);

		res = spi_flash_erase_preamble(ftdi);
		if (res < 0) {
			fprintf(stderr, "Flash erase preamble FAILED (%d)\n",
					res);
			goto failed_erase;
		}

		res = spi_flash_command_short(ftdi, SPI_CMD_WRITE_ENABLE);
		if (res < 0) {
			fprintf(stderr, "Flash write enable FAILED (%d)\n",
					res);
			goto failed_erase;
		}

		res = spi_flash_command_short(ftdi, SPI_CMD_SECTOR_ERASE);
		if (res < 0) {
			fprintf(stderr, "Flash erase setup FAILED (%d)\n",
					res);
			goto failed_erase;
		}

		res = spi_flash_set_erase_page(ftdi, page);
		if (res < 0) {
			fprintf(stderr, "Flash sector erase FAILED (%d)\n",
					res);
			goto failed_erase;
		}

		res = spi_poll_busy(ftdi);
		if (res < 0) {
			fprintf(stderr, "Flash BUSY polling FAILED (%d)\n",
					res);
			goto failed_erase;
		}

		if (spi_flash_command_short(ftdi, SPI_CMD_WRITE_DISABLE) < 0) {
			fprintf(stderr, "Flash write disable FAILED\n");
			goto failed_erase;
		}

		page += SECTOR_ERASE_PAGES;
		remaining -= SECTOR_ERASE_PAGES * PAGE_SIZE;
	}

failed_erase:
	if (spi_flash_command_short(ftdi, SPI_CMD_WRITE_DISABLE) < 0)
		fprintf(stderr, "Flash write disable FAILED\n");

	printf("\n");

	return res;
}

/* Return zero on success, a negative error value on failures. */
int read_flash(void *ftdi, const char *filename, uint32_t offset, uint32_t size)
{
	int res;
	FILE *hnd;
	uint8_t *buffer = malloc(size);

	if (!buffer) {
		fprintf(stderr, "Cannot allocate %d bytes\n", size);
		return -ENOMEM;
	}

	hnd = fopen(filename, "w");
	if (!hnd) {
		fprintf(stderr, "Cannot open file %s for writing\n", filename);
		free(buffer);
		return -EIO;
	}

	if (!size)
		size = flash_size;
	printf("Reading %d bytes at 0x%08x\n", size, offset);
	res = command_read_pages(ftdi, offset, size, buffer);
	if (res > 0) {
		if (fwrite(buffer, res, 1, hnd) != 1)
			fprintf(stderr, "Cannot write %s\n", filename);
	}
	printf("\r   %d bytes read.\n", res);

	fclose(hnd);
	free(buffer);
	return (res < 0) ? res : 0;
}

/* Return zero on success, a negative error value on failures. */
int write_flash(void *ftdi, const char *filename, uint32_t offset)
{
	int res, written;
	FILE *hnd;
	int size = flash_size;
	uint8_t *buffer = malloc(size);

	if (!buffer) {
		fprintf(stderr, "Cannot allocate %d bytes\n", size);
		return -ENOMEM;
	}

	hnd = fopen(filename, "r");
	if (!hnd) {
		fprintf(stderr, "Cannot open file %s for reading\n", filename);
		free(buffer);
		return -EIO;
	}
	res = fread(buffer, 1, size, hnd);
	if (res <= 0) {
		fprintf(stderr, "Cannot read %s\n", filename);
		free(buffer);
		return -EIO;
	}
	fclose(hnd);

	printf("Writing %d bytes at 0x%08x\n", res, offset);
	written = command_write_pages(ftdi, offset, res, buffer);
	if (written != res) {
		fprintf(stderr, "Error writing to flash\n");
		free(buffer);
		return -EIO;
	}
	printf("\rDone.\n");

	free(buffer);
	return 0;
}

static const struct option longopts[] = {
	COMMON_CMD_LONGOPTS,
	{"read", 1, 0, 'r'},
	{"write", 1, 0, 'w'},
	{"erase", 0, 0, 'e'},
	{"unprotect", 0, 0, 'u'},
	{NULL, 0, 0, 0}
};

void display_usage(char *program)
{
	fprintf(stderr, "Usage: %s [-d] [-v <VID>] [-p <PID>] [-i <1|2>] "
		"[-s <serial>] [-u] [-e] [-r <file>] [-w <file>]\n",
		program);
	display_common_usage();
	fprintf(stderr, "--u[nprotect] : remove flash write protect\n");
	fprintf(stderr, "--e[rase] : erase all the flash content\n");
	fprintf(stderr, "--r[ead] <file> : read the flash content and "
			"write it into <file>\n");
	fprintf(stderr, "--w[rite] <file> : read <file> and "
			"write it to flash\n");

	exit(2);
}

int parse_parameters(int argc, char **argv)
{
	int opt, idx;
	int flags = 0;

	while ((opt = getopt_long(argc, argv, COMMON_CMD_FLAGS"er:w:u",
				  longopts, &idx)) != -1) {
		if (!parse_common_arg(opt, argv[0]))
			switch (opt) {
			case 'e':
				flags |= FLAG_ERASE;
				break;
			case 'r':
				input_filename = optarg;
				break;
			case 'w':
				output_filename = optarg;
				break;
			case 'u':
				flags |= FLAG_UNPROTECT;
				break;
			}
	}
	return flags;
}

int tool_main(void *hnd, int flags)
{
	int ret = 1;
	int chipid;

	chipid = check_chipid(hnd);
	if (chipid < 0)
		goto terminate;
	/* compute embedded flash size from CHIPVER field */
	flash_size = (128 + (chipid & 0xF0)) * 1024;

	if (flags & FLAG_UNPROTECT)
		command_write_unprotect(hnd);

	if (flags & FLAG_ERASE || output_filename)
		command_erase(hnd, flash_size, 0);

	if (input_filename) {
		ret = read_flash(hnd, input_filename, 0, flash_size);
		if (ret)
			goto terminate;
	}

	if (output_filename) {
		ret = write_flash(hnd, output_filename, 0);
		if (ret)
			goto terminate;
	}

	/* Normal exit */
	ret = 0;
terminate:
	return ret;
}
