/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * For Chipset: RTK EC
 *
 * Function: RTK Flash Utility
 */

/*********************
 *      INCLUDES
 *********************/

#include "flash_map_backend.h"
#include "reg.h"

#include <stdint.h>
#include <stdio.h>
/*********************
 *      DEFINES
 *********************/

#define RTS_MONITOR_UUT_TAG 0x4352544Bul
#define RTS_MONITOR_HEADER_ADDR 0x20010000ul
#define RTS_TEMP_DATA_ADDR 0x20020000ul
#define RTS_CMD_SEL_ADDR 0x2005F000ul
#define RTS_SPI_PROGRAMMING_FLAG 0x20018000ul

/**********************
 *      TYPEDEFS
 **********************/

struct monitor_header_tag {
	/* offset 0x00: TAG NPCX_MONITOR_TAG */
	uint32_t tag;
	/* offset 0x04: Size of the binary being programmed (in bytes) */
	uint32_t size;
	/* offset 0x08: The RAM address of the binary to program into the SPI */
	uint32_t src_addr;
	/* offset 0x0C: The Flash address to be programmed (Absolute address) */
	uint32_t dest_addr;
	/* offset 0x10: Maximum allowable flash clock frequency */
	uint8_t max_clock;
	/* offset 0x11: SPI Flash read mode */
	uint8_t read_mode;
	/* offset 0x12: Reserved */
	uint16_t reserved;
} __packed;

/**********************
 *  EXTERN PROTOTYPES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void eflash_erase(int offset, int size);
static void eflash_write(int offset, int size, const char *data);
static int eflash_verify(int offset, int size, const char *data);
static void eflash_read(int offset, int size, const char *data);
static void uart_init_pll_115200(void);
static void serial_polling_send(const char *buf, uint32_t len);
/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int spic_flash_upload(void)
{
	/*
	 * Flash image has been uploaded to Code RAM
	 */
	uint32_t sz_image;
	uint32_t uut_tag;
	const char *image_base;
	uint32_t *flag_upload;
	struct monitor_header_tag *monitor_header =
		(struct monitor_header_tag *)(RTS_MONITOR_HEADER_ADDR);

	int spi_offset;

	flag_upload = (uint32_t *)(RTS_SPI_PROGRAMMING_FLAG);
	*flag_upload = 0;

	uut_tag = monitor_header->tag;
	/* If it is UUT tag, read required parameters from header */
	if (uut_tag == RTS_MONITOR_UUT_TAG) {
		sz_image = monitor_header->size;
		spi_offset = monitor_header->dest_addr;

		image_base = (const char *)(monitor_header->src_addr);
	} else {
		*flag_upload = 0x08;
		return -1;
	}

	intr_flash_pin_init();
	spic_init(3, 0);
	while ((UART->LSR & UART_LSR_THRE_Msk) == 0) {
		;
	}
	while (UART->USR_b.BUSY)
		;
	UART->SRR_b.UR = 1;
	uart_init_pll_115200();
	UART->FCR |= 0x47;

	uint32_t *temp_to_load;
	temp_to_load = (uint32_t *)RTS_CMD_SEL_ADDR;
	if (*temp_to_load == 0xA5A5A5A5) {
		*flag_upload |= 0x04;
		eflash_read(spi_offset, sz_image, image_base);

	} else {
		/* Start to erase */
		eflash_erase(spi_offset, sz_image);
		/* Start to write */
		if (image_base != NULL) {
			eflash_write(spi_offset, sz_image, image_base);
		}

		/* Verify data */
		if (eflash_verify(spi_offset, sz_image, image_base) == 0) {
			*flag_upload |= 0x02;
		}
	}
	/* Mark we have finished upload work */
	*flag_upload |= 0x01;

	/* Return the status back to ROM code is required for UUT */
	if (uut_tag == RTS_MONITOR_UUT_TAG)
		return *flag_upload;

	/* Infinite loop */
	for (;;)
		;

	return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void eflash_erase(int offset, int size)
{
	/* Alignment has been checked in upper layer */
	for (; size > 0; size -= FLASH_SECTOR_EARSE_SIZE,
			 offset += FLASH_SECTOR_EARSE_SIZE) {
		flash_erase_sector(offset, FLASH_ADDRESSING_3BYTE);
	}
}

static void eflash_write(int offset, int size, const char *data)
{
	int dest_addr = offset;
	const int sz_page = FLASH_PAGE_PROGRAM_SIZE;

	/* Write the data per CONFIG_FLASH_WRITE_IDEAL_SIZE bytes */
	for (; size >= sz_page; size -= sz_page) {
		flash_program_page(dest_addr, (uint8_t *)data, sz_page,
				   FLASH_ADDRESSING_3BYTE);

		data += sz_page;
		dest_addr += sz_page;
	}

	/* Handle final partial page, if any */
	if (size != 0) {
		flash_program_page(dest_addr, (uint8_t *)data, size,
				   FLASH_ADDRESSING_3BYTE);
	}
}

static void serial_polling_send(const char *buf, uint32_t len)
{
	uint32_t i = 0;
	for (i = 0; i < len; i++) {
		while ((UART->LSR & UART_LSR_THRE_Msk) == 0) {
			;
		}
		UART->THR = buf[i];
	}
}

static void eflash_read(int offset, int size, const char *data)
{
	int dest_addr = offset;
	const int sz_page = FLASH_PAGE_PROGRAM_SIZE;

	/* Write the data per CONFIG_FLASH_WRITE_IDEAL_SIZE bytes */
	for (; size >= sz_page; size -= sz_page) {
		flash_read(0x03, dest_addr, (char *)data, sz_page,
			   FLASH_ADDRESSING_3BYTE);

		serial_polling_send(data, sz_page);

		data += sz_page;
		dest_addr += sz_page;
	}

	/* Handle final partial page, if any */
	if (size != 0) {
		flash_read(0x03, dest_addr, (char *)data, size,
			   FLASH_ADDRESSING_3BYTE);
		serial_polling_send(data, size);
	}
}

static int eflash_verify(int offset, int size, const char *data)
{
	int dest_addr = offset;
	const int sz_page = FLASH_PAGE_PROGRAM_SIZE;

	uint32_t i;
	uint8_t tmp;
	uint8_t rd_buf[FLASH_PAGE_PROGRAM_SIZE];

	/* Write the data per CONFIG_FLASH_WRITE_IDEAL_SIZE bytes */
	for (; size >= sz_page; size -= sz_page) {
		flash_read(0x03, dest_addr, rd_buf, sz_page,
			   FLASH_ADDRESSING_3BYTE);
		for (i = 0; i < sz_page; i++) {
			tmp = *(data + i);
			if (rd_buf[i] != tmp) {
				return -1;
			}
		}

		data += sz_page;
		dest_addr += sz_page;
	}

	/* Handle final partial page, if any */
	if (size != 0) {
		flash_read(0x03, dest_addr, rd_buf, size,
			   FLASH_ADDRESSING_3BYTE);
		for (i = 0; i < size; i++) {
			tmp = *(data + i);
			if (rd_buf[i] != tmp) {
				return -1;
			}
		}
	}

	return 0;
}

//
// uart_init_pll_115200
//
static void uart_init_pll_115200(void)
{
	// UART->SRR_b.UR = 1;
	// UART->SRR_b.RFR = 1;
	// UART->SRR_b.XFR = 1;

	SYSTEM->UARTCLK_b.PWR = 0;
	SYSTEM->UARTCLK_b.SRC = 1;
	SYSTEM->UARTCLK_b.DIV = 0;
	SYSTEM->UARTCLK_b.PWR = 1;

	/*
	 * Set UART parameter
	 *  - 8-bit data, 1-bit stop, no parity
	 */
	UART->LCR_b.DLS = 3;
	UART->LCR_b.STOP = 0;
	UART->LCR_b.PEN = 0;

	/* Enable FIFO mode */
	UART->FCR_b.FIFOE = 1;

	/* Disable all interrupt */
	//  UART->IER = 0ul;

	/*
	 * Set baud rate
	 *  - Baud rate = (uart_clk) / (16 * divisor)
	 */
	UART->LCR_b.DLAB = 1;
	UART->DLH = 0x00000000ul;
	UART->DLL = 0x00000035ul;
	UART->LCR_b.DLAB = 0;
}
