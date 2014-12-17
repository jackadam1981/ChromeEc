/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * SPI module for Chrome EC
 * void spi_start_wait(uint32_t inst);
 * void spi_write(uint32_t inst, const uint8_t *data, uint32_t len);
 * void spi_read(uint32_t inst, uint8_t *data, uint32_t len);
 * void spi_length_set(uint32_t inst, uint32_t len);
*/

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "shared_mem.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "pmu.h"

#define G_SPI_FLASH_MAX_SIZE       128

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)


/**
 * @brief Set SPI transaction length
 * @details This is useful for performing reads without having to
 * pre-fill the TX buffers. Len should be <= G_SPI_FLASH_MAX_SIZE.
 */
void spi_length_set(uint32_t inst, uint32_t len)
{
	len -= 1;
	len &= 0x7F;
	GWRITE_FIELD_I(SPI, inst, XACT, SIZE, len);
}


/**
 * @brief Start SPI transfer and wait for completion
 */
void spi_start_wait(uint32_t inst)
{
	GWRITE_FIELD_I(SPI, inst, ISTATE_CLR, TXDONE, 1);
	GWRITE_FIELD_I(SPI, inst, XACT, START, 1);
	while (!GREAD_FIELD_I(SPI, inst, ISTATE_CLR, TXDONE))
		;
}

/**
 * @brief Copy data to the SPI TX buffer
 * @param data Pointer to 8-bit data
 * @param len Length of data
 */
void spi_write(uint32_t inst, const uint8_t *data, uint32_t len)
{
	volatile uint32_t *dest;
	uint32_t tmp;
	uint32_t shift;

	dest = GREG32_ADDR_I(SPI, inst, TX_DATA);

	CPRINTS("spi wr [%X] <-- [%X] len:%d", dest, data, len);

	spi_length_set(inst, len);

	/* unrolled 32-bit copies for better performance */
	while (len >= 4) {
		tmp = (*data++);
		tmp |= (*data++) << 8;
		tmp |= (*data++) << 16;
		tmp |= (*data++) << 24;
		*dest++ = tmp;
		len -= 4;
	}

	/* copy remaining bytes */
	if (len != 0) {
		shift = 0;
		tmp = 0;
		while (len-- > 0) {
			tmp |= (*data++) << shift;
			shift += 8;
		}
		*dest = tmp;
	}
}

/**
 * @brief Read data from the SPI RX buffer
 * @param data Pointer to 8-bit data
 * @param len Length of data
 */
void spi_read(uint32_t inst, uint8_t *data, uint32_t len)
{
	volatile uint32_t *src;
	uint32_t tmp;

	src = GREG32_ADDR_I(SPI, inst, RX_DATA);
	CPRINTS("spi rd [%X] --> [%X] len:%d", src, data, len);

	/* unrolled 32-bit copies for better performance */
	while (len >= 4) {
		tmp = *src++;
		*data++ = tmp;
		*data++ = tmp >> 8;
		*data++ = tmp >> 16;
		*data++ = tmp >> 24;
		len -= 4;
	}

	/* copy remaining bytes */
	if (len != 0) {
		tmp = *src;
		while (len-- > 0) {
			*data++ = tmp;
			tmp >>= 8;
		}
	}
}

void sps_write(uint16_t addr, uint8_t data)
{
	uint8_t buf[3];

	/* C00=0 (write), C01=0 (read/write data), bottom 3 bits of address */
	buf[0] = (0 << 0) | (0 << 1) | ((addr & 0x7) << 5);
	buf[1] = addr >> 3;
	buf[2] = data;

	spi_write(0, buf, sizeof(buf));
	spi_start_wait(0);
}

uint8_t sps_read(uint16_t addr)
{
	uint8_t buf[3];
	/*
	* C00=1 (read), C01=0 (read/write data)
	* bottom 3 bits of address
	*/
	buf[0] = (1 << 0) | (0 << 1) | ((addr & 0x7) << 5);
	buf[1] = addr >> 3;

	/* clocks for reading back data (writes out 0 dummy) */
	buf[2] = 0;

	spi_write(0, buf, sizeof(buf));
	spi_start_wait(0);
	spi_read(0, buf, sizeof(buf));

	return buf[2];
}

#define                             SPI_DATA_SIZE 0x80 /* 0x100 */
#define                             SPS_DATA_SIZE 0x80 /* 0x800 */
int do_sps_rw(uint8_t data)
{
	uint16_t i;
	uint8_t c;
	for (i = 0; i < SPS_DATA_SIZE; i++) {
		sps_write(i, (data == 0xAD) ? (i&0xFF) : data);
		c = sps_read(i);
		if (((data == 0xAD) ? (i&0xFF) : data) != c) {
			CPRINTS("%02X Read/Write mismatch at address %d",
				data, i);
			return i;
		}
	}
	return i;
}

void do_sps_tests(void)
{
	uint8_t data[] = { 0xAD, 0xAA, 0x55, 0x00, 0xFF };
	uint16_t i;
	for (i = 0; i < sizeof(data)/sizeof(data[0]); i++)
		do_sps_rw(data[i]);
}

int spi_enable(int enable)
{
	return EC_SUCCESS;
}


int spi_transaction(const uint8_t *txdata, int txlen,
		    uint8_t *rxdata, int rxlen)
{
	uint32_t inst = 0;
	spi_write(inst, txdata, txlen);
	spi_read(inst, rxdata, txlen);
	return EC_SUCCESS;
}

#if 0
/* Hooks */
static int spi_init(void)
{
	/* init clock */

	/* Ensure the SPI port is disabled.  This keeps us from interfering
	 * with the main chipset when we're not explicitly using the SPI
	 * bus. */
	spi_enable(0);

	return EC_SUCCESS;
}
DECLARE_HOOK(HOOK_INIT, spi_init, HOOK_PRIO_DEFAULT);
#endif

/*
 * SPI ROM (spirom) Console commands
 */
static int printrx(const char *desc, const uint8_t *txdata, int txlen,
		   int rxlen)
{
	uint8_t rxdata[32];
	int rv;
	int i;

	rv = spi_transaction(txdata, txlen, rxdata, rxlen);
	if (rv)
		return rv;

	ccprintf("%-12s:", desc);
	for (i = 0; i < rxlen; i++)
		ccprintf(" 0x%02x", rxdata[i]);
	ccputs("\n");
	return EC_SUCCESS;
}
static int command_spirom(int argc, char **argv)
{
	uint8_t txmandev[] = {0x90, 0x00, 0x00, 0x00};
	uint8_t txjedec[] = {0x9f};
	uint8_t txunique[] = {0x4b, 0x00, 0x00, 0x00, 0x00};
	uint8_t txsr1[] = {0x05};
	uint8_t txsr2[] = {0x35};

	spi_enable(1);

	printrx("Man/Dev ID", txmandev, sizeof(txmandev), 2);
	printrx("JEDEC ID", txjedec, sizeof(txjedec), 3);
	printrx("Unique ID", txunique, sizeof(txunique), 8);
	printrx("Status reg 1", txsr1, sizeof(txsr1), 1);
	printrx("Status reg 2", txsr2, sizeof(txsr2), 1);

	spi_enable(0);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(spirom, command_spirom,
			NULL,
			"Test reading SPI EEPROM",
			NULL);

static int command_spi_rw_test(int argc, char **argv)
{
	/* periph clocks */
	pmu_clock_en(PERIPH_SPI);
	pmu_clock_en(PERIPH_SPS);

	/* Configure SPI master */
	GWRITE_FIELD(SPI, CTRL, CPOL, 0);
	GWRITE_FIELD(SPI, CTRL, CPHA, 0);
	/* CSB to SCK setup time in SCK cycles + 1.5 */
	GWRITE_FIELD(SPI, CTRL, CSBSU, 1);
	/* CSB from SCK hold time in SCK cycles + 1 */
	GWRITE_FIELD(SPI, CTRL, CSBHLD, 1);
	/* SPI clk divider */
	GWRITE_FIELD(SPI, CTRL, IDIV,    7);
	GWRITE_FIELD(SPI, CTRL, TXBITOR, 0); /* LSB first */
	GWRITE_FIELD(SPI, CTRL, RXBITOR, 0); /* LSB first */

	/* Configure SPI slave */
	GWRITE_FIELD(SPS, CTRL, MODE, 1);       /* Swetland mode */
	GWRITE_FIELD(SPS, CTRL, CPHA, 0);       /* CPOL */
	GWRITE_FIELD(SPS, CTRL, CPHA, 0);       /* CPHA */
	GWRITE_FIELD(SPS, CTRL, IDLE_LVL, 0);   /* CSB idle level */
	GWRITE_FIELD(SPS, CTRL, TXBITOR, 0);    /* LSB first */
	GWRITE_FIELD(SPS, CTRL, RXBITOR, 0);    /* LSB first */
	GWRITE_FIELD(SPS, CTRL, ROM_ADDR_SIZE, 3); /* ROM mode addr size */

	spi_enable(1);
	do_sps_tests();
	spi_enable(0);
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(spitest, command_spi_rw_test,
			NULL,
			"Test read/write SPI EEPROM",
			NULL);


static int command_spi_read(int argc, char **argv)
{
	int rv = EC_SUCCESS;
	uint32_t offset = 0;
	uint32_t value, num = 1, i;
	char *e;
	char *buf;
	uint32_t inst = 0;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	offset = (uint32_t)strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	if (argc >= 3)
		num = strtoi(argv[2], &e, 0);

	rv = shared_mem_acquire(G_SPI_FLASH_MAX_SIZE, &buf);
	if (rv != EC_SUCCESS)
		return rv;

	spi_read(inst, buf, MIN(num, G_SPI_FLASH_MAX_SIZE));
	for (i = offset; i < num; i++) {
		value = buf[i];
		if (0 == (i%16))
			ccprintf("\n%08X: %02x", buf+i, value);
		else if (0 == (i%4))
			ccprintf(" %02x", value);
		else
			ccprintf("%02x", value);
		cflush();
	}
	ccprintf("\n");
	cflush();
	shared_mem_release(buf);
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(spird, command_spi_read,
			"addr [num]",
			"read num bytes from SPI flash",
			NULL);

static int command_spi_write(int argc, char **argv)
{
	uint8_t *address;
	uint32_t num;
	char *e;
	uint32_t inst = 0;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	address = (uint8_t *)(uintptr_t)strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	num = strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	ccprintf("write 0x%p, 0x%08x\n", address, num);
	spi_write(inst, address, MIN(num, G_SPI_FLASH_MAX_SIZE));

	cflush();  /* Flush before writing in case this crashes */
	return EC_SUCCESS;

}
DECLARE_CONSOLE_COMMAND(spiwr, command_spi_write,
			"addr num",
			"copy num bytes from [addr] to spi flash",
			NULL);

