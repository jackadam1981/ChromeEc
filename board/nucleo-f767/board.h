/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nucleo-F767ZI development board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* 180 MHz CPU/AHB clock frequency, 45 Mhz APB2, 45 Mhz APB1 */
#define CPU_CLOCK 180000000
#define CONFIG_FLASH_WRITE_SIZE STM32_FLASH_WRITE_SIZE_3300


/* the UART console is on USART2 (PD5/PD6) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 2

/* Optional features */
#undef CONFIG_LID_SWITCH
#undef CONFIG_HIBERNATE
#define CONFIG_FPU
#define CONFIG_MKBP_EVENT
#define CONFIG_PRINTF_LEGACY_LI_FORMAT
#define CONFIG_SHA256
#define CONFIG_SHA256_UNROLLED
#define CONFIG_STM_HWTIMER32
#define CONFIG_TASK_PROFILING
#define CONFIG_WATCHDOG_HELP

#undef CONFIG_ADC
#undef CONFIG_I2C

/* SPI configuration for the fingerprint sensor */
#define CONFIG_SPI_MASTER
#define CONFIG_SPI_FP_PORT    1  /* SPI3: second master config */
#ifdef SECTION_IS_RW
#undef  CONFIG_FP_SENSOR_EFSA614
#undef CONFIG_FP_SENSOR_FPC1025
#define CONFIG_FP_SENSOR_FPC1145
#define CONFIG_CMD_FPSENSOR_DEBUG
/*
 * Use the malloc code only in the RW section (for the private library),
 * we cannot enable it in RO since it is not compatible with the RW verification
 * (shared_mem_init done too late).
 */
#define CONFIG_MALLOC

/* we are doing slow compute */
#undef CONFIG_WATCHDOG_PERIOD_MS
#define CONFIG_WATCHDOG_PERIOD_MS 10000

#else /* SECTION_IS_RO */
/* RO verifies the RW partition signature */
#define CONFIG_RSA
#define CONFIG_RSA_KEY_SIZE 3072
#define CONFIG_RSA_EXPONENT_3
#define CONFIG_RWSIG
#endif
#define CONFIG_RWSIG_TYPE_RWSIG

#define CONFIG_CMD_SPI_XFER

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

void fps_event(enum gpio_signal signal);

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
