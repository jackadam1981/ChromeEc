/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Meowth Fingerprint MCU configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* the UART console is on USART1 (PB14/PB15) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 1

/* Optional features */
#define CONFIG_DMA
#define CONFIG_STM_HWTIMER32
#define CONFIG_WATCHDOG_HELP
#define CONFIG_TASK_PROFILING
#undef CONFIG_HIBERNATE
#undef CONFIG_LID_SWITCH

#undef CONFIG_ADC
#undef CONFIG_I2C
/* Temporary */
#undef CONFIG_FLASH
#undef CONFIG_FLASH_PHYSICAL
#undef CONFIG_UART_RX_DMA
#undef CONFIG_UART_TX_DMA

/* SPI configuration for the fingerprint sensor */
#undef CONFIG_SPI_MASTER
#undef CONFIG_SPI_FP_PORT  /*  1 */  /* SPI3: second master config */

#undef CONFIG_CMD_SPI_XFER

#define CONFIG_UART_TX_DMA_CH STM32_DMAS_USART2_TX
#define CONFIG_UART_RX_DMA_CH STM32_DMAS_USART2_RX
#define CONFIG_UART_TX_REQ_CH STM32_REQ_USART2_TX
#define CONFIG_UART_RX_REQ_CH STM32_REQ_USART2_RX

#define CONFIG_CMD_FLASH

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_WATCHDOG 16

#define CONFIG_WP_ALWAYS

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
