/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Honeybuns board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Baseboard features */
#include "baseboard.h"

/* Optional features */
#define CONFIG_SYSTEM_UNLOCKED /* Allow dangerous commands while in dev. */

#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096

/*
 * Enable the blink example that exercises the LEDs.
 */


/* Setup UART console */

#define STM32G431_EVAL_USE_LPUART_CONSOLE
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_TX_DMA
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 2048

#ifdef STM32G431_EVAL_USE_LPUART_CONSOLE
#define CONFIG_UART_CONSOLE 9
#define CONFIG_UART_TX_DMA_CH STM32_DMAC_LPUART_TX
#define CONFIG_UART_TX_DMA_PH 4
#else
#define CONFIG_UART_CONSOLE 1
#define CONFIG_UART_TX_DMA_CH STM32_DMAC_USART1_TX
#define CONFIG_UART_TX_DMA_PH 2
#endif


/* Keyboard features */

/* Sensors */

/* USB Type C and USB PD defines */

/* USB Type A Features */

/* BC 1.2 */

/* Volume Button feature */

/* Fan features */

/*
 * Macros for GPIO signals used in common code that don't match the
 * schematic names. Signal names in gpio.inc match the schematic and are
 * then redefined here to so it's more clear which signal is being used for
 * which purpose.
 */
#define GPIO_ENTERING_RW	GPIO_EC_ENTERING_RW
#define GPIO_WP_L		GPIO_EC_WP_L



#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"


#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
