/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * SPI flash driver for Chrome EC, particularly fruitpie board with Winbond
 * W25Q64FVZPIG flash memory.
 *
 * This uses DMA to handle transmission and reception.
 */

#include "config.h"
#include "console.h"
#include "dma.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "shared_mem.h"
#include "spi.h"
#include "spi_flash.h"
#include "util.h"
#include "watchdog.h"

/* Default DMA channel options */
static const struct dma_option dma_tx_option = {
	STM32_DMAC_CH7, (void *)&STM32_SPI2_REGS->dr,
	STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT
};

static const struct dma_option dma_rx_option = {
	STM32_DMAC_CH6, (void *)&STM32_SPI2_REGS->dr,
	STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT
};

/*
 * Maximum message size (in bytes) for the W25Q64FV SPI flash
 * Instruction (1) + Address (3) + Data (256) = 260
 */
#define SPI_FLASH_MAX_MESSAGE_SIZE	260

/*
 * Maximum single write size (in bytes) for the W25Q64FV SPI flash
 */
#define SPI_FLASH_MAX_WRITE_SIZE	256

/*
 * Registers for the W25Q64FV SPI flash
 */
#define SPI_FLASH_SR2_SUS			(1 << 15)
#define SPI_FLASH_SR2_CMP			(1 << 14)
#define SPI_FLASH_SR2_LB3			(1 << 13)
#define SPI_FLASH_SR2_LB2			(1 << 12)
#define SPI_FLASH_SR2_LB1			(1 << 11)
#define SPI_FLASH_SR2_QE			(1 << 9)
#define SPI_FLASH_SR2_SRP1			(1 << 8)
#define SPI_FLASH_SR1_SRP0			(1 << 7)
#define SPI_FLASH_SR1_SEC			(1 << 6)
#define SPI_FLASH_SR1_TB			(1 << 5)
#define SPI_FLASH_SR1_BP2			(1 << 4)
#define SPI_FLASH_SR1_BP1			(1 << 3)
#define SPI_FLASH_SR1_BP0			(1 << 2)
#define SPI_FLASH_SR1_WEL			(1 << 1)
#define SPI_FLASH_SR1_BUSY			(1 << 0)

/* Internal buffer used by SPI flash driver */
static uint8_t buf[SPI_FLASH_MAX_MESSAGE_SIZE];
static uint8_t spi_enabled;

/**
 * Sends and receives a message.
 *
 * @param snd_len	Number of message bytes to send
 * @param rcv_len	Number of bytes to receive
 *
 */
static void communicate(int snd_len, int rcv_len)
{
	uint8_t tmp;
	stm32_dma_chan_t *txdma;
	stm32_spi_regs_t *spi = STM32_SPI2_REGS;

	/* Enable SPI if it is disabled */
	if (!spi_enabled)
		spi_enable(1);

	/* Buffer overflow */
	if (snd_len + rcv_len >= SPI_FLASH_MAX_MESSAGE_SIZE)
		return;

	/* Wipe send buffer from snd_len to snd_len + rcv_len */
	memset(buf + snd_len, 0, rcv_len);

	/* Drive SS low */
	gpio_set_level(GPIO_PD_TX_EN, 0);

	/* Clear out the FIFO. Add to prevent overly smart compilers from
	optimizing out. */
	tmp = tmp + spi->dr;

	/* Set up RX DMA */
	dma_start_rx(&dma_rx_option, snd_len + rcv_len, buf);

	/* Set up TX DMA */
	txdma = dma_get_channel(dma_tx_option.channel);
	dma_prepare_tx(&dma_tx_option, snd_len + rcv_len, buf);
	dma_go(txdma);

	/* Wait for DMA transmission to complete */
	dma_wait(dma_tx_option.channel);

	/* Wait for FTLVL[1:0] to indicate FIFO empty */
	while (spi->sr & STM32_SPI_SR_FTLVL)
		;

	/* Wait for BSY to indicate last data frame is processed */
	while (spi->sr & STM32_SPI_SR_BSY)
		;

	/* Disable TX DMA */
	dma_disable(dma_tx_option.channel);

	/* Wait for DMA reception to complete */
	dma_wait(dma_rx_option.channel);

	/* Wait for FRLVL[1:0] to indicate FIFO empty */
	while (spi->sr & STM32_SPI_SR_FRLVL)
		;

	/* Disable RX DMA */
	dma_disable(dma_rx_option.channel);

	/* Drive SS high */
	gpio_set_level(GPIO_PD_TX_EN, 1);
}

/**
 * Initialize SPI module, registers, and clocks
 */
static void spi_initialize(void)
{
	stm32_spi_regs_t *spi = CONFIG_SPI_FLASH_REGISTER;

	board_set_debug(1);

	/* Set SPI master, baud rate, and software slave control */
	spi->cr1 |= STM32_SPI_CR1_MSTR | STM32_SPI_CR1_BR_DIV4R |
				STM32_SPI_CR1_SSM | STM32_SPI_CR1_SSI;

	/* Unset CPOL, CPHA, LSBFIRST, and CRC */
	spi->cr1 &= ~(STM32_SPI_CR1_CPOL | STM32_SPI_CR1_CPHA |
				STM32_SPI_CR1_LSBFIRST | STM32_SPI_CR1_CRCEN);

	/* Configure 8-bit datasize, set FRXTH, enable DMA,
	 * and enable NSS output */
	spi->cr2 = STM32_SPI_CR2_TXDMAEN | STM32_SPI_CR2_RXDMAEN |
			   STM32_SPI_CR2_FRXTH | STM32_SPI_CR2_DATASIZE(8);

	/* Enable SPI */
	spi->cr1 |= STM32_SPI_CR1_SPE;
}

/*
 * Shutdown SPI
 */
static void spi_shutdown(void)
{
	stm32_spi_regs_t *spi = CONFIG_SPI_FLASH_REGISTER;

	/* Disable DMA streams */
	dma_disable(dma_tx_option.channel);
	dma_disable(dma_rx_option.channel);

	/* Wait for FTLVL[1:0] to indicate FIFO empty */
	while (spi->sr & STM32_SPI_SR_FTLVL)
		;

	/* Wait for BSY to indicate last data frame is processed */
	while (spi->sr & STM32_SPI_SR_BSY)
		;

	/* Disable SPI */
	spi->cr1 &= ~STM32_SPI_CR1_SPE;

	/* Read until FRLVL[1:0] is empty */
	while (spi->sr & STM32_SPI_SR_FTLVL)
		buf[0] = spi->dr;

	/* Disable DMA buffers */
	spi->cr2 &= ~(STM32_SPI_CR2_TXDMAEN | STM32_SPI_CR2_RXDMAEN);

	board_set_debug(0);
}

/**
 * Enable/disable the SPI GPIOs
 * @param enable	Whether to enable or disable
 */
int spi_enable(int enable)
{
	if (enable) {
		/* Initialize SPI */
		spi_initialize();

		/* Enable pullup on PD_TX_EN/SPI_NSS */
		gpio_set_flags(GPIO_PD_TX_EN, GPIO_OUTPUT | GPIO_PULL_UP);

		/* Drive SS high */
		gpio_set_level(GPIO_PD_TX_EN, 1);

		/* Set SPI pins to alternate function */
		gpio_config_module(MODULE_USB_PD, 1);
	} else {
		/* Set SPI pins to inputs */
		gpio_config_module(MODULE_USB_PD, 0);

		/* Disable PD_TX_EN/SPI_NSS */
		gpio_set_flags(GPIO_PD_TX_EN, GPIO_INPUT);

		/* Shutdown SPI */
		spi_shutdown();
	}

	spi_enabled = enable;

	return EC_SUCCESS;
}

/**
 * Set the write enable latch
 */
static void spi_flash_write_enable(void)
{
	/* Compose instruction */
	buf[0] = SPI_FLASH_WRITE_ENABLE;

	communicate(1, 0);
}

/**
 * Returns the contents of SPI flash status register 1
 * @return register contents
 */
uint8_t spi_flash_get_status1(void)
{
	uint8_t res;

	/* Get SR 1 */
	buf[0] = SPI_FLASH_READ_SR1;
	communicate(1, 1);

	res = buf[1];
	return res;
}

/**
 * Returns the contents of SPI flash status register 2
 * @return register contents
 */
uint8_t spi_flash_get_status2(void)
{
	uint8_t res;

	/* Get SR 2 */
	buf[0] = SPI_FLASH_READ_SR2;
	communicate(1, 1);

	res = buf[1];
	return res;
}


/**
 * Sets the SPI flash status registers (non-volatile bits only)
 * @param reg1	Status register 1
 * @param reg2	Status register 2
 */
int spi_flash_set_status(int reg1, int reg2)
{
	/* Register has protection */
	if (spi_flash_get_status1() & SPI_FLASH_SR2_SRP1)
		return EC_ERROR_ACCESS_DENIED;

	/* Enable writing to SPI flash */
	spi_flash_write_enable();

	/* Compose instruction */
	buf[0] = SPI_FLASH_WRITE_SR;
	buf[1] = reg1;
	buf[2] = reg2;

	communicate(3, 0);

	/* Wait until chip is not busy */
	while (spi_flash_get_status1() & SPI_FLASH_SR1_BUSY)
		;

	return EC_SUCCESS;
}

/**
 * Returns the contents of SPI flash
 * @param buf	Buffer to write flash contents
 * @param offset Flash offset to start reading from
 * @param bytes	Number of bytes to read
 * @return EC_SUCCESS, or non-zero if any error.
 */
int spi_flash_read(uint8_t *buf_usr, int offset, int bytes)
{
	if (offset + bytes > CONFIG_SPI_FLASH_SIZE)
		return EC_ERROR_ACCESS_DENIED;

	/* Compose instruction */
	buf[0] = SPI_FLASH_READ;
	buf[1] = (offset >> 16) & 0xFF;
	buf[2] = (offset >> 8) & 0xFF;
	buf[3] = offset & 0xFF;

	communicate(4, bytes);

	memmove(buf_usr, buf + 4, bytes);
	return EC_SUCCESS;
}

/**
 * Erase 32kb of SPI flash.
 * @param offset Flash offset to start erasing
 */
static int spi_flash_erase_32kb(int offset)
{
	/* Chip has protection */
	if (spi_flash_get_status1()
		& (SPI_FLASH_SR1_BP0 | SPI_FLASH_SR1_BP1 | SPI_FLASH_SR1_BP2))
		return EC_ERROR_ACCESS_DENIED;

	/* Enable writing to SPI flash */
	spi_flash_write_enable();

	/* Compose instruction */
	buf[0] = SPI_FLASH_ERASE_32KB;
	buf[1] = (offset >> 16) & 0xFF;
	buf[2] = (offset >> 8) & 0xFF;
	buf[3] = offset & 0xFF;

	communicate(4, 0);

	/* Wait until chip is not busy */
	while (spi_flash_get_status1() & SPI_FLASH_SR1_BUSY)
		watchdog_reload();

	return EC_SUCCESS;
}

/**
 * Erase 4kb of SPI flash.
 * @param offset Flash offset to start erasing
 */
static int spi_flash_erase_4kb(int offset)
{
	/* Chip has protection */
	if (spi_flash_get_status1()
		& (SPI_FLASH_SR1_BP0 | SPI_FLASH_SR1_BP1 | SPI_FLASH_SR1_BP2))
		return EC_ERROR_ACCESS_DENIED;

	/* Enable writing to SPI flash */
	spi_flash_write_enable();

	/* Compose instruction */
	buf[0] = SPI_FLASH_ERASE_4KB;
	buf[1] = (offset >> 16) & 0xFF;
	buf[2] = (offset >> 8) & 0xFF;
	buf[3] = offset & 0xFF;

	communicate(4, 0);

	/* Wait until chip is not busy */
	while (spi_flash_get_status1() & SPI_FLASH_SR1_BUSY)
		watchdog_reload();

	return EC_SUCCESS;
}

/**
 * Erase SPI flash.
 * @param offset Flash offset to start erasing
 * @param bytes	Number of bytes to erase
 * @return EC_SUCCESS, or non-zero if any error.
 */
int spi_flash_erase(int offset, int bytes)
{
	int rv = EC_SUCCESS;

	/* Invalid input */
	if (offset < 0 || bytes < 0)
		return EC_ERROR_INVAL;

	/* Not aligned to sector (4kb) */
	if (offset % 4096 || bytes % 4096)
		return EC_ERROR_INVAL;

	/* Largest unit is block (32kb) */
	while (bytes != (bytes % (32 * 1024))) {
		rv = spi_flash_erase_32kb(offset);
		if (rv)
			return rv;

		bytes -= 32 * 1024;
		offset += 32 * 1024;
	}

	/* Largest unit is sector (4kb) */
	while (bytes != (bytes % (4 * 1024))) {
		rv = spi_flash_erase_4kb(offset);
		if (rv)
			return rv;

		bytes -= 4 * 1024;
		offset += 4 * 1024;
	}

	ASSERT(bytes == 0);
	return rv;
}

/**
 * Write to SPI flash. Assumes already erased.
 * @param offset Flash offset to write
 * @param bytes Number of bytes to write
 * @param data Data to write to flash
 * @return EC_SUCCESS, or non-zero if any error.
 */
int spi_flash_write(int offset, int bytes, const uint8_t const *data)
{
	int write_len;

	/* Invalid input */
	if (offset < 0 || bytes < 0)
		return EC_ERROR_INVAL;

	/* Write while there is remaining unwritten data */
	while (bytes > 0) {
		/* Enable writing to SPI flash */
		spi_flash_write_enable();

		/* First write (bytes % 256), then in multiples of 256 */
		write_len = (bytes % SPI_FLASH_MAX_WRITE_SIZE) ?
					(bytes % SPI_FLASH_MAX_WRITE_SIZE) :
					SPI_FLASH_MAX_WRITE_SIZE;

		/* Compose instruction */
		buf[0] = SPI_FLASH_PAGE_PRGRM;
		buf[1] = (offset >> 16) & 0xFF;
		buf[2] = (offset >> 8) & 0xFF;
		buf[3] = offset & 0xFF;

		/* Copy data to send buffer; buffers may overlap */
		memmove(buf + 4, data, write_len);

		communicate(4 + write_len, 0);

		/* Wait until chip is not busy */
		while (spi_flash_get_status1() & SPI_FLASH_SR1_BUSY)
			;

		bytes -= write_len;
		offset += write_len;
		data += write_len;
	}

	ASSERT(bytes == 0);
	return EC_SUCCESS;
}

/**
 * Returns the SPI flash manufacturer ID and device ID [8:0]
 * @return flash manufacturer + device ID
 */
uint16_t spi_flash_get_id(void)
{
	uint16_t res;

	/* Compose instruction */
	buf[0] = SPI_FLASH_MFR_DEV_ID;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;

	communicate(4, 2);

	res = (buf[4] << 8) | buf[5];
	return res;
}

/**
 * Returns the SPI flash JEDEC ID (manufacturer ID, memory type, and capacity)
 * @return flash JEDEC ID
 */
uint32_t spi_flash_get_jedec_id(void)
{
	uint32_t res;

	/* Compose instruction */
	buf[0] = SPI_FLASH_JEDEC_ID;

	communicate(1, 4);

	memmove((uint8_t *) &res, buf + 1, 4);
	return res;
}

/**
 * Returns the SPI flash unique ID (serial)
 * @return flash unique ID
 */
uint64_t spi_flash_get_unique_id(void)
{
	uint64_t res;

	/* Compose instruction */
	buf[0] = SPI_FLASH_UNIQUE_ID;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;

	communicate(5, 8);

	memmove((uint8_t *) &res, buf + 5, 8);
	return res;
}

static int command_spi_flashinfo(int argc, char **argv)
{
	uint32_t jedec = spi_flash_get_jedec_id();
	uint64_t unique = spi_flash_get_unique_id();

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

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(spi_flashinfo, command_spi_flashinfo,
	NULL,
	"Print SPI flash info",
	NULL);

static int command_spi_flasherase(int argc, char **argv)
{
	/* TODO: check protected */

	int offset = -1;
	int bytes = 4096;
	int rv = parse_offset_size(argc, argv, 1, &offset, &bytes);

	if (rv)
		return rv;

	ccprintf("Erasing %d bytes at 0x%x...\n", bytes, offset);
	return spi_flash_erase(offset, bytes);
}
DECLARE_CONSOLE_COMMAND(spi_flasherase, command_spi_flasherase,
	"offset [bytes]",
	"Erase flash",
	NULL);

static int command_spi_flashwrite(int argc, char **argv)
{
	/* TODO: check protected */
	char *data;
	int offset = -1;
	int bytes = SPI_FLASH_MAX_WRITE_SIZE;
	int write_len;
	int rv = EC_SUCCESS;
	int i;

	rv = parse_offset_size(argc, argv, 1, &offset, &bytes);
	if (rv)
		return rv;

	/* Acquire the shared memory buffer */
	rv = shared_mem_acquire(SPI_FLASH_MAX_WRITE_SIZE, &data);
	if (rv)
		goto err_free;

	/* Fill the data buffer with a pattern */
	for (i = 0; i < SPI_FLASH_MAX_WRITE_SIZE; i++)
		data[i] = i;

	ccprintf("Writing %d bytes to 0x%x...\n", bytes, offset);
	while (bytes > 0) {
		/* First write multiples of 256, then (bytes % 256) last */
		write_len = ((bytes % SPI_FLASH_MAX_WRITE_SIZE) == bytes) ?
					bytes : SPI_FLASH_MAX_WRITE_SIZE;

		rv = spi_flash_write(offset, write_len, data);
		if (rv)
			goto err_free;

		offset += write_len;
		bytes -= write_len;
	}

err_free:
	/* Free the buffer */
	shared_mem_release(data);

	ASSERT(bytes == 0);
	return rv;
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
	int rv = parse_offset_size(argc, argv, 1, &offset, &bytes);

	if (rv)
		return rv;

	/* Can't read past size of memory */
	if (offset + bytes > CONFIG_SPI_FLASH_SIZE)
		return EC_ERROR_INVAL;

	ccprintf("Reading %d bytes from 0x%x...\n", bytes, offset);
	/* Read <= 256 bytes to avoid allocating another buffer */
	while (bytes > 0) {
		/* First read (bytes % 256), then in multiples of 256 */
		read_len = (bytes % SPI_FLASH_MAX_WRITE_SIZE) ?
					(bytes % SPI_FLASH_MAX_WRITE_SIZE) :
					SPI_FLASH_MAX_WRITE_SIZE;

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
	uint8_t sr1 = spi_flash_get_status1();
	uint8_t sr2 = spi_flash_get_status2();

	ccprintf("Status Register 1: 0x%02x\nStatus Register 2: 0x%02x\n",
			 sr1, sr2);

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

	ccprintf("Writing 0x%02x to status register 1, ", val1);
	ccprintf("0x%02x to status register 2...\n", val2);

	return spi_flash_set_status(val1, val2);
}
DECLARE_CONSOLE_COMMAND(spi_flash_wsr, command_spi_flashwrite_sr,
	"value1 value2",
	"Write to status registers",
	NULL);
