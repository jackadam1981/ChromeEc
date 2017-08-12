/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nucleo-F767ZI development board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* 84 MHz CPU/AHB/APB2 clock frequency (APB1 = 42 Mhz) */
#define CPU_CLOCK 84000000
#define CONFIG_FLASH_WRITE_SIZE STM32_FLASH_WRITE_SIZE_3300


/* the UART console is on USART2 (PD5/PD6) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 2

/* Optional features */
#undef CONFIG_LID_SWITCH
#undef CONFIG_HIBERNATE
#define CONFIG_STM_HWTIMER32
#define CONFIG_WATCHDOG_HELP
#define CONFIG_TASK_PROFILING

#undef CONFIG_ADC
#undef CONFIG_I2C

#undef CONFIG_UART_RX_DMA
#define CONFIG_UART_TX_DMA_CH STM32_DMAS_USART2_TX
#define CONFIG_UART_RX_DMA_CH STM32_DMAS_USART2_RX
#define CONFIG_UART_TX_REQ_CH STM32_REQ_USART2_TX
#define CONFIG_UART_RX_REQ_CH STM32_REQ_USART2_RX

#define CONFIG_CMD_FLASH

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_WATCHDOG 11

#define CONFIG_WP_ALWAYS

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
