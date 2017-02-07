/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Sweetberry configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Specify RAM size */
#undef  CONFIG_RAM_SIZE
#define CONFIG_RAM_SIZE 0x20000

/* Use external clock */
/* #define CONFIG_STM32_CLOCK_HSE_HZ 8000000 */

#define CONFIG_BOARD_POST_GPIO_INIT

/* Enable console recasting of GPIO type. */
#define CONFIG_CMD_GPIO_EXTENDED

/* The UART console is on USART2 (PA2/PA3) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 2
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096
/* Don't waste precious DMA channels on console. */
#undef CONFIG_UART_TX_DMA
#undef CONFIG_UART_RX_DMA

#define CONFIG_UART_TX_REQ_CH 4
#define CONFIG_UART_RX_REQ_CH 4

/* We're I2C slave only */
#define CONFIG_I2C
#define I2C_PORT_0	0

/* SPI master */
#define CONFIG_SPI_MASTER
/* Enable SPI master xfer command */
#define CONFIG_CMD_SPI_XFER

/* USB Configuration */
#define CONFIG_USB
#define CONFIG_USB_PID 0x500f
#define CONFIG_USB_CONSOLE

#define CONFIG_USB_DWC_FS
#define CONFIG_USB_SELF_POWERED

#define CONFIG_USB_SERIALNO
#define DEFAULT_SERIALNO "Uninitialized"

/* USB interface indexes (use define rather than enum to expand them) */
#define USB_IFACE_CONSOLE	0
#define USB_IFACE_COUNT		1

/* USB endpoint indexes (use define rather than enum to expand them) */
#define USB_EP_CONTROL		0
#define USB_EP_CONSOLE		1
#define USB_EP_COUNT		2

/* This is not actually a Chromium EC so disable some features. */
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH
#undef CONFIG_WATCHDOG

/* Optional features */
#define CONFIG_STM_HWTIMER32
#define CONFIG_DMA_HELP
#define CONFIG_FPU

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 5

#include "gpio_signal.h"

/* USB string indexes */
enum usb_strings {
	USB_STR_DESC = 0,
	USB_STR_VENDOR,
	USB_STR_PRODUCT,
	USB_STR_SERIALNO,
	USB_STR_VERSION,
	USB_STR_CONSOLE_NAME,
	USB_STR_COUNT
};

void button_event(enum gpio_signal signal);
void sensor_event(enum gpio_signal signal);
#endif /* !__ASSEMBLER__ */
#endif /* __CROS_EC_BOARD_H */
