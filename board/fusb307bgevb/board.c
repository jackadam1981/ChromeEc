/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* FUSB307BGEVB configuration */

#include "common.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "queue_policies.h"
#include "registers.h"
#include "task.h"
#include "usart-stm32f0.h"
#include "usart_tx_dma.h"
#include "usart_rx_dma.h"
#include "usb_gpio.h"
#include "usb-stream.h"
#include "util.h"
#include "usb_mux.h"
#include "usb_charge.h"
#include "usb_pd_tcpm.h"
#include "usb_pd.h"
#include "charge_state.h"
#include "tcpm.h"
#include "i2c.h"
#include "power.h"
#include "power_button.h"
#include "lcd.h"
#include "fusb307.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static void tcpc_alert_event(enum gpio_signal signal)
{
	CPRINTS("Interrupt!!!");
	schedule_deferred_pd_interrupt(0);
}

/******************************************************************************
 * Build GPIO tables and expose a subset of the GPIOs over USB.
 */
void button_event(enum gpio_signal signal);
#include "gpio_list.h"

static enum gpio_signal const usb_gpio_list[] = {
	GPIO_USER_BUTTON,
};

/*
 * This instantiates struct usb_gpio_config const usb_gpio, plus several other
 * variables, all named something beginning with usb_gpio_
 */
 USB_GPIO_CONFIG(usb_gpio,
		usb_gpio_list,
		USB_IFACE_GPIO,
		USB_EP_GPIO);

/******************************************************************************
 * Setup USART1 as a loopback device, it just echo's back anything sent to it.
 */
static struct usart_config const loopback_usart;

static struct queue const loopback_queue =
	QUEUE_DIRECT(64, uint8_t,
		     loopback_usart.producer,
		     loopback_usart.consumer);

static struct usart_rx_dma const loopback_rx_dma =
	USART_RX_DMA(STM32_DMAC_CH3, 8);

static struct usart_tx_dma const loopback_tx_dma =
	USART_TX_DMA(STM32_DMAC_CH2, 16);

static struct usart_config const loopback_usart =
	USART_CONFIG(usart1_hw,
		     loopback_rx_dma.usart_rx,
		     loopback_tx_dma.usart_tx,
		     115200,
		     0,
		     loopback_queue,
		     loopback_queue);

/******************************************************************************
 * Forward USART4 as a simple USB serial interface.
 */
static struct usart_config const forward_usart;
struct usb_stream_config const forward_usb;

static struct queue const usart_to_usb = QUEUE_DIRECT(64, uint8_t,
						      forward_usart.producer,
						      forward_usb.consumer);
static struct queue const usb_to_usart = QUEUE_DIRECT(64, uint8_t,
						      forward_usb.producer,
						      forward_usart.consumer);

static struct usart_tx_dma const forward_tx_dma =
	USART_TX_DMA(STM32_DMAC_CH7, 16);

static struct usart_config const forward_usart =
	USART_CONFIG(usart4_hw,
		     usart_rx_interrupt,
		     forward_tx_dma.usart_tx,
		     115200,
		     0,
		     usart_to_usb,
		     usb_to_usart);

#define USB_STREAM_RX_SIZE	16
#define USB_STREAM_TX_SIZE	16

USB_STREAM_CONFIG(forward_usb,
		  USB_IFACE_STREAM,
		  USB_STR_STREAM_NAME,
		  USB_EP_STREAM,
		  USB_STREAM_RX_SIZE,
		  USB_STREAM_TX_SIZE,
		  usb_to_usart,
		  usart_to_usb)

/******************************************************************************
 * Handle button presses by cycling the LEDs on the board.  Also run a tick
 * handler to cycle them when they are not actively under USB control.
 */
static void button_event_deferred(void)
{
	static int count;
	CPRINTS("Button event: %d", count);
	
	lcd_setCursor(0, count & 0x03);
	pd_request_source_voltage(0, 5);

	count ++;
}
DECLARE_DEFERRED(button_event_deferred);

void button_event(enum gpio_signal signal)
{
	hook_call_deferred(&button_event_deferred_data, 100 * MSEC);
}

/* void usb_gpio_tick(void)
{
	if (usb_gpio.state->set_mask || usb_gpio.state->clear_mask)
		return;

	button_event(0);
}
DECLARE_HOOK(HOOK_TICK, usb_gpio_tick, HOOK_PRIO_DEFAULT); */

/******************************************************************************
 * Define the strings used in our USB descriptors.
 */
const void *const usb_strings[] = {
	[USB_STR_DESC]         = usb_string_desc,
	[USB_STR_VENDOR]       = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]      = USB_STRING_DESC("fusb307bgevb"),
	[USB_STR_VERSION]      = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_STREAM_NAME]  = USB_STRING_DESC("Forward"),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("Shell"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

/******************************************************************************
 * I2C interface.
 */
const struct i2c_port_t i2c_ports[] = {
	{"tcpc", I2C_PORT_TCPC, 400 /* kHz */, GPIO_I2C2_SCL, GPIO_I2C2_SDA}
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/******************************************************************************
 * PD
 */
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_TCPC,
			.addr_flags = FUSB307_I2C_SLAVE_ADDR_FLAGS,
		},
		.drv = &fusb307_tcpm_drv,
	},
};


uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;
	
	if (!gpio_get_level(GPIO_USB_C0_PD_INT_ODL))
		status |= PD_STATUS_TCPC_ALERT_0;

	return status;
}

void board_reset_pd_mcu(void)
{
}

int pd_snk_is_vbus_provided(int port)
{
	/* TODO(b:138352732): read IT8801 GPIO EN_USBC_CHARGE_L */
	return EC_ERROR_UNIMPLEMENTED;
}

void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
}


int board_set_active_charge_port(int charge_port)
{
	return EC_SUCCESS;
}

static uint8_t vbus_en;
int board_vbus_source_enabled(int port)
{
	return vbus_en;
}

void pd_set_input_current_limit(int port, uint32_t max_ma,
				uint32_t supply_voltage)
{
	/* No battery, nothing to do */
	return;
}

void pd_power_supply_reset(int port)
{
	/* Disable VBUS 
	fusb307_power_supply_reset(port);*/
}

int pd_set_power_supply_ready(int port)
{
	return EC_SUCCESS;
}

int pd_board_checks(void)
{
	return EC_SUCCESS;
}

#ifdef CONFIG_USBC_VCONN_SWAP
int pd_check_vconn_swap(int port)
{
	/*
	 * Allow vconn swap as long as we are acting as a dual role device,
	 * otherwise assume our role is fixed (not in S0 or console command
	 * to fix our role).
	 */
	/* return pd_get_dual_role(port) == PD_DRP_TOGGLE_ON;*/
}
#endif 

/******************************************************************************
 * Initialize board.
 */
static void board_init(void)
{
	/* Enable button interrupts */
	gpio_enable_interrupt(GPIO_USER_BUTTON);
	/* Enable TCPC alert interrupts */
	gpio_enable_interrupt(GPIO_USB_C0_PD_INT_ODL);

	lcd_init(20, 4, 0);
	lcd_setCursor(0, 0);
	lcd_printString("Hi LCD test");
	lcd_setCursor(0, 1);
	lcd_printString("Test 2nd row");
	queue_init(&loopback_queue);
	queue_init(&usart_to_usb);
	queue_init(&usb_to_usart);
	usart_init(&loopback_usart);
	usart_init(&forward_usart);

}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
