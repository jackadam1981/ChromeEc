/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Meowth Fingerprint MCU configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* the UART console is on USART1 */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 1
#define CONFIG_UART_TX_DMA
#define CONFIG_UART_TX_DMA_PH DMAMUX1_REQ_USART1_TX

/* Optional features */
#undef CONFIG_ADC
#define CONFIG_DMA
#define CONFIG_FPU
#undef CONFIG_HIBERNATE
#undef CONFIG_I2C
#undef CONFIG_LID_SWITCH
#define CONFIG_MKBP_EVENT
#define CONFIG_PRINTF_LEGACY_LI_FORMAT
#define CONFIG_SHA256
#define CONFIG_SPI
#define CONFIG_STM_HWTIMER32
#undef CONFIG_TASK_PROFILING
#define CONFIG_WATCHDOG_HELP
#define CONFIG_WP_ALWAYS

/* Temporary */
#undef CONFIG_FLASH
#undef CONFIG_FLASH_PHYSICAL
#define CONFIG_SYSTEM_UNLOCKED

#define CONFIG_CMD_FLASH

/* SPI configuration for the fingerprint sensor */
#define CONFIG_SPI_MASTER
#define CONFIG_SPI_FP_PORT  2 /* SPI4: third master config */
#ifdef SECTION_IS_RW
#define CONFIG_FP_SENSOR_FPC1145
#define CONFIG_CMD_FPSENSOR_DEBUG
/* shared_mem_init is done too late for RW verification, skip it in RO */
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

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_WATCHDOG 16

#include <stdint.h>
#include "gpio_signal.h"

void fps_event(enum gpio_signal signal);

/* STM32H7 flash support is not ready, let's pretend nothing is protected */
static inline uint32_t flash_get_protect(void)
{
	return 0;
}
static inline int flash_set_protect(uint32_t mask, uint32_t flags)
{
	return 0;
}

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
