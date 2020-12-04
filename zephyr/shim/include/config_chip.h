/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CONFIG_CHIP_H
#define __CROS_EC_CONFIG_CHIP_H

#include <devicetree.h>

/*
 * This file translates Kconfig options to platform/ec options.
 *
 * Options which are from Zephyr platform/ec module (Kconfig) start
 * with CONFIG_PLATFORM_EC_, and can be found in the Kconfig file.
 *
 * Options which are for the platform/ec configuration can be found in
 * common/config.h.
 */

#define CONFIG_ZEPHYR
#define CHROMIUM_EC

/* Chipset and power configuration */
#ifdef CONFIG_AP_X86_INTEL_TGL
#define CONFIG_CHIPSET_TIGERLAKE
#endif

/* eSPI signals */
#ifdef CONFIG_PLATFORM_EC_ESPI_VW_SLP_S3
#define CONFIG_HOSTCMD_ESPI_VW_SLP_S3
#endif

#ifdef CONFIG_PLATFORM_EC_ESPI_VW_SLP_S4
#define CONFIG_HOSTCMD_ESPI_VW_SLP_S4
#endif

/* Flash settings */
#undef CONFIG_EXTERNAL_STORAGE
#undef CONFIG_MAPPED_STORAGE
#undef CONFIG_FLASH_PSTATE
#undef CONFIG_FLASH_SIZE
#ifdef CONFIG_PLATFORM_EC_FLASH
#undef CONFIG_CMD_FLASHINFO
#undef CONFIG_CMD_FLASH
#define CONFIG_FLASH
#define CONFIG_FLASH_SIZE	0x80000
	/* TODO: use DT_PROP(DT_INST(inst, DT_DRV_COMPAT), size) ? */
#define CONFIG_MAPPED_STORAGE_BASE 0x64000000
#define CONFIG_FLASH_WRITE_SIZE		0x1  /* minimum write size */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE	256   /* one page size for write */
#define CONFIG_FLASH_ERASE_SIZE	0x1000
#define CONFIG_FLASH_BANK_SIZE		CONFIG_FLASH_ERASE_SIZE
#define CONFIG_EC_PROTECTED_STORAGE_OFF  0
#define CONFIG_EC_PROTECTED_STORAGE_SIZE 0x40000
#define CONFIG_EC_WRITABLE_STORAGE_OFF   0x40000
#define CONFIG_EC_WRITABLE_STORAGE_SIZE  0x40000
#define CONFIG_WP_STORAGE_OFF	CONFIG_EC_PROTECTED_STORAGE_OFF
#define CONFIG_WP_STORAGE_SIZE	CONFIG_EC_PROTECTED_STORAGE_SIZE
#define CONFIG_RO_SIZE		CONFIG_CROS_EC_RO_SIZE
#define CONFIG_RW_SIZE		CONFIG_CROS_EC_RW_SIZE

#define CONFIG_RO_HDR_SIZE	0x40
/* RO image resides at start of protected region, right after header */
#define CONFIG_RO_STORAGE_OFF	CONFIG_RO_HDR_SIZE

#ifdef PLATFORM_EC_EXTERNAL_STORAGE
#define CONFIG_EXTERNAL_STORAGE
#endif

#ifdef CONFIG_PLATFORM_EC_MAPPED_STORAGE
#define CONFIG_MAPPED_STORAGE
#endif

#ifdef CONFIG_PLATFORM_EC_CONSOLE_CMD_FLASH
#define CONFIG_CMD_FLASHINFO
#define CONFIG_CMD_FLASH
#endif

#endif /* CONFIG_PLATFORM_EC_FLASH */

#ifdef CONFIG_PLATFORM_EC_I2C
/* Also see shim/include/i2c/i2c.h which defines the ports enum */
#define CONFIG_I2C
#endif

#undef CONFIG_KEYBOARD_PROTOCOL_8042
#ifdef CONFIG_PLATFORM_EC_KEYBOARD_PROTOCOL_8042
#define CONFIG_KEYBOARD_PROTOCOL_8042
#endif /* CONFIG_PLATFORM_EC_KEYBOARD_PROTOCOL_8042 */

#undef CONFIG_CMD_KEYBOARD
#ifdef CONFIG_PLATFORM_EC_CONSOLE_CMD_KEYBOARD_8042
#define CONFIG_CMD_KEYBOARD
#endif

#undef CONFIG_KEYBOARD_COL2_INVERTED
#ifdef CONFIG_PLATFORM_EC_KEYBOARD_COL2_INVERTED
#define CONFIG_KEYBOARD_COL2_INVERTED
#endif  /* CONFIG_PLATFORM_EC_KEYBOARD_COL2_INVERTED */

#ifdef CONFIG_PLATFORM_EC_POWERSEQ_CPU_PROCHOT_ACTIVE_LOW
#define CONFIG_CHIPSET_CPU_PROCHOT_ACTIVE_LOW
#endif

#ifdef CONFIG_PLATFORM_EC_POWERSEQ_RSMRST_DELAY
#define CONFIG_CHIPSET_X86_RSMRST_DELAY
#endif

#ifdef CONFIG_PLATFORM_EC_POWERSEQ_SLP_S3_L_OVERRIDE
#define CONFIG_CHIPSET_SLP_S3_L_OVERRIDE
#endif

#ifdef CONFIG_PLATFORM_EC_POWERSEQ_PP3300_RAIL_FIRST
#define CONFIG_CHIPSET_PP3300_RAIL_FIRST
#endif

#ifdef CONFIG_PLATFORM_EC_POWERSEQ_RTC_RESET
#define CONFIG_BOARD_HAS_RTC_RESET
#endif

#ifdef CONFIG_PLATFORM_EC_POWERSEQ_PP5000_CONTROL
#define CONFIG_POWER_PP5000_CONTROL
#endif

#undef CONFIG_CMD_SHMEM
#ifdef CONFIG_PLATFORM_EC_CONSOLE_CMD_SHMEM
#define CONFIG_CMD_SHMEM
#endif

#ifdef CONFIG_PLATFORM_EC_TIMER
#define CONFIG_HWTIMER_64BIT
#define CONFIG_HW_SPECIFIC_UDELAY
#undef CONFIG_WATCHDOG

#undef CONFIG_CMD_GETTIME
#ifdef CONFIG_PLATFORM_EC_TIMER_CMD_GETTIME
#define CONFIG_CMD_GETTIME
#endif  /* CONFIG_PLATFORM_EC_TIMER_CMD_GETTIME */

#undef CONFIG_CMD_TIMERINFO
#ifdef CONFIG_PLATFORM_EC_TIMER_CMD_TIMERINFO
#define CONFIG_CMD_TIMERINFO
#endif  /* CONFIG_PLATFORM_EC_TIMER_CMD_TIMERINFO */

#undef CONFIG_CMD_WAITMS
#ifdef CONFIG_PLATFORM_EC_TIMER_CMD_WAITMS
#define CONFIG_CMD_WAITMS
#endif  /* CONFIG_PLATFORM_EC_TIMER_CMD_TIMERINFO */

#endif  /* CONFIG_PLATFORM_EC_TIMER */

/*
 * Load the chip family specific header. Normally for npcx, this would be done
 * by chip/npcx/config_chip.h but since this file is replacing that header
 * we'll need (for now) to load it ourselves. Long term, the functions, enums,
 * and constants in this header will be replaced by more Zephyr/devicetree
 * specific code.
 */
#ifdef CHIP_FAMILY_NPCX7
#include "config_chip-npcx7.h"
#endif /* CHIP_FAMILY_NPCX7 */

#endif  /* __CROS_EC_CONFIG_CHIP_H */
