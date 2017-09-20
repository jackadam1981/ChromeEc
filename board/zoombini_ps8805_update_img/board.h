/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __BOARD_H
#define __BOARD_H

/* NPCX7 config */
#define NPCX_UART_MODULE2 1  /* GPIO64/65 are used as UART pins. */
#define NPCX_TACH_SEL2    0  /* No tach. */
#define NPCX7_PWM1_SEL    0  /* GPIO C2 is not used as PWM1. */

/* Internal SPI flash on NPCX7 */
#define CONFIG_FLASH_SIZE (512 * 1024) /* It's really 1MB. */
#define CONFIG_SPI_FLASH_REGS
#define CONFIG_SPI_FLASH_W25Q80 /* Internal SPI flash type. */

#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define I2C_PORT_TCPC0   NPCX_I2C_PORT1_0
#define I2C_PORT_TCPC1   NPCX_I2C_PORT2_0
#define I2C_PORT_TCPC2   NPCX_I2C_PORT5_0

/* #define CONFIG_CONSOLE_RESTRICTED_INPUT */

#undef CONFIG_ADC
#undef CONFIG_HIBERNATE
#undef CONFIG_FLASH
#undef CONFIG_WATCHDOG
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH
#undef CONFIG_PECI
#undef CONFIG_SWITCH
#undef CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */
#endif /* __BOARD_H */
