/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* HyperDebug SPI logic and console commands */

#include "common.h"
#include "console.h"
#include "dma.h"
#include "gpio.h"
#include "registers.h"
#include "spi.h"
#include "stm32-dma.h"
#include "timer.h"
#include "usb_spi.h"
#include "util.h"

#define OCTOSPI_CLOCK (CPU_CLOCK)
#define SPI_CLOCK (CPU_CLOCK)

/* SPI devices, default to 406 kb/s for all. */
struct spi_device_t spi_devices[] = {
	{ .name = "SPI2",
	  .port = 1,
	  .div = 7,
	  .gpio_cs = GPIO_CN9_25,
	  .usb_flags = USB_SPI_ENABLED },
	{ .name = "QSPI",
	  .port = -1 /* OCTOSPI */,
	  .div = 255,
	  .gpio_cs = GPIO_CN10_6,
	  .usb_flags = USB_SPI_ENABLED
	  | USB_SPI_CUSTOM_SPI_DEVICE
	  | USB_SPI_EEPROM_DUAL_SUPPORT
	  | USB_SPI_EEPROM_QUAD_SUPPORT },
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

/*
 * Find spi device by name or by number.  Returns an index into spi_devices[],
 * or on error a negative value.
 */
static int find_spi_by_name(const char *name)
{
	int i;
	char *e;
	i = strtoi(name, &e, 0);

	if (!*e && i < spi_devices_used)
		return i;

	for (i = 0; i < spi_devices_used; i++) {
		if (!strcasecmp(name, spi_devices[i].name))
			return i;
	}

	/* SPI device not found */
	return -1;
}

static void print_spi_info(int index)
{
	uint32_t bits_per_second;

	if (spi_devices[index].usb_flags & USB_SPI_CUSTOM_SPI_DEVICE) {
		// OCTOSPI as 8 bit prescaler, dividing clock by 1..256.
		bits_per_second = OCTOSPI_CLOCK / (spi_devices[index].div + 1);
	} else {
		// Other SPIs have prescaler by power of two 2, 4, 8, ..., 256.
		bits_per_second = SPI_CLOCK / (2 << spi_devices[index].div);
	}

	ccprintf("  %d %s %d bps\n", index, spi_devices[index].name,
		 bits_per_second);

	/* Flush console to avoid truncating output */
	cflush();
}

/*
 * Get information about one or all SPI ports.
 */
static int command_spi_info(int argc, const char **argv)
{
	int i;

	/* If a SPI target is specified, print only that one */
	if (argc == 3) {
		int index = find_spi_by_name(argv[2]);
		if (index < 0) {
			ccprintf("SPI device not found\n");
			return EC_ERROR_PARAM2;
		}

		print_spi_info(index);
		return EC_SUCCESS;
	}

	/* Otherwise print them all */
	for (i = 0; i < spi_devices_used; i++) {
		print_spi_info(i);
	}

	return EC_SUCCESS;
}

static int command_spi_set_speed(int argc, const char **argv)
{
	int index;
	uint32_t desired_speed;
	char *e;
	if (argc < 5)
		return EC_ERROR_PARAM_COUNT;

	index = find_spi_by_name(argv[3]);
	if (index < 0)
		return EC_ERROR_PARAM3;

	desired_speed = strtoi(argv[4], &e, 0);
	if (*e)
		return EC_ERROR_PARAM4;

	if (spi_devices[index].usb_flags & USB_SPI_CUSTOM_SPI_DEVICE) {
		/*
		 * Find prescaler value by division, rounding up in order to get
		 * slightly slower speed than requested, if it cannot be matched
		 * exactly.
		 */
		int divisor =
			(OCTOSPI_CLOCK + desired_speed - 1) / desired_speed - 1;
		if (divisor >= 256)
			divisor = 255;
		STM32_OCTOSPI_DCR2 = spi_devices[index].div = divisor;
	} else {
		int divisor = 7;
		/*
		 * Find the smallest divisor that result in a speed not faster
		 * than what was requested.
		 */
		while (divisor > 0) {
			if (SPI_CLOCK / (2 << (divisor - 1)) > desired_speed) {
				/* One step further would make the clock too
				 * fast, stop here. */
				break;
			}
			divisor--;
		}

		/*
		 * Re-initialize spi controller to apply the new clock divisor.
		 */
		spi_enable(&spi_devices[index], 0);
		spi_devices[index].div = divisor;
		spi_enable(&spi_devices[index], 1);
	}

	return EC_SUCCESS;
}

static int command_spi_set(int argc, const char **argv)
{
	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;
	if (!strcasecmp(argv[2], "speed"))
		return command_spi_set_speed(argc, argv);
	return EC_ERROR_PARAM2;
}

static int command_spi(int argc, const char **argv)
{
	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;
	if (!strcasecmp(argv[1], "info"))
		return command_spi_info(argc, argv);
	if (!strcasecmp(argv[1], "set"))
		return command_spi_set(argc, argv);
	return EC_ERROR_PARAM1;
}
DECLARE_CONSOLE_COMMAND_FLAGS(spi, command_spi,
			      "info [PORT]"
			      "\nset speed PORT BPS",
			      "SPI bus manipulation", CMD_FLAG_RESTRICTED);

/******************************************************************************
 * OCTOSPI driver.
 */

/*
 * Wait for a certain set of status bits to all be asserted.
 */
static int octospi_wait_for(uint32_t flags, timestamp_t deadline)
{
	while ((STM32_OCTOSPI_SR & flags) != flags) {
		timestamp_t now = get_time();
		if (timestamp_expired(deadline, &now))
			return EC_ERROR_TIMEOUT;
	}
	return EC_SUCCESS;
}

/*
 * Board-specific SPI driver entry point, called by usb_spi.c.
 */
void usb_spi_board_enable(struct usb_spi_config const *config)
{
	/* All initialization already done in board_init(). */
}

void usb_spi_board_disable(struct usb_spi_config const *config)
{
}

static const struct dma_option dma_optospi_option = {
	STM32_DMAC_CH13,
	(void *)&STM32_OCTOSPI_DR,
	STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT
};

static bool previous_cs;


/*
 * Board-specific SPI driver entry point, called by usb_spi.c.  On this board,
 * the only spi device declared as requiring board specific driver is OCTOSPI.
 */
int usb_spi_board_transaction_async(
	const struct spi_device_t *spi_device,
	uint32_t eeprom_flags,
	const uint8_t *txdata, int txlen, uint8_t *rxdata,
	int rxlen)
{
	int rv = EC_SUCCESS;
	uint32_t opcode = 0, address = 0, alternate = 0;
	uint8_t opcode_len = (eeprom_flags & EEPROM_FLAG_OPCODE_LEN_MSK)
		>> EEPROM_FLAG_OPCODE_LEN_POS;
	uint8_t addr_len = (eeprom_flags & EEPROM_FLAG_ADDR_LEN_MSK)
		>> EEPROM_FLAG_ADDR_LEN_POS;
	uint8_t alt_len = (eeprom_flags & EEPROM_FLAG_ALT_LEN_MSK)
		>> EEPROM_FLAG_ALT_LEN_POS;
	uint8_t dummy_cycles = (eeprom_flags & EEPROM_FLAG_DUMMY_CYCLES_MSK)
		>> EEPROM_FLAG_DUMMY_CYCLES_POS;
	uint32_t data_len;
	uint32_t control_value = 0;
	timestamp_t deadline;

	if (!eeprom_flags) {
		if (rxlen == SPI_READBACK_ALL) {
			cprints(CC_SPI,
				"Full duplex not supported by OctoSPI hardware");
			return EC_ERROR_UNIMPLEMENTED;
		} else if (!rxlen && !txlen) {
			/* No operation requested, done. */
			return EC_SUCCESS;
		} else if (!rxlen) {
			/*
			 * Transmit-only transaction.  This is implemented by not using
			 * any of the up to 12 bytes of instructions, but as all "data".
			 */
			eeprom_flags |= EEPROM_FLAG_READ_WRITE_WRITE;
		} else if (txlen <= 12) {
			/*
			 * Sending of up to 12 bytes, followed by reading a possibly
			 * large number of bytes.  This is implemented by a "read"
			 * transaction using the instruction and address feature of
			 * OctoSPI.
			 */
			if (txlen <= 4) {
				opcode_len = txlen;
			} else if (txlen <= 8) {
				opcode_len = 4;
				addr_len = txlen - 4;
			} else {
				opcode_len = 4;
				addr_len = 4;
				alt_len = txlen - 8;
			}
		} else {
			/*
			 * Sending many bytes, followed by reading.  This would
			 * have to be implemented as two separate OctoSPI
			 * transactions.
			 */
			cprints(CC_SPI,
				"General write-then-read not supported by OctoSPI hardware");
			return EC_ERROR_UNIMPLEMENTED;
		}
	}

	previous_cs = gpio_get_level(spi_device->gpio_cs);

	/* Drive chip select low */
	gpio_set_level(spi_device->gpio_cs, 0);

	/* Deadline on the entire SPI transaction. */
	deadline.val = get_time().val + OCTOSPI_TRANSACTION_TIMEOUT_US;

	if ((eeprom_flags & EEPROM_FLAG_READ_WRITE_MSK)
	    == EEPROM_FLAG_READ_WRITE_WRITE) {
		data_len = txlen - opcode_len - addr_len - alt_len;
		/* Enable OCTOSPI, indirect write mode. */
		STM32_OCTOSPI_CR = STM32_OCTOSPI_CR_FMODE_IND_WRITE |
			STM32_OCTOSPI_CR_DMAEN | STM32_OCTOSPI_CR_EN;
	} else {
		data_len = rxlen;
		/* Enable OCTOSPI, indirect read mode. */
		STM32_OCTOSPI_CR = STM32_OCTOSPI_CR_FMODE_IND_READ |
			STM32_OCTOSPI_CR_DMAEN | STM32_OCTOSPI_CR_EN;
	}

	gpio_set_level(GPIO_CN10_31, 0);
	
	/* Clear completion flag from last transaction. */
	STM32_OCTOSPI_FCR = STM32_OCTOSPI_FCR_CTCF;

	/* Data length. */
	STM32_OCTOSPI_DLR = data_len - 1;

	if (opcode_len == 0) {
		control_value |= STM32_OCTOSPI_CCR_IMODE_NONE;
	} else {
		uint32_t mode = (eeprom_flags & EEPROM_FLAG_OPCODE_WIDTH_MSK)
			>> EEPROM_FLAG_OPCODE_WIDTH_POS;
		control_value |= (mode + 1) << STM32_OCTOSPI_CCR_IMODE_POS |
			(opcode_len - 1) << STM32_OCTOSPI_CCR_ISIZE_POS;
		if (eeprom_flags & EEPROM_FLAG_OPCODE_DTR_MSK)
			control_value |= STM32_OCTOSPI_CCR_IDTR;
		for (int i = 0; i < opcode_len; i++) {
			opcode <<= 8;
			opcode |= *txdata++;
			txlen--;
		}
	}
	if (addr_len == 0) {
		control_value |= STM32_OCTOSPI_CCR_ADMODE_NONE;
	} else {
		uint32_t mode = (eeprom_flags & EEPROM_FLAG_ADDR_WIDTH_MSK)
			>> EEPROM_FLAG_ADDR_WIDTH_POS;
		control_value |= (mode + 1) << STM32_OCTOSPI_CCR_ADMODE_POS |
			(addr_len - 1) << STM32_OCTOSPI_CCR_ADSIZE_POS;
		if (eeprom_flags & EEPROM_FLAG_ADDR_DTR_MSK)
			control_value |= STM32_OCTOSPI_CCR_ADDTR;
		for (int i = 0; i < addr_len; i++) {
			address <<= 8;
			address |= *txdata++;
			txlen--;
		}
	}
	if (alt_len == 0) {
		control_value |= STM32_OCTOSPI_CCR_ABMODE_NONE;
	} else {
		uint32_t mode = (eeprom_flags & EEPROM_FLAG_ALT_WIDTH_MSK)
			>> EEPROM_FLAG_ALT_WIDTH_POS;
		control_value |= (mode + 1) << STM32_OCTOSPI_CCR_ABMODE_POS |
			(alt_len - 1) << STM32_OCTOSPI_CCR_ABSIZE_POS;
		if (eeprom_flags & EEPROM_FLAG_ALT_DTR_MSK)
			control_value |= STM32_OCTOSPI_CCR_ABDTR;
		for (int i = 0; i < alt_len; i++) {
			alternate <<= 8;
			alternate |= *txdata++;
			txlen--;
		}
		STM32_OCTOSPI_ABR = alternate;
	}
	if (data_len == 0) {
		control_value |= STM32_OCTOSPI_CCR_DMODE_NONE;
	} else {
		uint32_t mode = (eeprom_flags & EEPROM_FLAG_DATA_WIDTH_MSK)
			>> EEPROM_FLAG_DATA_WIDTH_POS;
		control_value |= (mode + 1) << STM32_OCTOSPI_CCR_DMODE_POS;
		if (eeprom_flags & EEPROM_FLAG_DATA_DTR_MSK)
			control_value |= STM32_OCTOSPI_CCR_DDTR;
	}

	ccprintf("EEPROM flags: 0x%08x opcode:%x:%d, addr:%x:%d, alt:%x:%d, dummy:%d, data:%d\n", eeprom_flags, opcode, opcode_len, address, addr_len, alternate, alt_len, dummy_cycles, data_len);
	
	/* Dummy cycles. */
	STM32_OCTOSPI_TCR = dummy_cycles << STM32_OCTOSPI_TCR_DCYC_POS;

	STM32_OCTOSPI_CCR = control_value;


	/* Set instruction and address registers, triggering the start of the
	 * write+read transaction. */
	STM32_OCTOSPI_IR = opcode;
	STM32_OCTOSPI_AR = address;

	if ((eeprom_flags & EEPROM_FLAG_READ_WRITE_MSK)
	    == EEPROM_FLAG_READ_WRITE_WRITE) {
		if (txlen > 0) {
			dma_chan_t *txdma = dma_get_channel(STM32_DMAC_CH13);
			dma_prepare_tx(&dma_optospi_option, txlen, txdata);
			dma_go(txdma);
			gpio_set_level(GPIO_CN10_29, 0);
			rv = dma_wait(STM32_DMAC_CH13);
			dma_disable(STM32_DMAC_CH13);
			gpio_set_level(GPIO_CN10_31, 1);
			if (rv)
				return rv;
		} else {
			gpio_set_level(GPIO_CN10_29, 0);
			gpio_set_level(GPIO_CN10_31, 1);
		}
	} else {
		if (rxlen > 0) {
			dma_start_rx(&dma_optospi_option, rxlen, rxdata);
			gpio_set_level(GPIO_CN10_29, 0);
			rv = dma_wait(STM32_DMAC_CH13);
			dma_disable(STM32_DMAC_CH13);
			gpio_set_level(GPIO_CN10_31, 1);
			if (rv)
				return rv;
		} else {
			gpio_set_level(GPIO_CN10_29, 0);
			gpio_set_level(GPIO_CN10_31, 1);
		}
	}
	
	/* Wait for transaction completion flag. */
	rv = octospi_wait_for(STM32_OCTOSPI_SR_TCF, deadline);
	
	gpio_set_level(GPIO_CN10_29, 1);

	/* Return chip select to previous level. */
	gpio_set_level(spi_device->gpio_cs, previous_cs);

	return rv;
}

int usb_spi_board_transaction_flush(
	const struct spi_device_t *spi_device)
{
	return EC_SUCCESS;
}
