/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CONFIG_CHIP_H
#define __CROS_EC_CONFIG_CHIP_H

/* CPU core BFD configuration */
#include "core/cortex-m/config_core.h"

/* Number of IRQ vectors on the NVIC */
#define CONFIG_IRQ_COUNT 93

/* Use a bigger console output buffer */
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 2048

/* Interval between HOOK_TICK notifications */
#define HOOK_TICK_INTERVAL_MS 250
#define HOOK_TICK_INTERVAL    (HOOK_TICK_INTERVAL_MS * MSEC)

/* Maximum number of deferrable functions */
#define DEFERRABLE_MAX_COUNT 8

/* Number of I2C ports */
#define I2C_PORT_COUNT 4

/****************************************************************************/
/* Memory mapping */

/*
 * The memory region for RAM is actually 0x00100000-0x00120000. The lower 96K
 * stores code and the higher 32K stores data. To reflect this, let's say
 * the lower 96K is flash, and the higher 32K is RAM.
 */

/*
 * The memory region for RAM is actually 0x00100000-0x00120000.
 * RAM for Loader = 4k
 * RAM for RO/RW = 20k
 * CODE size of the Loader is 16k for debug version and 12k for Release version
 * As per the above configuartion  the lower 104k is
 * stores code and the higher 24K stores data. To reflect this, let's say
 * the lower 104K is flash[ 16k Loader and 88k RO/RW],
 * and the higher 24K is RAM shared by loader and RO/RW.
 */

#define NEW_CONFIG
#define DEBUG_VER

#ifdef NEW_CONFIG
/****************************************************************************/
/* Define our RAM layout. */

#define CONFIG_MEC_SRAM_BASE_START		  0x00100000
#define CONFIG_MEC_SRAM_BASE_END		  0x00120000
#define CONFIG_MEC_SRAM_SIZE			(CONFIG_MEC_SRAM_BASE_END - \
				CONFIG_MEC_SRAM_BASE_START)

/* 4k RAM for Loader for debug and release version */
#define CONFIG_RAM_SIZE_LOADER             0x0001000
#define CONFIG_RAM_BASE_LOADER             (CONFIG_MEC_SRAM_BASE_END - \
				CONFIG_RAM_SIZE_LOADER)

/* estimated 20k for RO/RW data allocation */
#define CONFIG_RAM_SIZE_RORW             0x00005000
#define CONFIG_RAM_BASE_RORW            (CONFIG_RAM_BASE_LOADER - \
				CONFIG_RAM_SIZE_RORW)


#define CONFIG_RAM_BASE              CONFIG_RAM_BASE_RORW
#define CONFIG_RAM_SIZE             (CONFIG_RAM_SIZE_LOADER + \
				CONFIG_RAM_SIZE_RORW)

#define CONFIG_RAM_SIZE_TOTAL	(CONFIG_RAM_SIZE_RORW + \
				CONFIG_RAM_SIZE_LOADER)

/* System stack size */
#define CONFIG_STACK_SIZE           4096

/* non-standard task stack sizes */
#define IDLE_TASK_STACK_SIZE        512
#define LARGER_TASK_STACK_SIZE      640

/* Default task stack size */
#define TASK_STACK_SIZE             512





/****************************************************************************/
/* Define our flash layout. */
#ifdef DEBUG_VER
#define CONFIG_LOADE_IMAGE_SIZE   0x00004000
#else
#define CONFIG_LOADE_IMAGE_SIZE   0x00003000
#endif

#define SHARED_RAM_LOADER_RORW		(CONFIG_MEC_SRAM_BASE_START + \
				(CONFIG_LOADE_IMAGE_SIZE - 4))

#define CONFIG_FLASH_PHYSICAL_SIZE  0x00040000
#define CONFIG_FLASH_BASE           (CONFIG_MEC_SRAM_BASE_START)
#define CONFIG_FLASH_SIZE		CONFIG_FLASH_PHYSICAL_SIZE

#define CONFIG_FW_LOADER_OFF        0
#define CONFIG_FW_LOADER_SIZE       CONFIG_LOADE_IMAGE_SIZE

#define CONFIG_FW_IMAGE_SIZE        (88 * 1024)

/* RO/RW firmware must after Loader code */
#define CONFIG_FW_RO_OFF            CONFIG_FW_LOADER_SIZE
#define CONFIG_FW_RO_SIZE           CONFIG_FW_IMAGE_SIZE


#define CONFIG_FW_RW_OFF            CONFIG_FW_LOADER_SIZE
#define CONFIG_FW_RW_SIZE           CONFIG_FW_RO_SIZE


/* TODO(crosbug.com/p/23796): why 2 sets of configs with the same numbers? */
#define CONFIG_FW_WP_RO_OFF         CONFIG_FW_LOADER_OFF
#define CONFIG_FW_WP_RO_SIZE        (CONFIG_FW_LOADER_SIZE + \
				CONFIG_FW_RO_SIZE)

#define CONFIG_FLASH_BANK_SIZE      0x00000800  /* protect bank size */
#define CONFIG_FLASH_ERASE_SIZE     0x00001000  /* erase bank size */
#define CONFIG_FLASH_WRITE_SIZE     0x00000004  /* minimum write size */

/* Ideal flash write size fills the 32-entry flash write buffer */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE (32 * 4)



/****************************************************************************/

#else
#define CONFIG_RAM_BASE             0x00118000
#define CONFIG_RAM_SIZE             0x00008000

/* System stack size */
#define CONFIG_STACK_SIZE           4096

/* non-standard task stack sizes */
#define IDLE_TASK_STACK_SIZE        512
#define LARGER_TASK_STACK_SIZE      640

/* Default task stack size */
#define TASK_STACK_SIZE             512

#define CONFIG_FLASH_BASE           0x00100000

#define CONFIG_FLASH_PHYSICAL_SIZE  0x00030000

/* Size of one firmware image in RAM */

/****************************************************************************/
/* Define our flash layout. */

/* Size of one firmware image in flash */
#ifndef CONFIG_FW_IMAGE_SIZE
#define CONFIG_FW_IMAGE_SIZE		(CONFIG_FLASH_PHYSICAL_SIZE / 2)
#endif

/* RO firmware must start at beginning of flash */
#define CONFIG_FW_RO_OFF		0

#define CONFIG_FW_RO_SIZE		CONFIG_FW_IMAGE_SIZE
#define CONFIG_FLASH_SIZE		CONFIG_FLASH_PHYSICAL_SIZE

/* Either way, RW firmware is one firmware image offset from the start */
#define CONFIG_FW_RW_OFF		CONFIG_FW_IMAGE_SIZE
#define CONFIG_FW_RW_SIZE		CONFIG_FW_IMAGE_SIZE

/* TODO(crosbug.com/p/23796): why 2 sets of configs with the same numbers? */
#define CONFIG_FW_WP_RO_OFF		CONFIG_FW_RO_OFF
#define CONFIG_FW_WP_RO_SIZE		CONFIG_FW_RO_SIZE

#define CONFIG_FLASH_BANK_SIZE      0x00000800  /* protect bank size */
#define CONFIG_FLASH_ERASE_SIZE     0x00000400  /* erase bank size */
#define CONFIG_FLASH_WRITE_SIZE     0x00000004  /* minimum write size */

/* Ideal flash write size fills the 32-entry flash write buffer */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE (32 * 4)


/****************************************************************************/

#endif

#define MEC1322_LOADER_IMAGE_FLASHADDR (0x170000UL)
#define MEC1322_RO_IMAGE_FLASHADDR	(MEC1322_LOADER_IMAGE_FLASHADDR + \
				CONFIG_LOADE_IMAGE_SIZE)

#define MEC1322_RW_IMAGE_FLASHADDR (0x190000UL)

/*
 * The EC uses the one bank of flash to emulate a SPI-like write protect
 * register with persistent state.
 */
#define CONFIG_FW_PSTATE_SIZE		CONFIG_FLASH_BANK_SIZE

#define CONFIG_FLASH_BASE_EXT_SPI MEC1322_LOADER_IMAGE_FLASHADDR
#define CONFOG_RO_WP_SPI_OFF			0
#define CONFIG_RO_SPI_OFF			CONFIG_FW_LOADER_SIZE
#define CONFIG_RW_SPI_OFF			(MEC1322_RW_IMAGE_FLASHADDR - \
				MEC1322_LOADER_IMAGE_FLASHADDR)
#define CONFIG_FW_PSTATE_OFF		(CONFIG_RW_SPI_OFF << 2)
/* Customize the build */
/* Optional features present on this chip */
#if 0
#define CONFIG_ADC
#define CONFIG_PECI
#define CONFIG_MPU
#endif
#define CONFIG_I2C
#define CONFIG_LPC
#define CONFIG_FPU
#define CONFIG_SPI
#define CONFIG_DMA
#undef CONFIG_SWITCH
#define CONFIG_FLASH_EXT_SPI
#undef CONFIG_FLASH

#endif  /* __CROS_EC_CONFIG_CHIP_H */
