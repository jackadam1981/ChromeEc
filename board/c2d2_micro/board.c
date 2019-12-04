/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* C2D2 Micro configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "queue.h"
#include "queue_policies.h"
#include "registers.h"
#include "timer.h"
#include "usart-stm32f0.h"
#include "usart_tx_dma.h"
#include "usart_rx_dma.h"
#include "usb_descriptor.h"
#include "usb-stream.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

#include "gpio_list.h"

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= STM32_RCC_SYSCFGEN;

	/*
	 * the DMA mapping is :
	 *  Chan 3 : USART3_RX
	 *  Chan 5 : USART2_RX
	 *  Chan 6 : USART4_RX (Disable)
	 *  Chan 6 : SPI2_RX
	 *  Chan 7 : SPI2_TX
	 *
	 *  i2c : no dma
	 *  tim16/17: no dma
	 */
	STM32_SYSCFG_CFGR1 |= BIT(10);  /* Remap USART1 RX/TX DMA */

	/* /\* Remap SPI2 to DMA channels 6 and 7 *\/ */
	/* /\* STM32F072 SPI2 defaults to using DMA channels 4 and 5 *\/ */
	/* /\* but cros_ec hardcodes a 6/7 assumption in registers.h *\/ */
	/* STM32_SYSCFG_CFGR1 |= BIT(24); */

}

/******************************************************************************
 * Define the strings used in our USB descriptors.
 */

const void *const usb_strings[] = {
	[USB_STR_DESC]         = usb_string_desc,
	[USB_STR_VENDOR]       = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]      = USB_STRING_DESC("C2D2 Micro"),
	/* This gets filled in at runtime. */
	[USB_STR_SERIALNO]     = USB_STRING_DESC(""),
	[USB_STR_VERSION]      = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("C2D2 Shell"),
	[USB_STR_USART1_STREAM_NAME]  = USB_STRING_DESC("EC"),
	[USB_STR_USART3_STREAM_NAME]  = USB_STRING_DESC("CPU"),
	[USB_STR_USART4_STREAM_NAME]  = USB_STRING_DESC("Cr50"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

/******************************************************************************
 ** ADC channels
*/
const struct adc_t adc_channels[] = {
	/* Sensing the H1's voltage at the DUT side.  Converted to mV. */
	[ADC_H1_VREF] = {
		.name = "H1_VREF",
		.factor_mul = 3300,
		.factor_div = 4096,
		.shift = 0,
		.channel = STM32_AIN(3),
	},

	[ADC_EC_VREF] = {
		.name = "EC_VREF",
		.factor_mul = 3300,
		.factor_div = 4096,
		.shift = 0,
		.channel = STM32_AIN(4),
	}
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/******************************************************************************
 * Forward UARTs as a USB serial interface.
 */

#define USB_STREAM_RX_SIZE	32
#define USB_STREAM_TX_SIZE	64

/******************************************************************************
 * Forward USART1 (EC) as a simple USB serial interface.
 */

static struct usart_config const usart1;
struct usb_stream_config const usart1_usb;

static struct queue const usart1_to_usb = QUEUE_DIRECT(128, uint8_t,
	usart1.producer, usart1_usb.consumer);
static struct queue const usb_to_usart1 = QUEUE_DIRECT(64, uint8_t,
	usart1_usb.producer, usart1.consumer);

static struct usart_rx_dma const usart1_rx_dma =
	USART_RX_DMA(STM32_DMAC_CH5, 32);

static struct usart_config const usart1 =
	USART_CONFIG(usart1_hw,
		usart1_rx_dma.usart_rx,
		usart_tx_interrupt,
		115200,
		0,
		usart1_to_usb,
		usb_to_usart1);

USB_STREAM_CONFIG_USART_IFACE(usart1_usb,
	USB_IFACE_USART1_STREAM,
	USB_STR_USART1_STREAM_NAME,
	USB_EP_USART1_STREAM,
	USB_STREAM_RX_SIZE,
	USB_STREAM_TX_SIZE,
	usb_to_usart1,
	usart1_to_usb,
	usart1)


/******************************************************************************
 * Forward USART3 (CPU) as a simple USB serial interface.
 */

static struct usart_config const usart3;
struct usb_stream_config const usart3_usb;

static struct queue const usart3_to_usb = QUEUE_DIRECT(1024, uint8_t,
	usart3.producer, usart3_usb.consumer);
static struct queue const usb_to_usart3 = QUEUE_DIRECT(64, uint8_t,
	usart3_usb.producer, usart3.consumer);

static struct usart_rx_dma const usart3_rx_dma =
	USART_RX_DMA(STM32_DMAC_CH3, 32);

static struct usart_config const usart3 =
	USART_CONFIG(usart3_hw,
		usart3_rx_dma.usart_rx,
		usart_tx_interrupt,
		115200,
		0,
		usart3_to_usb,
		usb_to_usart3);

USB_STREAM_CONFIG_USART_IFACE(usart3_usb,
	USB_IFACE_USART3_STREAM,
	USB_STR_USART3_STREAM_NAME,
	USB_EP_USART3_STREAM,
	USB_STREAM_RX_SIZE,
	USB_STREAM_TX_SIZE,
	usb_to_usart3,
	usart3_to_usb,
	usart3)


/******************************************************************************
 * Forward USART4 (cr50) as a simple USB serial interface.
 *  We cannot enable DMA due to lack of DMA channels.
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

USB_STREAM_CONFIG_USART_IFACE(usart4_usb,
	USB_IFACE_USART4_STREAM,
	USB_STR_USART4_STREAM_NAME,
	USB_EP_USART4_STREAM,
	USB_STREAM_RX_SIZE,
	USB_STREAM_TX_SIZE,
	usb_to_usart4,
	usart4_to_usb,
	usart4)

/******************************************************************************
 * Check parity setting on usarts.
 */
static int command_uart_parity(int argc, char **argv)
{
	int parity = 0, newparity;
	struct usart_config const *usart;
	char *e;

	if ((argc < 2) || (argc > 3))
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "usart1"))
		usart = &usart1;
	else if (!strcasecmp(argv[1], "usart3"))
		usart = &usart3;
	else if (!strcasecmp(argv[1], "usart4"))
		usart = &usart4;
	else
		return EC_ERROR_PARAM1;

	if (argc == 3) {
		parity = strtoi(argv[2], &e, 0);
		if (*e || (parity < 0) || (parity > 2))
			return EC_ERROR_PARAM2;

		usart_set_parity(usart, parity);
	}

	newparity = usart_get_parity(usart);
	ccprintf("Parity on %s is %d.\n", argv[1], newparity);

	if ((argc == 3) && (newparity != parity))
		return EC_ERROR_UNKNOWN;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(parity, command_uart_parity,
			"usart[1|3|4] [0|1|2]",
			"Set parity on uart");

/******************************************************************************
 * Set baud rate setting on usarts.
 */
static int command_uart_baud(int argc, char **argv)
{
	int baud = 0;
	struct usart_config const *usart;
	char *e;

	if ((argc < 2) || (argc > 3))
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "usart1"))
		usart = &usart1;
	else if (!strcasecmp(argv[1], "usart3"))
		usart = &usart3;
	else if (!strcasecmp(argv[1], "usart4"))
		usart = &usart4;
	else
		return EC_ERROR_PARAM1;

	baud = strtoi(argv[2], &e, 0);
	if (*e || baud < 0)
		return EC_ERROR_PARAM2;

	usart_set_baud(usart, baud);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(baud, command_uart_baud,
			"usart[1|3|4] rate",
			"Set baud rate on uart");

/******************************************************************************
 * Hold the usart pins low while disabling it, or return it to normal.
 */
static int command_hold_usart_low(int argc, char **argv)
{
	/* Each bit represents if that port is being held low */
	static int usart_status;

	const struct usart_config *usart;
	int usart_mask;
	enum gpio_signal tx, rx;

	if (argc > 3 || argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "usart1")) {
		usart = &usart1;
		usart_mask = 1 << 1;
		tx = GPIO_UART_DBG_TX_EC_RX_SCL;
		rx = GPIO_UART_EC_TX_DBG_RX_SDA;
	} else if (!strcasecmp(argv[1], "usart3")) {
		usart = &usart3;
		usart_mask = 1 << 3;
		tx = GPIO_UART_DBG_TX_AP_RX_INA_SCL;
		rx = GPIO_UART_AP_TX_DBG_RX_INA_SDA;
	} else if (!strcasecmp(argv[1], "usart4")) {
		usart = &usart4;
		usart_mask = 1 << 4;
		tx = GPIO_UART_DBG_TX_H1_RX;
		rx = GPIO_UART_H1_TX_DBG_RX;
	} else {
		return EC_ERROR_PARAM1;
	}

	/* Updating the status of this port */
	if (argc == 3) {
		char *e;
		const int hold_low = strtoi(argv[2], &e, 0);

		if (*e || (hold_low < 0) || (hold_low > 1))
			return EC_ERROR_PARAM2;

		if (!!(usart_status & usart_mask) == hold_low) {
			/* Do nothing since there is no change */
		} else if (hold_low) {
			/*
			 * Only one USART can be held low at a time, because
			 * re-initializing one USART will pull all of the USART
			 * GPIO pins back into alternate mode.
			 */
			if (usart_status)
				return EC_ERROR_BUSY;

			/*
			 * Shutdown the USB uart,
			 * turn off alternate mode, then set the RX line
			 * pin to output low to enter UART programming mode.
			 */
			usart_shutdown(usart);
			gpio_config_pin(MODULE_USART, rx, 0);
			gpio_config_pin(MODULE_USART, tx, 0);
			gpio_set_flags(rx, GPIO_OUT_LOW);

			usart_status |= usart_mask;
		} else {
			/*
			 * This will reset the alternate mode of the
			 * GPIO pins appropriately and restart USB UART
			 */
			usart_init(usart);

			/*
			 * Since only one USART can be held low at a time, the
			 * uart_status will always be 0 after this call.
			 */
			usart_status = 0;
		}
	}

	/* Print status for get and set case. */
	ccprintf("USART status: %s\n",
			usart_status & usart_mask ? "held low" : "normal");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(hold_usart_low, command_hold_usart_low,
			"usart[1|3|4] [0|1]?",
			"Get/set the hold-low state for usart port");

/******************************************************************************
 * Support SPI bridging over USB, this requires usb_spi_board_enable and
 * usb_spi_board_disable to be defined to enable and disable the SPI bridge.
 */

#if 0 
/* SPI devices */
const struct spi_device_t spi_devices[] = {
	{ CONFIG_SPI_FLASH_PORT, 1, GPIO_SPI_CS},
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

void usb_spi_board_enable(struct usb_spi_config const *config)
{
	/* Configure SPI GPIOs */
	gpio_config_module(MODULE_SPI_FLASH, 1);

	/* Set all four SPI pins to high speed */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0xff000000;

	/* Enable clocks to SPI2 module */
	STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI2;

	/* Reset SPI2 */
	STM32_RCC_APB1RSTR |= STM32_RCC_PB1_SPI2;
	STM32_RCC_APB1RSTR &= ~STM32_RCC_PB1_SPI2;

	spi_enable(CONFIG_SPI_FLASH_PORT, 1);
}

void usb_spi_board_disable(struct usb_spi_config const *config)
{
	spi_enable(CONFIG_SPI_FLASH_PORT, 0);

	/* Disable clocks to SPI2 module */
	STM32_RCC_APB1ENR &= ~STM32_RCC_PB1_SPI2;

	/* Release SPI GPIOs */
	gpio_config_module(MODULE_SPI_FLASH, 0);
}

USB_SPI_CONFIG(usb_spi, USB_IFACE_SPI, USB_EP_SPI);
#endif /* 0 */

static void set_mux_ctl(bool enable)
{
	if (enable) {
		/* Disable SPI interface and swing mux for UARTs. */
		gpio_set_level(GPIO_C2D2_MUX_UART_ODL, 0);
	} else {
		gpio_set_level(GPIO_C2D2_MUX_UART_ODL, 1);
	}
}

static int command_muxctl(int argc, char **argv)
{
	int assert;

	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;

	if (parse_bool(argv[1], &assert))
		return EC_ERROR_PARAM1;

	set_mux_ctl(assert);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(muxctl, command_muxctl,
			"muxctl <bool>",
			"Swing C2D2 mux for UARTs(on) vs SPI(off)");

static void board_init(void)
{
	/* Connect the VREFs in order to sample them */
	gpio_set_level(GPIO_EN_SPIVREF_RSVD_H1VREF_H1_RST, 1);
	gpio_set_level(GPIO_EN_SPIVREF_HOLDN_ECVREF_H1_PWRBTN, 1);

	/* USB to serial queues */
	queue_init(&usart1_to_usb);
	queue_init(&usb_to_usart1);
	queue_init(&usart3_to_usb);
	queue_init(&usb_to_usart3);
	queue_init(&usart4_to_usb);
	queue_init(&usb_to_usart4);

	/* UART init */
	usart_init(&usart1);
	usart_init(&usart3);
	usart_init(&usart4);

	/* Shutdown SPI by default. */
	/* usb_spi_enable(&usb_spi, 0); */
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

enum vref {
	OFF,
	PP1800,
	PP3300,
	VREF_COUNT,
};

static enum vref ec_vref;
static enum vref h1_vref;

static enum vref adc_to_vref(int adc)
{
	if (adc > 2300)
		return PP3300;
	else if (adc > 1500)
		return PP1800;
	else
		return OFF;
}

static int vref_state;
static void get_vrefs(void)
{
	int h1_vref_mv = adc_read_channel(ADC_H1_VREF);
	int ec_vref_mv = adc_read_channel(ADC_EC_VREF);

	CPRINTF("H1 VREF: %d\n", h1_vref_mv == ADC_READ_ERROR ? -1 : h1_vref_mv);
	CPRINTF("EC VREF: %d\n", ec_vref_mv == ADC_READ_ERROR ? -1 : ec_vref_mv);

	if (h1_vref_mv != ADC_READ_ERROR) {
		h1_vref = adc_to_vref(h1_vref_mv);
		gpio_set_level(GPIO_SEL_SPIVREF_H1VREF_3V3, h1_vref == PP3300);

		if (h1_vref == OFF)
			vref_state &= BIT(0);
		else
			vref_state |= BIT(0);
	}

	if (ec_vref_mv != ADC_READ_ERROR) {
		ec_vref = adc_to_vref(ec_vref_mv);
		gpio_set_level(GPIO_SEL_SPIVREF_ECVREF_3V3, ec_vref == PP3300);

		if (ec_vref == OFF)
			vref_state &= BIT(1);
		else
			vref_state |= BIT(1);
	}

	if (!vref_state && gpio_get_level(GPIO_EN_MISO_MOSI_H1_UART)) {
		CPRINTS("Turning off UARTs. (No VREFs)");
		gpio_set_level(GPIO_EN_MISO_MOSI_H1_UART, 0);
		gpio_set_level(GPIO_EN_CLK_CSN_EC_UART, 0);
	} else if (vref_state && !gpio_get_level(GPIO_EN_MISO_MOSI_H1_UART)) {
		CPRINTS("Turning on UARTs. (Got VREFs)");
		gpio_set_level(GPIO_EN_MISO_MOSI_H1_UART, 1);
		gpio_set_level(GPIO_EN_CLK_CSN_EC_UART, 1);
	}
}
DECLARE_HOOK(HOOK_SECOND, get_vrefs, HOOK_PRIO_DEFAULT);

/* static void set_vref(void) */
/* { */
/* 	/\* Read the VREFs *\/ */
/* 	gpio_set_flags(GPIO_SPIVREF_RSVD_H1VREF_H1_RST_ODL, GPIO_ANALOG); */
/* 	gpio_set_flags(GPIO_SPIVREF_HOLDN_ECVREF_H1_PWRBTN_ODL, GPIO_ANALOG); */

/* } */
