/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CONFIG_CHIP_H
#define __CROS_EC_CONFIG_CHIP_H
/* Number of IRQ vectors on the NVIC */
#define CONFIG_IRQ_COUNT 93

/* Use a bigger console output buffer */
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 2048

/* Interval between HOOK_TICK notifications */
#define HOOK_TICK_INTERVAL (250 * MSEC)

/* Maximum number of deferrable functions */
#define DEFERRABLE_MAX_COUNT 8

/* Number of I2C ports */
#define I2C_PORT_COUNT 4

/****************************************************************************/
/* Memory mapping */

/* MEC1322 uses external flash */
#define CONFIG_EXT_FLASH

#define CONFIG_RAM_BASE             0x00100000
#define CONFIG_RAM_SIZE             0x00020000

#define CONFIG_RAM_CODE_RO_OFF      0x0
#define CONFIG_RAM_CODE_RO_SIZE     0x0000c000
#define CONFIG_RAM_CODE_RW_OFF      0xc000
#define CONFIG_RAM_CODE_RW_SIZE     0x0000c000
#define CONFIG_RAM_CODE_SIZE        0x18000

#define CONFIG_RAM_DATA_OFF         0x18000
#define CONFIG_RAM_DATA_SIZE        0x8000

/* System stack size */
#define CONFIG_STACK_SIZE           4096

/* non-standard task stack sizes */
#define IDLE_TASK_STACK_SIZE        512
#define LARGER_TASK_STACK_SIZE      640

/* Default task stack size */
#define TASK_STACK_SIZE             512

#define CONFIG_FLASH_BASE           0x00100000

/* Ideal flash write size fills the 32-entry flash write buffer */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE (32 * 4)

/* This is the physical size of the flash on the chip. We'll reserve one bank
 * in order to emulate per-bank write-protection UNTIL REBOOT. The hardware
 * doesn't support a write-protect pin, and if we make the write-protection
 * permanent, it can't be undone easily enough to support RMA. */
#define CONFIG_FLASH_PHYSICAL_SIZE  0x00018000

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

/* TODO: why 2 sets of configs with the same numbers? */
#define CONFIG_FW_WP_RO_OFF		CONFIG_FW_RO_OFF
#define CONFIG_FW_WP_RO_SIZE		CONFIG_FW_RO_SIZE

/****************************************************************************/
/* Lock the boot configuration to prevent brickage. */

/*
 * No GPIO trigger for ROM bootloader.
 * Keep JTAG debugging enabled.
 * Use 0xA442 flash write key.
 * Lock it this way.
 */
#define CONFIG_BOOTCFG_VALUE 0x7ffffffe

/****************************************************************************/
/* Customize the build */
/* Optional features present on this chip */
#if 0
#define CONFIG_ADC
#define CONFIG_I2C
#define CONFIG_LPC
#define CONFIG_PECI
#define CONFIG_SWITCH
#define CONFIG_MPU
#endif
#define CONFIG_FPU

#undef CONFIG_FLASH

#endif  /* __CROS_EC_CONFIG_CHIP_H */
