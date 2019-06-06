/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MAX32660 board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Optional features */
#define CONFIG_SYSTEM_UNLOCKED /* Allow dangerous commands */
#if 0
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#endif

#define CONFIG_FPU

/* Modules we want to exclude */
#undef CONFIG_LID_SWITCH
#undef CONFIG_PECI
#undef CONFIG_SWITCH

#if 0
#define CONFIG_CMD_HOSTCMD
#endif

#if 0
#undef CONFIG_HOSTCMD_EVENTS
#define CONFIG_I2C
#endif

/* #define CONFIG_I2C_SLAVE */
/* #define CONFIG_HOSTCMD_I2C_SLAVE_ADDR (0x51 << 1) */

/* Write protect is active high */
#define CONFIG_WP_ACTIVE_HIGH

#undef CONFIG_WATCHDOG_PERIOD_MS
#define CONFIG_WATCHDOG_PERIOD_MS 2240

#ifndef __ASSEMBLER__

/* Slave I2C port configuration */
#if 0
#define I2C_PORT_SLAVE 0
#define I2C_PORT_EC I2C_PORT_SLAVE

/* I2C ports */
#define I2C_PORT_TEMPERATURE 1
#define I2C_PORT_OLED 1
#endif

/* Second UART port */
#define CONFIG_UART_HOST 1

#include "gpio_signal.h"

#if 0
void button_interrupt(enum gpio_signal signal);
#endif

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
