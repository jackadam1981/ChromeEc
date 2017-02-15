/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
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
 * Number of I2C controllers. Controller 0 has 2 ports, so the chip has one
 * additional port.
 */
#define CONFIG_I2C_MULTI_PORT_CONTROLLER

#define I2C_CONTROLLER_COUNT	4
#define I2C_PORT_COUNT		5

/****************************************************************************/
/* Memory mapping */

/* TODO - Do the locations change with increase in code SRAM?
 * MEC1701H model has a total of 256KB SRAM.
 *   CODE at 0xE0000 - 0x117FFF, DATA at 0x118000 - 0x11FFFF
 *   MEC17xx can locate fetch code from data or data from code.
 * The memory region for 256KB RAM is actually 0x000E0000-0x0011FFFF.
 * RAM for RO/RW = 20k
 * CODE size of the Loader is 3k
 * As per the above configuartion the upper 20k
 * is used to store data.The rest is for code.
 * the lower 107K is flash[ 3k Loader and 104k RO/RW],
 * and the higher 20K is RAM shared by loader and RO/RW.
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
#if 0
#define CONFIG_ADC
#define CONFIG_PECI
#define CONFIG_MPU
#endif
#define CONFIG_DMA
#define CONFIG_LPC
#define CONFIG_SPI
#define CONFIG_SWITCH

/* TODO - Why use base 10 ? 
 * Board level gpio.inc is using MEC1322/MEC17xx data sheet GPIO pin 
 * numbers which are octal.
 * MEC1322/MEC17xx GPIO's are arranged in ports containing 32 pins
 * For example:
 * GPIO_015 = port 0, bit (8 + 7) = 15
 * GPIO_0123 = port (0123 / 040) = 2, bit (0123 % 040) = 023 = 19
 * OR port = 0123 >> 5, bit = 0123 & 037 = 023 = 19
 * As they are using the octal number as base 10 in gpio.inc
 * GPIO(PCH_SLP_S0_L,          PIN(211), GPIO_INPUT | GPIO_PULL_DOWN)
 * We must convert base 10 211 to octal in order to use shifting.
 */
/* GPIO_PIN(15) -> 5, 32 */
/* #define GPIO_PIN(index) (index / 10), (1 << (index % 10)) */
/* GPIO_DEC2OCT(decnum) only works if index is base 10 number. */
/* We have added a leading zero to all pin numbers in gpio.inc
 * #define GPIO_DEC2OCT(decnum) 0 ## decnum
 * #define GPIO_PIN(index) (GPIO_DEC2OCT(index) >> 5), (GPIO_DEC2OCT(n) & 0x1F)
 *
 * From comments in ec/include/gpio.wrap the PIN(x) macro expands to GPIO_PIN(x)
 * which is defined here.
 * We expect x parameter of GPIO_PIN(x) to be the octal number from the MEC17xx
 * data sheet. MEC17xx data sheet GPIO numbers are octal.
 * GPIO_PIN(x) produces two comma separated values which are the GPIO bank number
 * and bit position in the bank. MEC17xx GPIO banks/ports contain 32 GPIO's.
 * Each bank/port is connected to a GIRQ.
 * MEC17xx has 6 banks/ports.
 * The common GPIO structure from ec/include/gpio.h is
 * struct gpio_info {
 *         Signal name
 *      const char *name;
 *
 *       Port base address
 *     uint32_t port;
 *
 *       Bitmask on that port (1 << N; 0 = signal not implemented)
 *     uint32_t mask;
 *
 *      Flags (GPIO_*; see above)
 *     uint32_t flags;
 * };
 *
*/
#define GPIO_PIN(index) ((index) >> 5), (1ul << ((index) & 0x1F))

#define GPIO_PIN_MASK(bank, mask) (bank), (mask)

#endif  /* __CROS_EC_CONFIG_CHIP_H */
