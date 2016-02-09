/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Rei board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Undefining defaults for bringup. */
#define CONFIG_BRINGUP
#undef CONFIG_FLASH
#undef CONFIG_FMAP
#undef CONFIG_FLASH_PSTATE
#undef CONFIG_FLASH_PSTATE_BANK
#undef CONFIG_HIBERNATE
#undef CONFIG_LID_SWITCH
#undef CONFIG_WATCHDOG_PERIOD_MS
#define CONFIG_WATCHDOG_PERIOD_MS 60000
#define CONFIG_I2C

/* Not sure how this number is determined. */
#undef DEFERRABLE_MAX_COUNT
#define DEFERRABLE_MAX_COUNT 16

#define CPU_CLOCK 5292
#define CONFIG_WATCHDOG

/* The baud rate is 9600 */
#undef CONFIG_UART_BAUD_RATE
#define CONFIG_UART_BAUD_RATE 9600

#define CONFIG_CMD_FORCETIME


#ifndef __ASSEMBLER__

#include "gpio_signal.h"

#endif /* defined(__ASSEMBLER__) */

#endif /* __CROS_EC_BOARD_H */
