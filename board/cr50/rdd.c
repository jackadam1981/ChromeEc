/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "gpio.h"
#include "rdd.h"
#include "registers.h"
#include "uartn.h"
#include "usb_api.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)

static int enable;

/* If the UART TX is enabled the pinmux select will have a non-zero value */
static int uart_enabled(int uart)
{
	if (uart == UART_AP)
		return GREAD(PINMUX, DIOA7_SEL);
	return GREAD(PINMUX, DIOB5_SEL);
}

static void uart_set_tx_out(int uart, int signal)
{
	/* Enable or disable the TX output */
	if (uart == UART_AP)
		GWRITE(PINMUX, DIOA7_SEL, signal);
	else
		GWRITE(PINMUX, DIOB5_SEL, signal);
}

static int servo_is_connected(void)
{
	return ((!uart_enabled(UART_EC) && gpio_get_level(GPIO_SERVO_UART2)) ||
		(!uart_enabled(UART_AP) && gpio_get_level(GPIO_SERVO_UART1)));
}

void uartn_tx_connect(int uart, enum gpio_signal signal)
{
	enum gpio_signal servo_int;
	int tx_out;

	if (!enable) {
		/*
		 * If we are not trying to enable the UART disable the
		 * interrupt.
		 */
		gpio_disable_interrupt(signal);
		return;
	}

	if (servo_is_connected()) {
		CPRINTS("Servo is attached cannot enable %s UART",
			uart == UART_AP ? "AP" : "EC");
		return;
	}

	if (uart == UART_AP) {
		servo_int = GPIO_SERVO_UART1;
		tx_out = GC_PINMUX_UART1_TX_SEL;
	} else {
		servo_int = GPIO_SERVO_UART2;
		tx_out = GC_PINMUX_UART2_TX_SEL;
	}

	if (gpio_get_level(signal)) {
		/* Disable power interrupts on uart */
		gpio_disable_interrupt(servo_int);
		gpio_disable_interrupt(signal);

		/* Enable UART output */
		uart_set_tx_out(uart, tx_out);
	} else if (!uart_enabled(uart)) {
		CPRINTS("%s is powered off", uart == UART_AP ? "AP" : "EC");
		gpio_enable_interrupt(servo_int);
		gpio_enable_interrupt(signal);
	}
}

void uartn_tx_disconnect(int uart)
{
	uart_set_tx_out(uart, 0);
}

void rdd_attached(void)
{
	/* Indicate case-closed debug mode (active low) */
	gpio_set_level(GPIO_CCD_MODE_L, 0);

	/* Select the CCD PHY */
	usb_select_phy(USB_SEL_PHY1);
}

void rdd_detached(void)
{
	/* Disconnect from AP and EC UART TX */
	uartn_tx_disconnect(UART_EC);
	uartn_tx_disconnect(UART_AP);

	/* Done with case-closed debug mode */
	gpio_set_level(GPIO_CCD_MODE_L, 1);

	/* Select the AP PHY */
	usb_select_phy(USB_SEL_PHY0);
}

static int command_uart(int argc, char **argv)
{
	if (argc > 1) {
		if (!strcasecmp("enable", argv[1])) {
			enable = 1;
			uartn_tx_connect(UART_EC, GPIO_EC_ON);
			uartn_tx_connect(UART_AP, GPIO_AP_ON);
		} else if (!strcasecmp("disable", argv[1])) {
			enable = 0;
			uartn_tx_disconnect(UART_EC);
			uartn_tx_disconnect(UART_AP);
		}
	}

	ccprintf("AP UART %s\nEC UART %s\n",
		uart_enabled(UART_AP) ? "enabled" : "disabled",
		uart_enabled(UART_EC) ? "enabled" : "disabled");
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(uart, command_uart,
	"[enable|disable]",
	"Get/set the UART TX connection state",
	NULL);
