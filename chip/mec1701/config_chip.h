/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CONFIG_CHIP_H
#define __CROS_EC_CONFIG_CHIP_H

/* CPU core BFD configuration */
#include "core/cortex-m/config_core.h"

/* Number of IRQ vectors on the NVIC */
#define CONFIG_IRQ_COUNT	157

/* Use a bigger console output buffer */
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE	2048

/* Interval between HOOK_TICK notifications */
#define HOOK_TICK_INTERVAL_MS	250
#define HOOK_TICK_INTERVAL	(HOOK_TICK_INTERVAL_MS * MSEC)

/*
 * MEC17xx family has 4 I2C master/slave
 * controllers and 11 I2C ports. Any
 * port may be mapped to any controller.
 * Enable multi-port controller feature.
 * Board level configuration determines
 * how many controllers/ports are used and
 * the mapping of port(s) to controller(s).
 * NOTE: Some MEC17xx reduced pin packages
 * may not implement all 11 I2C ports.
 */
#define CONFIG_I2C_MULTI_PORT_CONTROLLER


/****************************************************************************/
/* Memory mapping */

/*
 * MEC1701H has a total of 256KB SRAM.
 *   CODE at 0xE0000 - 0x117FFF, DATA at 0x118000 - 0x11FFFF
 *   MEC17xx can fetch code from data or data from code.
 */

/****************************************************************************/
/* Define our RAM layout. */

#define CONFIG_MEC_SRAM_BASE_START	0x000E0000
#define CONFIG_MEC_SRAM_BASE_END	0x00120000
#define CONFIG_MEC_SRAM_SIZE		(CONFIG_MEC_SRAM_BASE_END - \
					CONFIG_MEC_SRAM_BASE_START)

/* 32k RAM for RO / RW / loader */
#define CONFIG_RAM_SIZE			0x00008000
#define CONFIG_RAM_BASE			(CONFIG_MEC_SRAM_BASE_END - \
					CONFIG_RAM_SIZE)

/* System stack size */
#define CONFIG_STACK_SIZE		1024

/* non-standard task stack sizes */
#define IDLE_TASK_STACK_SIZE		512
#define LARGER_TASK_STACK_SIZE		640

#define CHARGER_TASK_STACK_SIZE		640
#define HOOKS_TASK_STACK_SIZE		640
#define CONSOLE_TASK_STACK_SIZE		640
#define HOST_CMD_TASK_STACK_SIZE	640

/*
 * TODO: Large stack consumption
 * https://code.google.com/p/chrome-os-partner/issues/detail?id=49245
 */
#define PD_TASK_STACK_SIZE		800

/* Default task stack size */
#define TASK_STACK_SIZE			512

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
#define CONFIG_PROGRAM_MEMORY_BASE	0x000E0000

#include "config_flash_layout.h"

/****************************************************************************/
/* Customize the build */
/* Optional features present on this chip */
#define CONFIG_ADC
#define CONFIG_DMA
#define CONFIG_LPC
#define CONFIG_SPI
#define CONFIG_SWITCH


/*
 * Define this to use MEC1701 ROM SPI read API
 * in little firmware module instead of SPI code
 * from this module
 */
#undef CONFIG_CHIP_LFW_USE_ROM_SPI


/* SHA256 code using Hash engine */
#define CONFIG_SHA256_HW

/*
 * Board level gpio.inc is using MEC17xx data sheet GPIO pin
 * numbers which are octal.
 * MEC17xx GPIO's are arranged in ports containing 32 pins
 * For example:
 * GPIO_015 = port 0, bit (8 + 7) = 15
 * GPIO_0123 = port (0123 / 040) = 2, bit (0123 % 040) = 023 = 19
 * OR port = 0123 >> 5, bit = 0123 & 037 = 023 = 19
 * As they are using the octal number as base 10 in gpio.inc
 * GPIO(PCH_SLP_S0_L,          PIN(0211), GPIO_INPUT | GPIO_PULL_DOWN)
 * Convert base 10 numbers to octal in gpio.inc by adding leading 0.
 *
 * MEC17xx has 6 banks/ports each containing 32 GPIO's.
 * Each bank/port is connected to a GIRQ.
 *
 *
 */

#define GPIO_BANK(index) ((index) >> 5)
#define GPIO_BANK_MASK(index) (1ul << ((index) & 0x1F))

#define GPIO_PIN(index) GPIO_BANK(index), GPIO_BANK_MASK(index)

#define GPIO_PIN_MASK(bank, mask) (bank), (mask)

#ifndef __ASSEMBLER__

/*
 * include TFDP macros
 */
#include "tfdp_chip.h"

#endif /* #ifndef __ASSEMBLER__ */

#endif  /* __CROS_EC_CONFIG_CHIP_H */
