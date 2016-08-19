/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Polyberry board configuration */

#include "common.h"
#include "dma.h"
#include "ec_version.h"
#include "gpio.h"
#include "gpio_list.h"
#include "hooks.h"
#include "queue_policies.h"
#include "registers.h"
#include "stm32-dma.h"
#include "task.h"
#include "update_fw.h"
#include "usart-stm32f4.h"
#include "usart_tx_dma.h"
#include "usart_rx_dma.h"
#include "usb-stream.h"
#include "usb_descriptor.h"
#include "usb_dwc_hw.h"
#include "usb_dwc_console.h"
#include "usb_dwc_update.h"
#include "util.h"

/******************************************************************************
 * Define the strings used in our USB descriptors.
 */
const void *const usb_strings[] = {
	[USB_STR_DESC]		= usb_string_desc,
	[USB_STR_VENDOR]	= USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]	= USB_STRING_DESC("Polyberry"),
	[USB_STR_SERIALNO]	= USB_STRING_DESC("1234-a"),
	[USB_STR_VERSION]	= USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME]	= USB_STRING_DESC("Polyberry EC Shell"),
	[USB_STR_UPDATE_NAME]	= USB_STRING_DESC("Firmware update"),
	[USB_STR_USART1_STREAM_NAME]  = USB_STRING_DESC("Polyberry AP"),
	[USB_STR_USART2_STREAM_NAME]  = USB_STRING_DESC("Polyberry SH"),
	[USB_STR_USART5_STREAM_NAME]  = USB_STRING_DESC("Polyberry SSC"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);


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
 * Forward USART5 as a simple USB serial interface.
 */

static struct usart_config const usart5;
struct usb_stream_config const usart5_usb;

static struct queue const usart5_to_usb = QUEUE_DIRECT(64, uint8_t,
	usart5.producer, usart5_usb.consumer);
static struct queue const usb_to_usart5 = QUEUE_DIRECT(64, uint8_t,
	usart5_usb.producer, usart5.consumer);

static struct usart_config const usart5 =
	USART_CONFIG(usart5_hw,
		usart_rx_interrupt,
		usart_tx_interrupt,
		115200,
		usart5_to_usb,
		usb_to_usart5);

USB_STREAM_CONFIG(usart5_usb,
	USB_IFACE_USART5_STREAM,
	USB_STR_USART5_STREAM_NAME,
	USB_EP_USART5_STREAM,
	USB_STREAM_RX_SIZE,
	USB_STREAM_TX_SIZE,
	usb_to_usart5,
	usart5_to_usb)


/******************************************************************************
 * Support firmware upgrade over USB. We can update whichever section is not
 * the current section.
 */

/*
 * This array defines possible sections available for the firmware update.
 * The section which does not map the current executing code is picked as the
 * valid update area. The values are offsets into the flash space.
 */
const struct section_descriptor board_rw_sections[] = {
	{CONFIG_RO_MEM_OFF,
	 CONFIG_RO_MEM_OFF + CONFIG_RO_SIZE},
	{CONFIG_RW_MEM_OFF,
	 CONFIG_RW_MEM_OFF + CONFIG_RW_SIZE},
};
const struct section_descriptor * const rw_sections = board_rw_sections;
const int num_rw_sections = ARRAY_SIZE(board_rw_sections);


/*****************************************************************************
 * Initialize DWC usb enpoint arrays and setup config.
 */
struct dwc_usb usb_ctl = {
	.ep = {
		&ep0_ctl,
		&ep_console_ctl,
		&usb_update_ep_ctl,
		&usart1_usb_ep_ctl,
		&usart2_usb_ep_ctl,
		&usart5_usb_ep_ctl,
	},
	.speed = USB_SPEED_FS,
	.phy_type = USB_PHY_ULPI,
	.dma_en = 1,
	.irq = STM32_IRQ_OTG_HS,
};


#define GPIO_SET_HS(bank, number)	\
	(STM32_GPIO_OSPEEDR(GPIO_##bank) |= (0x3 << ((number) * 2)))

void board_config_post_gpio_init(void)
{
	/* We use MCO2 clock passthrough to provide a clock to USB HS */
	gpio_config_module(MODULE_MCO, 1);
	/* GPIO PC9 to high speed */
	GPIO_SET_HS(C,  9);

	if (usb_ctl.phy_type == USB_PHY_ULPI)
		gpio_set_level(GPIO_USB_MUX_SEL, 0);
	else
		gpio_set_level(GPIO_USB_MUX_SEL, 1);

	/* Set USB GPIO to high speed */
	GPIO_SET_HS(A, 11);
	GPIO_SET_HS(A, 12);

	GPIO_SET_HS(C,  3);
	GPIO_SET_HS(C,  2);
	GPIO_SET_HS(C,  0);
	GPIO_SET_HS(A,  5);

	GPIO_SET_HS(B,  5);
	GPIO_SET_HS(B, 13);
	GPIO_SET_HS(B, 12);
	GPIO_SET_HS(B,  2);
	GPIO_SET_HS(B, 10);
	GPIO_SET_HS(B,  1);
	GPIO_SET_HS(B,  0);
	GPIO_SET_HS(A,  3);
}

static void board_init(void)
{
	/* USB to serial queues */
	queue_init(&usart1_to_usb);
	queue_init(&usb_to_usart1);
	queue_init(&usart2_to_usb);
	queue_init(&usb_to_usart2);
	queue_init(&usart5_to_usb);
	queue_init(&usb_to_usart5);

	/* UART init */
	usart_init(&usart1);
	usart_init(&usart2);
	usart_init(&usart5);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
