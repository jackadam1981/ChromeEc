/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hammer configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* TODO: Remove CONFIG_SYSTEM_UNLOCKED prior to building MP FW. */
#define CONFIG_SYSTEM_UNLOCKED

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* Enable USART1 and USB streams */
#define CONFIG_STREAM_USART
#define CONFIG_STREAM_USB
#define CONFIG_CMD_USART_INFO

/* The UART console is on USART1 (PA9/PA10) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 1
#undef CONFIG_UART_TX_DMA
#undef CONFIG_UART_RX_DMA

/* Optional features */
#define CONFIG_STM_HWTIMER32
#define CONFIG_HW_CRC

/* USB Configuration */
#define CONFIG_USB
/* TODO(drinkcat): Replace this by proper ID */
#define CONFIG_USB_PID 0xCAFE
#define CONFIG_USB_CONSOLE
#define CONFIG_USB_UPDATE
#define CONFIG_USB_HID

#undef CONFIG_USB_MAXPOWER_MA
#define CONFIG_USB_MAXPOWER_MA 100

#define CONFIG_USB_SERIALNO
/* TODO(drinkcat): Replace this by proper serial number */
#define DEFAULT_SERIALNO "Uninitialized"

/* USB interface indexes (use define rather than enum to expand them) */
#define USB_IFACE_UPDATE	0
#define USB_IFACE_CONSOLE	1
#define USB_IFACE_HID		2
#define USB_IFACE_COUNT		3

/* USB endpoint indexes (use define rather than enum to expand them) */
#define USB_EP_CONTROL		0
#define USB_EP_UPDATE		1
#define USB_EP_CONSOLE		2
#define USB_EP_HID		3
#define USB_EP_COUNT		4

/* This is not actually an EC so disable some features. */
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH

/* Enable I2C */
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define I2C_PORT_MASTER 0

/* Sign and switch to RW partition on boot. */
#define CONFIG_RWSIG
#define CONFIG_RSA
#define CONFIG_SHA256
#define CONFIG_RSA_KEY_SIZE 2048

/* Enable elan trackpad driver */
#define CONFIG_TOUCHPAD_ELAN
#define CONFIG_TOUCHPAD_I2C_PORT 0
#define CONFIG_TOUCHPAD_I2C_ADDR (0x15 << 1)

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2

#include "gpio_signal.h"

/* USB string indexes */
enum usb_strings {
	USB_STR_DESC = 0,
	USB_STR_VENDOR,
	USB_STR_PRODUCT,
	USB_STR_SERIALNO,
	USB_STR_VERSION,
	USB_STR_CONSOLE_NAME,
	USB_STR_UPDATE_NAME,
	USB_STR_COUNT
};

#endif /* !__ASSEMBLER__ */
#endif /* __CROS_EC_BOARD_H */
