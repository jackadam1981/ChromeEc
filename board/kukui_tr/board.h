/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Kukui eMMC emulator board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* TODO: Remove CONFIG_SYSTEM_UNLOCKED prior to building MP FW. */
#define CONFIG_SYSTEM_UNLOCKED
/* TODO(b:63378217): Define FLASH_PSTATE_LOCKED prior to building MP FW. */
#undef CONFIG_FLASH_PSTATE_LOCKED

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* The UART console is on USART1 (PA9/PA10) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 1

/* Optional features */
#define CONFIG_LTO
#define CONFIG_FORCE_CONSOLE_RESUME
#define CONFIG_STM_HWTIMER32

#define CONFIG_LZ4

/* Optional features */
#define CONFIG_BOARD_PRE_INIT
#define CONFIG_WATCHDOG_HELP

/* No lid switch */
#undef CONFIG_LID_SWITCH

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_WATCHDOG 16

#include "gpio_signal.h"

void emmc_cmd_interrupt(enum gpio_signal signal);

#endif /* !__ASSEMBLER__ */
#endif /* __CROS_EC_BOARD_H */
