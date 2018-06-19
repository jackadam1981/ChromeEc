/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Atlas sensor hub configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/*
 * Allow dangerous commands.
 * TODO: Remove this config before production.
 */
#define CONFIG_SYSTEM_UNLOCKED

/*
 * By default, enable all console messages. Disable host command debug.
 * TODO: Remove unused console messages.
 */
#define CC_DEFAULT CC_ALL
#undef CONFIG_HOSTCMD_DEBUG_MODE
#define CONFIG_HOSTCMD_DEBUG_MODE HCDEBUG_OFF

/* ISH specific*/
#define CONFIG_ISH_30
#undef  CONFIG_DEBUG_ASSERT
#define CONFIG_CLOCK_CRYSTAL
#define CONFIG_ISH_UART_0

/* EC */
#define CONFIG_FLASH_SIZE 0x80000
#define CONFIG_FPU
#define CONFIG_I2C
#define CONFIG_I2C_MASTER

/* ISH runs on RAM, disable RO */
#undef CONFIG_FW_INCLUDE_RO
#undef CONFIG_RW_MEM_OFF
#undef CONFIG_RO_SIZE
#undef CONFIG_RW_SIZE
#define CONFIG_RW_MEM_OFF 0
#define CONFIG_RO_SIZE 0
#define CONFIG_RW_SIZE CONFIG_FLASH_SIZE

/* I2C ports */
#define I2C_PORT_TP ISH_I2C0

/* Features and modules */
#undef CONFIG_CMD_HASH
#undef CONFIG_CMD_I2C_SCAN
#undef CONFIG_CMD_I2C_XFER
#undef CONFIG_CMD_KEYBOARD
#undef CONFIG_CMD_POWER_AP
#undef CONFIG_CMD_POWERINDEBUG
#undef CONFIG_CMD_SHMEM
#undef CONFIG_CMD_TIMERINFO
#undef CONFIG_EXTPOWER
#undef CONFIG_KEYBOARD_KSO_BASE
#undef CONFIG_FLASH
#undef CONFIG_FMAP
#undef CONFIG_LID_SWITCH
#undef CONFIG_SWITCH
#undef CONFIG_WATCHDOG
#undef CONFIG_CMD_ACCELS
#undef CONFIG_CMD_HASH
#undef CONFIG_CMD_TEMP_SENSOR
#undef CONFIG_CMD_TIMERINFO
#undef CONFIG_ADC
#undef CONFIG_SHA256

#ifndef __ASSEMBLER__

#include "gpio_signal.h"


#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
