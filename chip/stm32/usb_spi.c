/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "compile_time_macros.h"
#include "link_defs.h"
#include "registers.h"
#include "spi.h"
#include "usb.h"
#include "usb_spi.h"

/*
 * Command:
 *     +------------------+-----------------+------------------------+
 *     | write count : 1B | read count : 1B | write payload : <= 62B |
 *     +------------------+-----------------+------------------------+
 *
 *     write count:   1 byte, zero based count of bytes to write
 *
 *     read count:    1 byte, zero based count of bytes to read
 *
 *     write payload: up to 62 bytes of data to write, length must match
 *                    write count
 *
 * Response:
 *     +-------------+-----------------------+
 *     | status : 2B | read payload : <= 62B |
 *     +-------------+-----------------------+
 *
 *     status: 2 byte status
 *         0x0000: Success
 *         0x0001: SPI timeout
 *         0x0002: Busy, try again
 *             This can happen if someone else has acquired the shared memory
 *             buffer that the SPI driver uses as /dev/null
 *         0x0003: Write count invalid (> 62 bytes, or missmatch with payload)
 *         0x0004: Read count invalid (> 62 bytes)
 *         0x8000: Unknown error mask
 *             The bottom 15 bits will contain the bottom 15 bits from the EC
 *             error code.
 *
 *     read payload: up to 62 bytes of data read from SPI, length will match
 *                   requested read count
 */

enum usb_spi_error {
	usb_spi_success             = 0x0000,
	usb_spi_timeout             = 0x0001,
	usb_spi_busy                = 0x0002,
	usb_spi_write_count_invalid = 0x0003,
	usb_spi_read_count_invalid  = 0x0004,
	usb_spi_unknown_error       = 0x8000,
};

/*
 * Endpoint index, and pointers to the USB packet RAM buffers.
 */
static int const endpoint = CONFIG_USB_SPI_ENDPOINT;

/*
 * The USB peripheral doesn't support DMA access to its packet RAM so
 * we have to copy messages out into a bounce buffer.
 */
static uint16_t buffer[USB_MAX_PACKET_SIZE / 2];

/*
 * Packet RAM TX and RX buffers;
 */
static usb_uint rx_buffer[USB_MAX_PACKET_SIZE / 2] __usb_ram;
static usb_uint tx_buffer[USB_MAX_PACKET_SIZE / 2] __usb_ram;

BUILD_ASSERT(ARRAY_SIZE(buffer) == ARRAY_SIZE(rx_buffer));
BUILD_ASSERT(ARRAY_SIZE(buffer) == ARRAY_SIZE(tx_buffer));

#define MAX_WRITE_COUNT 62
#define MAX_READ_COUNT  62

void usb_spi_tx(void)
{
	STM32_TOGGLE_EP(endpoint, EP_TX_MASK, EP_TX_NAK, 0);
}

void usb_spi_rx(void)
{
	STM32_TOGGLE_EP(endpoint, EP_RX_MASK, EP_RX_NAK, 0);
	task_wake(TASK_ID_USB_SPI);
}

void usb_spi_reset(void)
{
	btable_ep[endpoint].tx_addr  = usb_sram_addr(tx_buffer);
	btable_ep[endpoint].tx_count = 0;

	btable_ep[endpoint].rx_addr  = usb_sram_addr(rx_buffer);
	btable_ep[endpoint].rx_count =
		0x8000 | ((USB_MAX_PACKET_SIZE / 32 - 1) << 10);

	STM32_USB_EP(endpoint) = ((endpoint <<  0) | /* Endpoint Addr*/
				  (2        <<  4) | /* TX NAK */
				  (0        <<  9) | /* Bulk EP */
				  (3        << 12)); /* RX Valid */
}

int16_t usb_spi_map_error(int error)
{
	switch (error) {
	case EC_SUCCESS:       return usb_spi_success;
	case EC_ERROR_TIMEOUT: return usb_spi_timeout;
	case EC_ERROR_BUSY:    return usb_spi_busy;
	default:               return usb_spi_unknown_error | (error & 0x7fff);
	}
}

static void board_init_spi2(void)
{
	/* Remap SPI2 to DMA channels 6 and 7 */
	STM32_SYSCFG_CFGR1 |= (1 << 24);

	/* Set pin NSS to general purpose output mode (01b). */
	/* Set pins SCK, MISO, and MOSI to alternate function (10b). */
	STM32_GPIO_MODER(GPIO_B) &= ~0xff000000;
	STM32_GPIO_MODER(GPIO_B) |= 0xa9000000;

	/* Set all four pins to alternate function 0 */
	STM32_GPIO_AFRH(GPIO_B) &= ~(0xffff0000);

	/* Set all four pins to output push-pull */
	STM32_GPIO_OTYPER(GPIO_B) &= ~(0xf000);

	/* Set pullup on NSS */
	STM32_GPIO_PUPDR(GPIO_B) |= 0x1000000;

	/* Set all four pins to high speed */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0xff000000;

	/* Reset SPI2 */
	STM32_RCC_APB1RSTR |= (1 << 14);
	STM32_RCC_APB1RSTR &= ~(1 << 14);

	/* Enable clocks to SPI2 module */
	STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI2;
}

void usb_spi_task(void)
{
	int primed = 0;

	board_init_spi2();

	spi_enable(1);

	while (1) {
		size_t  i;
		uint8_t count;
		uint8_t write_count;
		uint8_t read_count;

		task_wait_event(-1);

		count = btable_ep[endpoint].rx_count & 0x3ff;

		for (i = 0; i < (count + 1) / 2; ++i)
			buffer[i] = rx_buffer[i];

		/*
		 * RX packet consumed, mark the packet as VALID.  The master
		 * could queue up the next command while we process this SPI
		 * transaction and prepare the response.
		 */
		STM32_TOGGLE_EP(endpoint, EP_RX_MASK, EP_RX_VALID, 0);

		write_count = (buffer[0] >> 0) & 0xff;
		read_count  = (buffer[0] >> 8) & 0xff;

		if (primed) {
			ccprintf("second:\n");
			ccprintf("    write_count = %d\n", write_count);
			ccprintf("    read_count  = %d\n", read_count);
			primed = 0;
		}

		if (write_count == 1 &&
		    read_count == 0 &&
		    (buffer[1] & 0xff) == 6) {
			ccprintf("primed:\n");
			ccprintf("    write_count = %d\n", write_count);
			ccprintf("    read_count  = %d\n", read_count);
			primed = 1;
		}

		if (write_count > MAX_READ_COUNT ||
		    write_count != (count - 2)) {
			buffer[0] = usb_spi_write_count_invalid;
		} else if (read_count > MAX_READ_COUNT) {
			buffer[0] = usb_spi_read_count_invalid;
		} else {
			buffer[0] = usb_spi_map_error(
				spi_transaction((uint8_t *)(buffer + 1),
						write_count,
						(uint8_t *)(buffer + 1),
						read_count));
		}

		/*
		 * Copy read bytes and status back out of bounce buffer and
		 * update TX packet state (mark as VALID for master to read).
		 */
		for (i = 0; i < (read_count + 1 + 2) / 2; ++i)
			tx_buffer[i] = buffer[i];

		btable_ep[endpoint].tx_count = read_count + 2;

		STM32_TOGGLE_EP(endpoint, EP_TX_MASK, EP_TX_VALID, 0);
	}
}

const struct usb_interface_descriptor
USB_IFACE_DESC(CONFIG_USB_SPI_INTERFACE) = {
	.bLength            = USB_DT_INTERFACE_SIZE,
	.bDescriptorType    = USB_DT_INTERFACE,
	.bInterfaceNumber   = CONFIG_USB_SPI_INTERFACE,
	.bAlternateSetting  = 0,
	.bNumEndpoints      = 2,
	.bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface         = 0,
};

const struct usb_endpoint_descriptor
USB_EP_DESC(CONFIG_USB_SPI_INTERFACE, 0) = {
	.bLength          = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x80 | CONFIG_USB_SPI_ENDPOINT,
	.bmAttributes     = 0x02 /* Bulk IN */,
	.wMaxPacketSize   = USB_MAX_PACKET_SIZE,
	.bInterval        = 10,
};

const struct usb_endpoint_descriptor
USB_EP_DESC(CONFIG_USB_SPI_INTERFACE, 1) = {
	.bLength          = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  = USB_DT_ENDPOINT,
	.bEndpointAddress = CONFIG_USB_SPI_ENDPOINT,
	.bmAttributes     = 0x02 /* Bulk OUT */,
	.wMaxPacketSize   = USB_MAX_PACKET_SIZE,
	.bInterval        = 0,
};

USB_DECLARE_EP(CONFIG_USB_SPI_ENDPOINT, usb_spi_tx, usb_spi_rx, usb_spi_reset);
