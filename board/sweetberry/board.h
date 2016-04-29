/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Sweetberry configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* The UART console is on USART1 (PA9/PA10) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 1

/* Optional features */
#define CONFIG_STM_HWTIMER32
#define CONFIG_HW_CRC

#if 0
/* USB Configuration */
#define CONFIG_USB
#define CONFIG_USB_PID 0x501c
#define CONFIG_USB_CONSOLE

/* USB interface indexes (use define rather than enum to expand them) */
#define USB_IFACE_CONSOLE 0
#define USB_IFACE_COUNT   1

/* USB endpoint indexes (use define rather than enum to expand them) */
#define USB_EP_CONTROL 0
#define USB_EP_CONSOLE 1
#define USB_EP_COUNT   2
#endif 

/* This is not actually an EC so disable some features. */
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH

/* I2C */
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define I2C_PORT_MASTER 0

/* Flash */
#undef CONFIG_FLASH_PROTECT_NEXT_BOOT

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

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
	USB_STR_COUNT
};

#endif /* !__ASSEMBLER__ */
#endif /* __CROS_EC_BOARD_H */
