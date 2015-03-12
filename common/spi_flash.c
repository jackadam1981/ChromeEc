/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * SPI flash driver for Chrome EC.
 */

#include "common.h"
#include "console.h"
#include "shared_mem.h"
#include "spi.h"
#include "spi_flash.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

/*
 * Time to sleep when chip is busy
 */
#define SPI_FLASH_SLEEP_USEC	100

/*
 * This is the max time for 32kb flash erase
 */
#define SPI_FLASH_TIMEOUT_USEC	(800*MSEC)

/*
 * Common registers for SPI flash. All registers may not be valid for
 * all parts.
 */
#define SPI_FLASH_SR2_SUS		(1 << 7)
#define SPI_FLASH_SR2_CMP		(1 << 6)
#define SPI_FLASH_SR2_LB3		(1 << 5)
#define SPI_FLASH_SR2_LB2		(1 << 4)
#define SPI_FLASH_SR2_LB1		(1 << 3)
#define SPI_FLASH_SR2_QE		(1 << 1)
#define SPI_FLASH_SR2_SRP1		(1 << 0)
#define SPI_FLASH_SR1_SRP0		(1 << 7)
#define SPI_FLASH_SR1_SEC		(1 << 6)
#define SPI_FLASH_SR1_TB		(1 << 5)
#define SPI_FLASH_SR1_BP2		(1 << 4)
#define SPI_FLASH_SR1_BP1		(1 << 3)
#define SPI_FLASH_SR1_BP0		(1 << 2)
#define SPI_FLASH_SR1_WEL		(1 << 1)
#define SPI_FLASH_SR1_BUSY		(1 << 0)

/* Internal buffer used by SPI flash driver */
static uint8_t buf[SPI_FLASH_MAX_MESSAGE_SIZE];

/* Part-specific flags */
enum spi_flags {
	SPI_FLAG_HAS_SR1 = 0x1,
	SPI_FLAG_HAS_SR2 = 0x2,
};

/* Bit state for protect range table */
enum bit_state {
	OFF = 0,
	ON = 1,
	X = -1, /* Don't care */
};

/* Compare macro for (x =? b) for 'X' comparison */
#define COMPARE_BIT(a, b) ((a) != X && (a) != (b))
/* Assignment macro where 'X' = 0 */
#define GET_BIT(a) ((a) == X ? 0 : (a))

struct protect_range {
	enum bit_state cmp;
	enum bit_state sec;
	enum bit_state tb;
	enum bit_state bp[3];    /* Ordered {BP2, BP1, BP0} */
	uint32_t protect_start;
	uint32_t protect_len;
};

/*
 * Define flags and protect table for each SPI ROM part. It's not necessary
 * to define all ranges in the datasheet since we'll usually protect only
 * none or half of the ROM. The table is searched sequentially, so ordering
 * according to likely configurations improves performance slightly.
 */
#ifdef CONFIG_SPI_FLASH_W25X40
static const uint32_t spi_flash_flags = SPI_FLAG_HAS_SR1;
static const struct protect_range spi_flash_protect_ranges[] = {
	{ X, X, X, { 0, 0, 0 }, 0, 0 },       /* No protection */
	{ X, X, 1, { 0, 1, 1 }, 0, 0x40000 }, /* Lower 1/2 */
	{ X, X, 1, { 0, 0, 1 }, 0, 0x10000 }, /* Lower 1/8 */
	{ X, X, 1, { 0, 1, 0 }, 0, 0x20000 }, /* Lower 1/4 */
	{ X, X, X, { 1, X, X }, 0, 0x80000 }, /* All protected */
};

#elif defined(CONFIG_SPI_FLASH_W25Q64)
static const uint32_t spi_flash_flags = SPI_FLAG_HAS_SR1 |
					SPI_FLAG_HAS_SR2;
static const struct protect_range spi_flash_protect_ranges[] = {
	{ 0, X, X, { 0, 0, 0 }, 0, 0 },        /* No protection */
	{ 0, 0, 1, { 1, 1, 0 }, 0, 0x400000 }, /* Lower 1/2 */
	{ 0, 1, 1, { 1, 0, X }, 0, 0x008000 }, /* Lower 1/256 */
	{ 0, 0, 1, { 0, 0, 1 }, 0, 0x020000 }, /* Lower 1/64 */
	{ 0, 0, 1, { 0, 1, 0 }, 0, 0x040000 }, /* Lower 1/32 */
	{ 0, 0, 1, { 0, 1, 1 }, 0, 0x080000 }, /* Lower 1/16 */
	{ 0, 0, 1, { 1, 0, 0 }, 0, 0x100000 }, /* Lower 1/8 */
	{ 0, 0, 1, { 1, 0, 1 }, 0, 0x200000 }, /* Lower 1/4 */
	{ 0, X, X, { 1, 1, 1 }, 0, 0x800000 }, /* All protected */
};
#endif

/**
 * Computes block write protection range from registers
 * Returns start == len == 0 for no protection
 *
 * @param sr1 Status register 1
 * @param sr2 Status register 2
 * @param start Output pointer for protection start offset
 * @param len Output pointer for protection length
 *
 * @return EC_SUCCESS, or non-zero if any error.
 */
static int reg_to_protect(uint8_t sr1, uint8_t sr2, unsigned int *start,
	unsigned int *len)
{
	const struct protect_range *range;
	int i;
	uint8_t cmp;
	uint8_t sec;
	uint8_t tb;
	uint8_t bp;

	/* Determine flags */
	cmp = (sr2 & SPI_FLASH_SR2_CMP) ? 1 : 0;
	sec = (sr1 & SPI_FLASH_SR1_SEC) ? 1 : 0;
	tb = (sr1 & SPI_FLASH_SR1_TB) ? 1 : 0;
	bp = (sr1 & (SPI_FLASH_SR1_BP2 | SPI_FLASH_SR1_BP1 | SPI_FLASH_SR1_BP0))
		 >> 2;

	/* Bad pointers or invalid data */
	if (!start || !len || sr1 == -1 || sr2 == -1)
		return EC_ERROR_INVAL;

	for (i = 0; i < ARRAY_SIZE(spi_flash_protect_ranges); ++i) {
		range = &spi_flash_protect_ranges[i];
		if (COMPARE_BIT(range->cmp, cmp))
			continue;
		if (COMPARE_BIT(range->sec, sec))
			continue;
		if (COMPARE_BIT(range->tb, tb))
			continue;
		if (COMPARE_BIT(range->bp[0], bp & 0x4))
			continue;
		if (COMPARE_BIT(range->bp[1], bp & 0x2))
			continue;
		if (COMPARE_BIT(range->bp[2], bp & 0x1))
			continue;

		*start = range->protect_start;
		*len = range->protect_len;
		return EC_SUCCESS;
	}

	/* Invalid range, or valid range missing from our table */
	return EC_ERROR_INVAL;
}

/**
 * Computes block write protection registers from range
 *
 * @param start Desired protection start offset
 * @param len Desired protection length
 * @param sr1 Output pointer for status register 1
 * @param sr2 Output pointer for status register 2
 *
 * @return EC_SUCCESS, or non-zero if any error.
 */
static int protect_to_reg(unsigned int start, unsigned int len,
	uint8_t *sr1, uint8_t *sr2)
{
	const struct protect_range *range;
	int i;
	char cmp = 0;
	char sec = 0;
	char tb = 0;
	char bp = 0;

	/* Bad pointers */
	if (!sr1 || !sr2 || *sr1 == -1 || *sr2 == -1)
		return EC_ERROR_INVAL;

	/* Invalid data */
	if ((start && !len) || start + len > CONFIG_SPI_FLASH_SIZE)
		return EC_ERROR_INVAL;

	for (i = 0; i < ARRAY_SIZE(spi_flash_protect_ranges); ++i) {
		range = &spi_flash_protect_ranges[i];
		if (range->protect_start == start &&
		    range->protect_len == len) {
			cmp = GET_BIT(range->cmp);
			sec = GET_BIT(range->sec);
			tb = GET_BIT(range->tb);
			bp = GET_BIT(range->bp[0]) << 2 |
			     GET_BIT(range->bp[1]) << 1 |
			     GET_BIT(range->bp[2]);

			*sr1 = (sec ? SPI_FLASH_SR1_SEC : 0) |
			       (tb ? SPI_FLASH_SR1_TB : 0) |
			       (bp << 2);
			*sr2 = (cmp ? SPI_FLASH_SR2_CMP : 0);
			return EC_SUCCESS;
		}
	}

	/* Invalid range, or valid range missing from our table */
	return EC_ERROR_INVAL;
}

/**
 * Waits for chip to finish current operation. Must be called after
 * erase/write operations to ensure successive commands are executed.
 *
 * @return EC_SUCCESS or error on timeout
 */
int spi_flash_wait(void)
{
	timestamp_t timeout;

	timeout.val = get_time().val + SPI_FLASH_TIMEOUT_USEC;
	/* Wait until chip is not busy */
	while (spi_flash_get_status1() & SPI_FLASH_SR1_BUSY) {
		usleep(SPI_FLASH_SLEEP_USEC);

		if (get_time().val > timeout.val)
			return EC_ERROR_TIMEOUT;
	}

	return EC_SUCCESS;
}

/**
 * Set the write enable latch
 */
static int spi_flash_write_enable(void)
{
	uint8_t cmd = SPI_FLASH_WRITE_ENABLE;
	return spi_transaction(&cmd, 1, NULL, 0);
}

/**
 * Returns the contents of SPI flash status register 1
 * @return register contents or -1 on error
 */
uint8_t spi_flash_get_status1(void)
{
	uint8_t cmd = SPI_FLASH_READ_SR1;
	uint8_t resp;

	if (spi_transaction(&cmd, 1, &resp, 1) != EC_SUCCESS)
		return -1;

	return resp;
}

/**
 * Returns the contents of SPI flash status register 2
 * @return register contents or -1 on error
 */
uint8_t spi_flash_get_status2(void)
{
	uint8_t cmd = SPI_FLASH_READ_SR2;
	uint8_t resp;

	/* Second status register not present? */
	if (!(spi_flash_flags & SPI_FLAG_HAS_SR2))
		return 0;

	if (spi_transaction(&cmd, 1, &resp, 1) != EC_SUCCESS)
		return -1;

	return resp;
}

/**
 * Sets the SPI flash status registers (non-volatile bits only)
 * Pass reg2 == -1 to only set reg1.
 *
 * @param reg1 Status register 1
 * @param reg2 Status register 2 (optional)
 *
 * @return EC_SUCCESS, or non-zero if any error.
 */
int spi_flash_set_status(int reg1, int reg2)
{
	uint8_t cmd[3] = {SPI_FLASH_WRITE_SR, reg1, reg2};
	int rv = EC_SUCCESS;

	/* Register has protection */
	if (spi_flash_check_wp() != SPI_WP_NONE)
		return EC_ERROR_ACCESS_DENIED;

	/* Enable writing to SPI flash */
	rv = spi_flash_write_enable();
	if (rv)
		return rv;

	if (reg2 == -1 || !(spi_flash_flags & SPI_FLAG_HAS_SR2))
		rv = spi_transaction(cmd, 2, NULL, 0);
	else
		rv = spi_transaction(cmd, 3, NULL, 0);
	if (rv)
		return rv;

	return rv;
}

/**
 * Returns the content of SPI flash
 *
 * @param buf Buffer to write flash contents
 * @param offset Flash offset to start reading from
 * @param bytes Number of bytes to read. Limited by receive buffer to 256.
 *
 * @return EC_SUCCESS, or non-zero if any error.
 */
int spi_flash_read(uint8_t *buf_usr, unsigned int offset, unsigned int bytes)
{
	uint8_t cmd[4] = {SPI_FLASH_READ,
			  (offset >> 16) & 0xFF,
			  (offset >> 8) & 0xFF,
			  offset & 0xFF};

	if (offset + bytes > CONFIG_SPI_FLASH_SIZE)
		return EC_ERROR_INVAL;

	return spi_transaction(cmd, 4, buf_usr, bytes);
}

/**
 * Erase a block of SPI flash.
 *
 * @param offset Flash offset to start erasing
 * @param block Block size in kb (4 or 32)
 *
 * @return EC_SUCCESS, or non-zero if any error.
 */
static int spi_flash_erase_block(unsigned int offset, unsigned int block)
{
	uint8_t cmd[4];
	int rv = EC_SUCCESS;

	/* Invalid block size */
	if (block != 4 && block != 32)
		return EC_ERROR_INVAL;

	/* Not block aligned */
	if ((offset % (block * 1024)) != 0)
		return EC_ERROR_INVAL;

	/* Wait for previous operation to complete */
	rv = spi_flash_wait();
	if (rv)
		return rv;

	/* Enable writing to SPI flash */
	rv = spi_flash_write_enable();
	if (rv)
		return rv;

	/* Compose instruction */
	cmd[0] = (block == 4) ? SPI_FLASH_ERASE_4KB : SPI_FLASH_ERASE_32KB;
	cmd[1] = (offset >> 16) & 0xFF;
	cmd[2] = (offset >> 8) & 0xFF;
	cmd[3] = offset & 0xFF;

	rv = spi_transaction(cmd, 4, NULL, 0);
	if (rv)
		return rv;

	return rv;
}

/**
 * Erase SPI flash.
 *
 * @param offset Flash offset to start erasing
 * @param bytes Number of bytes to erase
 *
 * @return EC_SUCCESS, or non-zero if any error.
 */
int spi_flash_erase(unsigned int offset, unsigned int bytes)
{
	int rv = EC_SUCCESS;

	/* Invalid input */
	if (offset + bytes > CONFIG_SPI_FLASH_SIZE)
		return EC_ERROR_INVAL;

	/* Not aligned to sector (4kb) */
	if (offset % 4096 || bytes % 4096)
		return EC_ERROR_INVAL;

	/* Largest unit is block (32kb) */
	if (offset % (32 * 1024) == 0) {
		while (bytes != (bytes % (32 * 1024))) {
			rv = spi_flash_erase_block(offset, 32);
			if (rv)
				return rv;

			bytes -= 32 * 1024;
			offset += 32 * 1024;
		}
	}

	/* Largest unit is sector (4kb) */
	while (bytes != (bytes % (4 * 1024))) {
		rv = spi_flash_erase_block(offset, 4);
		if (rv)
			return rv;

		bytes -= 4 * 1024;
		offset += 4 * 1024;
	}

	return rv;
}

/**
 * Write to SPI flash. Assumes already erased.
 * Limited to SPI_FLASH_MAX_WRITE_SIZE by chip.
 *
 * @param offset Flash offset to write
 * @param bytes Number of bytes to write
 * @param data Data to write to flash
 *
 * @return EC_SUCCESS, or non-zero if any error.
 */
int spi_flash_write(unsigned int offset, unsigned int bytes,
	const uint8_t const *data)
{
	int rv;

	/* Invalid input */
	if (!data || offset + bytes > CONFIG_SPI_FLASH_SIZE ||
	    bytes > SPI_FLASH_MAX_WRITE_SIZE)
		return EC_ERROR_INVAL;

	/* Enable writing to SPI flash */
	rv = spi_flash_write_enable();
	if (rv)
		return rv;

	/* Copy data to send buffer; buffers may overlap */
	memmove(buf + 4, data, bytes);

	/* Compose instruction */
	buf[0] = SPI_FLASH_PAGE_PRGRM;
	buf[1] = (offset >> 16) & 0xFF;
	buf[2] = (offset >> 8) & 0xFF;
	buf[3] = offset & 0xFF;

	return spi_transaction(buf, 4 + bytes, NULL, 0);
}

/**
 * Returns the SPI flash JEDEC ID (manufacturer ID, memory type, and capacity)
 *
 * @return flash JEDEC ID or -1 on error
 */
uint32_t spi_flash_get_jedec_id(void)
{
	uint8_t cmd = SPI_FLASH_JEDEC_ID;
	uint32_t resp;

	if (spi_transaction(&cmd, 1, (uint8_t *)&resp, 4) != EC_SUCCESS)
		return -1;

	return resp;
}

/**
 * Returns the SPI flash unique ID (serial)
 *
 * @return flash unique ID or -1 on error
 */
uint64_t spi_flash_get_unique_id(void)
{
	uint8_t cmd[5] = {SPI_FLASH_UNIQUE_ID, 0, 0, 0, 0};
	uint64_t resp;

	if (spi_transaction(cmd, 5, (uint8_t *)&resp, 8) != EC_SUCCESS)
		return -1;

	return resp;
}

/**
 * Check for SPI flash status register write protection
 * Cannot sample WP pin, so caller should sample it if necessary, if
 * SPI_WP_HARDWARE is returned.
 *
 * @return enum spi_flash_wp status based on protection
 */
enum spi_flash_wp spi_flash_check_wp(void)
{
	int sr1_prot = spi_flash_get_status1() & SPI_FLASH_SR1_SRP0;
	int sr2_prot = spi_flash_get_status2() & SPI_FLASH_SR2_SRP1;

	if (sr2_prot)
		return sr1_prot ? SPI_WP_PERMANENT : SPI_WP_POWER_CYCLE;
	else if (sr1_prot)
		return SPI_WP_HARDWARE;

	return SPI_WP_NONE;
}

/**
 * Set SPI flash status register write protection
 *
 * @param wp Status register write protection mode
 *
 * @return EC_SUCCESS for no protection, or non-zero if error.
 */
int spi_flash_set_wp(enum spi_flash_wp w)
{
	int sr1 = spi_flash_get_status1();
	int sr2 = spi_flash_get_status2();

	switch (w) {
	case SPI_WP_NONE:
		sr1 &= ~SPI_FLASH_SR1_SRP0;
		sr2 &= ~SPI_FLASH_SR2_SRP1;
		break;
	case SPI_WP_HARDWARE:
		sr1 |= SPI_FLASH_SR1_SRP0;
		sr2 &= ~SPI_FLASH_SR2_SRP1;
		break;
	case SPI_WP_POWER_CYCLE:
		sr1 &= ~SPI_FLASH_SR1_SRP0;
		sr2 |= SPI_FLASH_SR2_SRP1;
		break;
	case SPI_WP_PERMANENT:
		sr1 |= SPI_FLASH_SR1_SRP0;
		sr2 |= SPI_FLASH_SR2_SRP1;
		break;
	default:
		return EC_ERROR_INVAL;
	}

	return spi_flash_set_status(sr1, sr2);
}

/**
 * Check for SPI flash block write protection
 *
 * @param offset Flash block offset to check
 * @param bytes Flash block length to check
 *
 * @return EC_SUCCESS for no protection, or non-zero if error.
 */
int spi_flash_check_protect(unsigned int offset, unsigned int bytes)
{
	uint8_t sr1 = spi_flash_get_status1();
	uint8_t sr2 = spi_flash_get_status2();
	unsigned int start;
	unsigned int len;
	int rv = EC_SUCCESS;

	/* Invalid value */
	if (sr1 == -1 || sr2 == -1 || offset + bytes > CONFIG_SPI_FLASH_SIZE)
		return EC_ERROR_INVAL;

	/* Compute current protect range */
	rv = reg_to_protect(sr1, sr2, &start, &len);
	if (rv)
		return rv;

	/* Check if ranges overlap */
	if (MAX(start, offset) < MIN(start + len, offset + bytes))
		return EC_ERROR_ACCESS_DENIED;

	return EC_SUCCESS;
}

/**
 * Set SPI flash block write protection
 * If offset == bytes == 0, remove protection.
 *
 * @param offset Flash block offset to protect
 * @param bytes Flash block length to protect
 *
 * @return EC_SUCCESS, or non-zero if error.
 */
int spi_flash_set_protect(unsigned int offset, unsigned int bytes)
{
	int rv;
	uint8_t sr1 = spi_flash_get_status1();
	uint8_t sr2 = spi_flash_get_status2();

	/* Invalid values */
	if (sr1 == -1 || sr2 == -1 || offset + bytes > CONFIG_SPI_FLASH_SIZE)
		return EC_ERROR_INVAL;

	/* Compute desired protect range */
	rv = protect_to_reg(offset, bytes, &sr1, &sr2);
	if (rv)
		return rv;

	return spi_flash_set_status(sr1, sr2);
}

static int command_spi_flashinfo(int argc, char **argv)
{
	uint32_t jedec;
	uint64_t unique;
	int rv;

	spi_enable(1);

	/* Wait for previous operation to complete */
	rv = spi_flash_wait();
	if (rv)
		return rv;

	jedec = spi_flash_get_jedec_id();
	unique = spi_flash_get_unique_id();

	ccprintf("Manufacturer ID: %02x\nDevice ID: %02x %02x\n",
		((uint8_t *)&jedec)[0], ((uint8_t *)&jedec)[1],
		((uint8_t *)&jedec)[2]);
	ccprintf("Unique ID: %02x %02x %02x %02x %02x %02x %02x %02x\n",
		((uint8_t *)&unique)[0], ((uint8_t *)&unique)[1],
		((uint8_t *)&unique)[2], ((uint8_t *)&unique)[3],
		((uint8_t *)&unique)[4], ((uint8_t *)&unique)[5],
		((uint8_t *)&unique)[6], ((uint8_t *)&unique)[7]);
	ccprintf("Capacity: %4d MB\n",
		SPI_FLASH_SIZE(((uint8_t *)&jedec)[2]) / 1024);

	return rv;
}
DECLARE_CONSOLE_COMMAND(spi_flashinfo, command_spi_flashinfo,
	NULL,
	"Print SPI flash info",
	NULL);

#ifdef CONFIG_CMD_SPI_FLASH
static int command_spi_flasherase(int argc, char **argv)
{
	int offset = -1;
	int bytes = 4096;
	int rv = parse_offset_size(argc, argv, 1, &offset, &bytes);

	if (rv)
		return rv;

	spi_enable(1);

	/* Chip has protection */
	if (spi_flash_check_protect(offset, bytes))
		return EC_ERROR_ACCESS_DENIED;

	/* Wait for previous operation to complete */
	rv = spi_flash_wait();
	if (rv)
		return rv;

	ccprintf("Erasing %d bytes at 0x%x...\n", bytes, offset);
	rv = spi_flash_erase(offset, bytes);
	if (rv)
		return rv;

	/* Wait for the operation to complete */
	return spi_flash_wait();
}
DECLARE_CONSOLE_COMMAND(spi_flasherase, command_spi_flasherase,
	"offset [bytes]",
	"Erase flash",
	NULL);

static int command_spi_flashwrite(int argc, char **argv)
{
	int offset = -1;
	int bytes = SPI_FLASH_MAX_WRITE_SIZE;
	int write_len;
	int rv = EC_SUCCESS;
	int i;

	rv = parse_offset_size(argc, argv, 1, &offset, &bytes);
	if (rv)
		return rv;

	spi_enable(1);

	/* Chip has protection */
	if (spi_flash_check_protect(offset, bytes))
		return EC_ERROR_ACCESS_DENIED;

	/* Fill the data buffer with a pattern */
	for (i = 0; i < SPI_FLASH_MAX_WRITE_SIZE; i++)
		buf[i] = i;

	ccprintf("Writing %d bytes to 0x%x...\n", bytes, offset);
	while (bytes > 0) {
		watchdog_reload();

		/* First write multiples of 256, then (bytes % 256) last */
		write_len = ((bytes % SPI_FLASH_MAX_WRITE_SIZE) == bytes) ?
					bytes : SPI_FLASH_MAX_WRITE_SIZE;

		/* Wait for previous operation to complete */
		rv = spi_flash_wait();
		if (rv)
			return rv;

		/* Perform write */
		rv = spi_flash_write(offset, write_len, buf);
		if (rv)
			return rv;

		offset += write_len;
		bytes -= write_len;
	}

	ASSERT(bytes == 0);

	return spi_flash_wait();
}
DECLARE_CONSOLE_COMMAND(spi_flashwrite, command_spi_flashwrite,
	"offset [bytes]",
	"Write pattern to flash",
	NULL);

static int command_spi_flashread(int argc, char **argv)
{
	int i;
	int offset = -1;
	int bytes = -1;
	int read_len;
	int rv;

	rv = parse_offset_size(argc, argv, 1, &offset, &bytes);
	if (rv)
		return rv;

	spi_enable(1);

	/* Can't read past size of memory */
	if (offset + bytes > CONFIG_SPI_FLASH_SIZE)
		return EC_ERROR_INVAL;

	/* Wait for previous operation to complete */
	rv = spi_flash_wait();
	if (rv)
		return rv;

	ccprintf("Reading %d bytes from 0x%x...\n", bytes, offset);
	/* Read <= 256 bytes to avoid allocating another buffer */
	while (bytes > 0) {
		watchdog_reload();

		/* First read (bytes % 256), then in multiples of 256 */
		read_len = (bytes % SPI_FLASH_MAX_READ_SIZE) ?
					(bytes % SPI_FLASH_MAX_READ_SIZE) :
					SPI_FLASH_MAX_READ_SIZE;

		rv = spi_flash_read(buf, offset, read_len);
		if (rv)
			return rv;

		for (i = 0; i < read_len; i++) {
			if (i % 16 == 0)
				ccprintf("%02x:", offset + i);

			ccprintf(" %02x", buf[i]);

			if (i % 16 == 15 || i == read_len - 1)
				ccputs("\n");
		}

		offset += read_len;
		bytes -= read_len;
	}

	ASSERT(bytes == 0);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(spi_flashread, command_spi_flashread,
	"offset bytes",
	"Read flash",
	NULL);

static int command_spi_flashread_sr(int argc, char **argv)
{
	spi_enable(1);

	ccprintf("Status Register 1: 0x%02x\n", spi_flash_get_status1());
	ccprintf("Status Register 2: 0x%02x\n", spi_flash_get_status2());

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(spi_flash_rsr, command_spi_flashread_sr,
	NULL,
	"Read status registers",
	NULL);

static int command_spi_flashwrite_sr(int argc, char **argv)
{
	int val1 = 0;
	int val2 = 0;
	int rv = parse_offset_size(argc, argv, 1, &val1, &val2);

	if (rv)
		return rv;

	spi_enable(1);

	/* Wait for previous operation to complete */
	rv = spi_flash_wait();
	if (rv)
		return rv;

	ccprintf("Writing 0x%02x to status register 1, ", val1);
	ccprintf("0x%02x to status register 2...\n", val2);
	rv = spi_flash_set_status(val1, val2);
	if (rv)
		return rv;

	/* Wait for the operation to complete */
	return spi_flash_wait();
}
DECLARE_CONSOLE_COMMAND(spi_flash_wsr, command_spi_flashwrite_sr,
	"value1 value2",
	"Write to status registers",
	NULL);

static int command_spi_flashprotect(int argc, char **argv)
{
	int val1 = 0;
	int val2 = 0;
	int rv = parse_offset_size(argc, argv, 1, &val1, &val2);

	if (rv)
		return rv;

	spi_enable(1);

	/* Wait for previous operation to complete */
	rv = spi_flash_wait();
	if (rv)
		return rv;

	ccprintf("Setting protection for 0x%06x to 0x%06x\n", val1, val1+val2);
	rv = spi_flash_set_protect(val1, val2);
	if (rv)
		return rv;

	/* Wait for the operation to complete */
	return spi_flash_wait();
}
DECLARE_CONSOLE_COMMAND(spi_flash_prot, command_spi_flashprotect,
	"offset len",
	"Set block protection",
	NULL);
#endif
