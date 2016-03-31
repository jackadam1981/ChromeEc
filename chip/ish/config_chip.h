/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CONFIG_CHIP_H
#define __CROS_EC_CONFIG_CHIP_H

/* CPU core BFD configuration */
#include "core/minute-ia/config_core.h"

/* Number of IRQ vectors on the ISH */
#define CONFIG_IRQ_COUNT	15

/* Use a bigger console output buffer */
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE	2048

/* Interval between HOOK_TICK notifications */
#define HOOK_TICK_INTERVAL_MS	250
#define HOOK_TICK_INTERVAL	(HOOK_TICK_INTERVAL_MS * MSEC)

/* Maximum number of deferrable functions */
#define DEFERRABLE_MAX_COUNT	8

/*
 * Number of I2C controllers. Controller 0 has 2 ports, so the chip has one
 * additional port.
 */
#define CONFIG_I2C_MULTI_PORT_CONTROLLER

#define I2C_CONTROLLER_COUNT	4
#define I2C_PORT_COUNT		3

/****************************************************************************/
/* Memory mapping */

/****************************************************************************/
/* Define our RAM layout. */

#define CONFIG_ISH_SRAM_BASE_START	0xFF000000
#define CONFIG_ISH_SRAM_BASE_END	0xFF09FFFF
#define CONFIG_ISH_SRAM_SIZE		(CONFIG_ISH_SRAM_BASE_END - \
					CONFIG_ISH_SRAM_BASE_START + 1)

/* 20k RAM for RO / RW / loader */
#define CONFIG_RAM_SIZE			0x00005000
#define CONFIG_RAM_BASE			(CONFIG_ISH_SRAM_BASE_END - \
					CONFIG_RAM_SIZE)

/* System stack size */
#define CONFIG_STACK_SIZE		1024

/* non-standard task stack sizes */
#define IDLE_TASK_STACK_SIZE		1024
#define LARGER_TASK_STACK_SIZE		1024

#define CHARGER_TASK_STACK_SIZE		640
#define HOOKS_TASK_STACK_SIZE		2048
#define CONSOLE_TASK_STACK_SIZE		640
#define HOST_CMD_TASK_STACK_SIZE	640
#define IPC_TASK_STACK_SIZE		1024
#define MOTION_SENSE_TASK_STACK_SIZE	2048
/* Default task stack size */
#define TASK_STACK_SIZE			640

/****************************************************************************/
/* Define our flash layout. */

/* Protect bank size 4K bytes */
#define CONFIG_FLASH_BANK_SIZE		0x00001000
/* Sector erase size 4K bytes */
#define CONFIG_FLASH_ERASE_SIZE		0x00001000
/* Minimum write size */
#define CONFIG_FLASH_WRITE_SIZE		0x00000004

/* One page size for write */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE	256

/* Program memory base address */
#define CONFIG_PROGRAM_MEMORY_BASE	0x00100000

#include "config_flash_layout.h"

/****************************************************************************/
/* Customize the build */
/* Optional features present on this chip */
#define CONFIG_LPC
#define CONFIG_SPI
#define CONFIG_SWITCH

#define GPIO_PIN(index) ((index / 10), (1 << (index % 10)))
#define GPIO_PIN_MASK(pin, mask) ((pin), (mask))

#endif  /* __CROS_EC_CONFIG_CHIP_H */
