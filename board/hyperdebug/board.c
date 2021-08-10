/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* HyperDebug board configuration */

#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
/*
 * #include "hooks.h"
 */
#include "i2c.h"
/*
 * #include "i2c_ite_flash_support.h"
 */
#include "queue_policies.h"
#include "registers.h"
#include "spi.h"
/*
 * #include "system.h"
 * #include "task.h"
 * #include "timer.h"
 * #include "update_fw.h"
 * #include "usart_tx_dma.h"
 * #include "usart_rx_dma.h"
 */
#include "usart-stm32l5.h"
#include "usb_hw.h"
/*
 * #include "usb_i2c.h"
 */
#include "usb_spi.h"
#include "usb-stream.h"
/*
 * #include "util.h"
 * #include "chip/stm32/usb-stm32f3.h"
 */
#include "gpio_list.h"

void board_config_pre_init(void)
{
}

/******************************************************************************
 * Forward UARTs as a USB serial interface.
 */

#define USB_STREAM_RX_SIZE	16
#define USB_STREAM_TX_SIZE	16

/******************************************************************************
 * Forward USART1 as a simple USB serial interface.
 */

static struct usart_config const usart1;
struct usb_stream_config const usart1_usb;

static struct queue const usart1_to_usb = QUEUE_DIRECT(64, uint8_t,
	usart1.producer, usart1_usb.consumer);
static struct queue const usb_to_usart1 = QUEUE_DIRECT(64, uint8_t,
	usart1_usb.producer, usart1.consumer);

static struct usart_config const usart1 =
	USART_CONFIG(usart1_hw,
		usart_rx_interrupt,
		usart_tx_interrupt,
		115200,
		0,
		usart1_to_usb,
		usb_to_usart1);

USB_STREAM_CONFIG(usart1_usb,
	USB_IFACE_USART1_STREAM,
	USB_STR_USART1_STREAM_NAME,
	USB_EP_USART1_STREAM,
	USB_STREAM_RX_SIZE,
	USB_STREAM_TX_SIZE,
	usb_to_usart1,
	usart1_to_usb)

/******************************************************************************
 * Forward USART2 as a simple USB serial interface.
 */

static struct usart_config const usart2;
struct usb_stream_config const usart2_usb;

static struct queue const usart2_to_usb = QUEUE_DIRECT(64, uint8_t,
	usart2.producer, usart2_usb.consumer);
static struct queue const usb_to_usart2 = QUEUE_DIRECT(64, uint8_t,
	usart2_usb.producer, usart2.consumer);

static struct usart_config const usart2 =
	USART_CONFIG(usart2_hw,
		usart_rx_interrupt,
		usart_tx_interrupt,
		115200,
		0,
		usart2_to_usb,
		usb_to_usart2);

USB_STREAM_CONFIG(usart2_usb,
	USB_IFACE_USART2_STREAM,
	USB_STR_USART2_STREAM_NAME,
	USB_EP_USART2_STREAM,
	USB_STREAM_RX_SIZE,
	USB_STREAM_TX_SIZE,
	usb_to_usart2,
	usart2_to_usb)

/******************************************************************************
 * Forward USART4 as a simple USB serial interface.
 */

static struct usart_config const usart4;
struct usb_stream_config const usart4_usb;

static struct queue const usart4_to_usb = QUEUE_DIRECT(64, uint8_t,
	usart4.producer, usart4_usb.consumer);
static struct queue const usb_to_usart4 = QUEUE_DIRECT(64, uint8_t,
	usart4_usb.producer, usart4.consumer);

static struct usart_config const usart4 =
	USART_CONFIG(usart4_hw,
		usart_rx_interrupt,
		usart_tx_interrupt,
		115200,
		0,
		usart4_to_usb,
		usb_to_usart4);

USB_STREAM_CONFIG(usart4_usb,
	USB_IFACE_USART4_STREAM,
	USB_STR_USART4_STREAM_NAME,
	USB_EP_USART4_STREAM,
	USB_STREAM_RX_SIZE,
	USB_STREAM_TX_SIZE,
	usb_to_usart4,
	usart4_to_usb)


/******************************************************************************
 * Support SPI bridging over USB, this requires usb_spi_board_enable and
 * usb_spi_board_disable to be defined to enable and disable the SPI bridge.
 */

/* SPI devices */
const struct spi_device_t spi_devices[] = {
	{ /*CONFIG_SPI_FLASH_PORT*/ 0, 1, GPIO_NUCLEO_LED1},
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

void usb_spi_board_enable(struct usb_spi_config const *config)
{
	/* Configure SPI GPIOs */
	gpio_config_module(MODULE_SPI_FLASH, 1);

	/* Set all four SPI pins to high speed */
	/* STM32_GPIO_OSPEEDR(GPIO_B) |= 0xff000000; */

	/* Enable clocks to SPI2 module */
	/* STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI2; */

	/* Reset SPI2 */
	/* STM32_RCC_APB1RSTR |= STM32_RCC_PB1_SPI2; */
	/* STM32_RCC_APB1RSTR &= ~STM32_RCC_PB1_SPI2; */

	spi_enable(&spi_devices[0], 1);
}

void usb_spi_board_disable(struct usb_spi_config const *config)
{
	spi_enable(&spi_devices[0], 0);

	/* Disable clocks to SPI2 module */
	STM32_RCC_APB1ENR &= ~STM32_RCC_PB1_SPI2;

	/* Release SPI GPIOs */
	gpio_config_module(MODULE_SPI_FLASH, 0);
}

USB_SPI_CONFIG(usb_spi, USB_IFACE_SPI, USB_EP_SPI, 0);

/******************************************************************************
 * Support I2C bridging over USB.
 */

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 100,
		GPIO_TPM_I2C1_HOST_SCL, GPIO_TPM_I2C1_HOST_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

int usb_i2c_board_is_enabled(void) { return 1; }

/******************************************************************************
 * Define the strings used in our USB descriptors.
 */

const void *const usb_strings[] = {
	[USB_STR_DESC]         = usb_string_desc,
	[USB_STR_VENDOR]       = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]      = USB_STRING_DESC("HyperDebug"),
	[USB_STR_SERIALNO]     = 0,
	[USB_STR_VERSION]      = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("HyperDebug Shell"),
	[USB_STR_UPDATE_NAME]  = USB_STRING_DESC("Firmware update"),
	[USB_STR_I2C_NAME]     = USB_STRING_DESC("I2C"),
	[USB_STR_USART1_STREAM_NAME]  = USB_STRING_DESC("UART1"),
	[USB_STR_USART2_STREAM_NAME]  = USB_STRING_DESC("UART2"),
	[USB_STR_USART4_STREAM_NAME]  = USB_STRING_DESC("UART4"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

/******************************************************************************
 * Initialize board.
 */
static void board_init(void)
{
	/* USB to serial queues */
	queue_init(&usart1_to_usb);
	queue_init(&usb_to_usart1);
	queue_init(&usart2_to_usb);
	queue_init(&usb_to_usart2);
	queue_init(&usart4_to_usb);
	queue_init(&usb_to_usart4);

	/* UART init */
	usart_init(&usart1);
	usart_init(&usart2);
	usart_init(&usart4);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
