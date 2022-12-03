/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* HyperDebug board configuration */

#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "i2c.h"
#include "queue_policies.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "usart-stm32l5.h"
#include "usb-stream.h"
#include "usb_hw.h"
#include "usb_spi.h"

#include <stdio.h>

/* Must come after other header files and interrupt handler declarations */
#include "gpio_list.h"

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= STM32_RCC_SYSCFGEN;
}

/******************************************************************************
 * Forward UARTs as a USB serial interface.
 */

#define USB_STREAM_RX_SIZE 16
#define USB_STREAM_TX_SIZE 16

/******************************************************************************
 * Forward USART2 as a simple USB serial interface.
 */

static struct usart_config const usart2;
struct usb_stream_config const usart2_usb;

static struct queue const usart2_to_usb =
	QUEUE_DIRECT(64, uint8_t, usart2.producer, usart2_usb.consumer);
static struct queue const usb_to_usart2 =
	QUEUE_DIRECT(64, uint8_t, usart2_usb.producer, usart2.consumer);

static struct usart_config const usart2 =
	USART_CONFIG(usart2_hw, usart_rx_interrupt, usart_tx_interrupt, 115200,
		     0, usart2_to_usb, usb_to_usart2);

USB_STREAM_CONFIG(usart2_usb, USB_IFACE_USART2_STREAM,
		  USB_STR_USART2_STREAM_NAME, USB_EP_USART2_STREAM,
		  USB_STREAM_RX_SIZE, USB_STREAM_TX_SIZE, usb_to_usart2,
		  usart2_to_usb)

/******************************************************************************
 * Forward USART3 as a simple USB serial interface.
 */

static struct usart_config const usart3;
struct usb_stream_config const usart3_usb;

static struct queue const usart3_to_usb =
	QUEUE_DIRECT(64, uint8_t, usart3.producer, usart3_usb.consumer);
static struct queue const usb_to_usart3 =
	QUEUE_DIRECT(64, uint8_t, usart3_usb.producer, usart3.consumer);

static struct usart_config const usart3 =
	USART_CONFIG(usart3_hw, usart_rx_interrupt, usart_tx_interrupt, 115200,
		     0, usart3_to_usb, usb_to_usart3);

USB_STREAM_CONFIG(usart3_usb, USB_IFACE_USART3_STREAM,
		  USB_STR_USART3_STREAM_NAME, USB_EP_USART3_STREAM,
		  USB_STREAM_RX_SIZE, USB_STREAM_TX_SIZE, usb_to_usart3,
		  usart3_to_usb)

/******************************************************************************
 * Forward USART4 as a simple USB serial interface.
 */

static struct usart_config const usart4;
struct usb_stream_config const usart4_usb;

static struct queue const usart4_to_usb =
	QUEUE_DIRECT(64, uint8_t, usart4.producer, usart4_usb.consumer);
static struct queue const usb_to_usart4 =
	QUEUE_DIRECT(64, uint8_t, usart4_usb.producer, usart4.consumer);

static struct usart_config const usart4 =
	USART_CONFIG(usart4_hw, usart_rx_interrupt, usart_tx_interrupt, 115200,
		     0, usart4_to_usb, usb_to_usart4);

USB_STREAM_CONFIG(usart4_usb, USB_IFACE_USART4_STREAM,
		  USB_STR_USART4_STREAM_NAME, USB_EP_USART4_STREAM,
		  USB_STREAM_RX_SIZE, USB_STREAM_TX_SIZE, usb_to_usart4,
		  usart4_to_usb)

/******************************************************************************
 * Forward USART5 as a simple USB serial interface.
 */

static struct usart_config const usart5;
struct usb_stream_config const usart5_usb;

static struct queue const usart5_to_usb =
	QUEUE_DIRECT(64, uint8_t, usart5.producer, usart5_usb.consumer);
static struct queue const usb_to_usart5 =
	QUEUE_DIRECT(64, uint8_t, usart5_usb.producer, usart5.consumer);

static struct usart_config const usart5 =
	USART_CONFIG(usart5_hw, usart_rx_interrupt, usart_tx_interrupt, 115200,
		     0, usart5_to_usb, usb_to_usart5);

USB_STREAM_CONFIG(usart5_usb, USB_IFACE_USART5_STREAM,
		  USB_STR_USART5_STREAM_NAME, USB_EP_USART5_STREAM,
		  USB_STREAM_RX_SIZE, USB_STREAM_TX_SIZE, usb_to_usart5,
		  usart5_to_usb)

/******************************************************************************
 * Support SPI bridging over USB, this requires usb_spi_board_enable and
 * usb_spi_board_disable to be defined to enable and disable the SPI bridge.
 */

/* SPI devices */
const struct spi_device_t spi_devices[] = {
	{ 1 /* SPI2 */, 3, GPIO_SPI2_CS },
	{ -1 /* OCTO_SPI */, -1, GPIO_QSPI_DEV_CS_L /*GPIO_SPI2_CS*/ },
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

void usb_spi_board_enable(struct usb_spi_config const *config)
{
	/* Configure SPI GPIOs */
	gpio_config_module(MODULE_SPI, 1);
	gpio_config_module(MODULE_SPI_FLASH, 1);

	/* Set all SPI pins to high speed */
	STM32_GPIO_OSPEEDR(GPIO_F) |= 0xFFF00000;
	STM32_GPIO_OSPEEDR(GPIO_D) |= 0x000000C3;
	STM32_GPIO_OSPEEDR(GPIO_C) |= 0x000000F0;

	/* Enable clocks to SPI2 module */
	STM32_RCC_APB1ENR1 |= STM32_RCC_APB1ENR1_SPI2EN;

	/* Reset SPI2 */
	STM32_RCC_APB1RSTR1 |= STM32_RCC_APB1RSTR1_SPI2RST;
	STM32_RCC_APB1RSTR1 &= ~STM32_RCC_APB1RSTR1_SPI2RST;

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
	{ .name = "controller",
	  .port = I2C_PORT_CONTROLLER,
	  .kbps = 100,
	  .scl = GPIO_TPM_I2C1_HOST_SCL,
	  .sda = GPIO_TPM_I2C1_HOST_SDA },
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

int usb_i2c_board_is_enabled(void)
{
	return 1;
}

/******************************************************************************
 * Define the strings used in our USB descriptors.
 */

const void *const usb_strings[] = {
	[USB_STR_DESC] = usb_string_desc,
	[USB_STR_VENDOR] = USB_STRING_DESC("Google LLC"),
	[USB_STR_PRODUCT] = USB_STRING_DESC("HyperDebug"),
	[USB_STR_SERIALNO] = 0,
	[USB_STR_VERSION] = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("HyperDebug Shell"),
	[USB_STR_SPI_NAME] = USB_STRING_DESC("SPI"),
	[USB_STR_I2C_NAME] = USB_STRING_DESC("I2C"),
	[USB_STR_USART2_STREAM_NAME] = USB_STRING_DESC("UART2"),
	[USB_STR_USART3_STREAM_NAME] = USB_STRING_DESC("UART3"),
	[USB_STR_USART4_STREAM_NAME] = USB_STRING_DESC("UART4"),
	[USB_STR_USART5_STREAM_NAME] = USB_STRING_DESC("UART5"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);


int usb_spi_board_transaction(const struct spi_device_t *spi_device,
			      const uint8_t *txdata, int txlen, uint8_t *rxdata,
			      int rxlen) {
	int rv = EC_SUCCESS;
	bool spi_chip_select_already_asserted;
	ccprintf("spi: tx=%d rx=%d\n", txlen, rxlen);

	spi_chip_select_already_asserted = !gpio_get_level(spi_device->gpio_cs);

	/* Drive SS low */
	gpio_set_level(spi_device->gpio_cs, 0);

	if (!rxlen && !txlen) {
		// No opneration
	} else if (rxlen == SPI_READBACK_ALL) {
		ccprintf("Full duplex not supported by OctoSPI hardware\n");
		rv = EC_ERROR_BUSY;
	} else if (!rxlen) {
		STM32_OCTOSPI_CR = 0x000000001; // Enable OCTOSPI, indirect write mode
		while (STM32_OCTOSPI_SR & 0x20);

		STM32_OCTOSPI_DLR = txlen - 1;
		STM32_OCTOSPI_CCR = 0x01000000; // No instruction or address, only data
		STM32_OCTOSPI_TCR = 0; // Dummy cycles
		STM32_OCTOSPI_IR = 0x01020304;
		STM32_OCTOSPI_AR = 0x14131211;
		for (int i = 0; i < txlen; i += 4) {
			uint32_t value = 0;
			for (int j = 0; j < 4; j++) {
				if (i + j < txlen)
					value |= txdata[i + j] << (j * 8);
			}
			STM32_OCTOSPI_DR = value;
		}
		while (!(STM32_OCTOSPI_SR & 0x02));
		STM32_OCTOSPI_FCR = 0x02;
	} else if (txlen == 0) {
		ccprintf("Unhandled\n");
		rv = EC_ERROR_BUSY;
	} else if (txlen <= 8) {
		uint32_t instruction = 0, address = 0;
		STM32_OCTOSPI_CR = 0x010000001; // Enable OCTOSPI, indirect read mode
		while (STM32_OCTOSPI_SR & 0x20);

		STM32_OCTOSPI_DLR = rxlen - 1;
		STM32_OCTOSPI_TCR = 0; // Dummy cycles
		if (txlen == 0) {
		} else if (txlen <= 4) {
			STM32_OCTOSPI_CCR = 0x01000001 | (txlen - 1) << 4;
			for (int i = 0; i < txlen; i++) {
				instruction <<= 8;
				instruction |= txdata[i];
			}
		} else {
			STM32_OCTOSPI_CCR = 0x01003101 | (txlen - 5) << 4;
			//STM32_OCTOSPI_ABR = 0x31323334;
			for (int i = 0; i < txlen - 4; i++) {
				instruction <<= 8;
				instruction |= txdata[i];
			}
			for (int i = 0; i < 4; i++) {
				address <<= 8;
				address |= txdata[txlen - 4 + i];
			}
		}
		STM32_OCTOSPI_IR = instruction;
		STM32_OCTOSPI_AR = address;
		for (int i = 0; i < rxlen; i += 4) {
			uint32_t value = STM32_OCTOSPI_DR;;
			for (int j = 0; j < 4; j++) {
				if (i + j < rxlen)
					rxdata[i + j] = value >> (j * 8);
			}
		}
		while (!(STM32_OCTOSPI_SR & 0x02));
		STM32_OCTOSPI_FCR = 0x02;
	} else {
		ccprintf("Unhandled\n");
		rv = EC_ERROR_BUSY;
	}
	if (!spi_chip_select_already_asserted) {
		/* Drive SS high */
		gpio_set_level(spi_device->gpio_cs, 1);
	}
	return rv;
}

/******************************************************************************
 * Initialize board.
 */

static void board_init(void)
{
	STM32_GPIO_BSRR(STM32_GPIOE_BASE) |= 0x0000FF00;

	/* We know VDDIO2 is present, enable the GPIO circuit. */
	STM32_PWR_CR2 |= STM32_PWR_CR2_IOSV;

	/* USB to serial queues */
	queue_init(&usart2_to_usb);
	queue_init(&usb_to_usart2);
	queue_init(&usart3_to_usb);
	queue_init(&usb_to_usart3);
	queue_init(&usart4_to_usb);
	queue_init(&usb_to_usart4);
	queue_init(&usart5_to_usb);
	queue_init(&usb_to_usart5);

	STM32_GPIO_BSRR(STM32_GPIOE_BASE) |= 0x0F000000;
	/* UART init */
	usart_init(&usart2);
	usart_init(&usart3);
	usart_init(&usart4);
	usart_init(&usart5);

	/* Structured endpoints */
	usb_spi_enable(&usb_spi, 1);
	STM32_GPIO_BSRR(STM32_GPIOE_BASE) |= 0xF0000000;

	/* Enable OCTOSPI */
	gpio_config_module(MODULE_SPI_FLASH, 1);
	STM32_RCC_AHB3ENR |= STM32_RCC_AHB3ENR_QSPIEN;
	while (STM32_OCTOSPI_SR & 0x20);

	STM32_OCTOSPI_DCR1 = 0x021F0000;
	while (STM32_OCTOSPI_SR & 0x20);
	STM32_OCTOSPI_DCR2 = 63; // Clock prescaler (max value 255)
	while (STM32_OCTOSPI_SR & 0x20);
	

}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

const char *board_read_serial(void)
{
	const uint32_t *stm32_unique_id =
		(const uint32_t *)STM32_UNIQUE_ID_BASE;
	static char serial[13];

	// Compute 12 hex digits from three factory programmed 32-bit "Unique
	// ID" words in a manner that has been observed to be consistent with
	// how the STM DFU ROM bootloader presents its serial number.  This
	// means that the serial number of any particular HyperDebug board will
	// remain the same as it enters and leaves DFU mode for software
	// upgrade.
	int rc = snprintf(serial, sizeof(serial), "%08X%04X",
			  stm32_unique_id[0] + stm32_unique_id[2],
			  stm32_unique_id[1] >> 16);
	if (12 != rc)
		return NULL;
	return serial;
}

/**
 * Find a GPIO signal by name.
 *
 * This is copied from gpio.c unfortunately, as it is static over there.
 *
 * @param name		Signal name to find
 *
 * @return the signal index, or GPIO_COUNT if no match.
 */
static enum gpio_signal find_signal_by_name(const char *name)
{
	int i;

	if (!name || !*name)
		return GPIO_COUNT;

	for (i = 0; i < GPIO_COUNT; i++)
		if (gpio_is_implemented(i) &&
		    !strcasecmp(name, gpio_get_name(i)))
			return i;

	return GPIO_COUNT;
}

/*
 * Set the mode of a GPIO pin: input/opendrain/pushpull.
 */
static int command_gpio_mode(int argc, const char **argv)
{
	int gpio;
	int flags;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	gpio = find_signal_by_name(argv[1]);
	if (gpio == GPIO_COUNT)
		return EC_ERROR_PARAM1;
	flags = gpio_get_flags(gpio);

	flags = flags & ~(GPIO_INPUT | GPIO_OUTPUT | GPIO_OPEN_DRAIN);
	if (strcasecmp(argv[2], "input") == 0)
		flags |= GPIO_INPUT;
	else if (strcasecmp(argv[2], "opendrain") == 0)
		flags |= GPIO_OUTPUT | GPIO_OPEN_DRAIN;
	else if (strcasecmp(argv[2], "pushpull") == 0)
		flags |= GPIO_OUTPUT;
	else
		return EC_ERROR_PARAM2;

	/* Update GPIO flags. */
	gpio_set_flags(gpio, flags);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND_FLAGS(gpiomode, command_gpio_mode,
			      "name <input | opendrain | pushpull>",
			      "Set a GPIO mode", CMD_FLAG_RESTRICTED);

/*
 * Set the weak pulling of a GPIO pin: up/down/none.
 */
static int command_gpio_pull_mode(int argc, const char **argv)
{
	int gpio;
	int flags;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	gpio = find_signal_by_name(argv[1]);
	if (gpio == GPIO_COUNT)
		return EC_ERROR_PARAM1;
	flags = gpio_get_flags(gpio);

	flags = flags & ~(GPIO_PULL_UP | GPIO_PULL_DOWN);
	if (strcasecmp(argv[2], "none") == 0)
		;
	else if (strcasecmp(argv[2], "up") == 0)
		flags |= GPIO_PULL_UP;
	else if (strcasecmp(argv[2], "down") == 0)
		flags |= GPIO_PULL_DOWN;
	else
		return EC_ERROR_PARAM2;

	/* Update GPIO flags. */
	gpio_set_flags(gpio, flags);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND_FLAGS(gpiopullmode, command_gpio_pull_mode,
			      "name <none | up | down>",
			      "Set a GPIO weak pull mode", CMD_FLAG_RESTRICTED);
