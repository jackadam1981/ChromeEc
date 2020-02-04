/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * STM32H7A3 + FPC 1145 Fingerprint MCU configuration
 */

#ifndef __BOARD_H
#define __BOARD_H

#undef CONFIG_SYSTEM_UNLOCKED

/* TODO(hesling): Disable */
/* #define DEBUG 1 */
/* #define CONFIG_SOFTWARE_PANIC */
#define CONFIG_DEBUG_EXCEPTIONS
/* #define CONFIG_DEBUG_STACK_OVERFLOW */
#define CONFIG_DEBUG_EXCEPTION_BREAKPOINT
#define CONFIG_DEBUG_EXCEPTION_HANG
#define CONFIG_SOFTWARE_PANIC
/* #define CONFIG_DEBUG_BRINGUP */

/*
 * The checks that this enables seem to be broken
 *
 * #0  panic_assert_fail (msg=msg@entry=0x800abb0 "current_task != (task_ *)scratchpad", func=func@entry=0x800aadf <__func__.7674> "task_get_current", fname=fname@entry=0x800ab71 "core/cortex-m/task.c", linenum=linenum@entry=249) at common/panic_output.c:97
 * #1  0x08008274 in task_get_current () at core/cortex-m/task.c:249
 * #2  task_get_current () at core/cortex-m/task.c:245
 * #3  0x08008540 in mutex_lock (mtx=mtx@entry=0x240004f0 <bkpdata_write_mutex>) at core/cortex-m/task.c:855
 * #4  0x08001f20 in bkpdata_write (index=<optimized out>, value=<optimized out>) at chip/stm32/system.c:115
 * #5  0x080020d0 in check_reset_cause () at chip/stm32/system.c:181
 * #6  system_pre_init () at chip/stm32/system.c:336
 * #7  0x080047d4 in main () at common/main.c:91
 * #8  0x0800031a in data_loop () at core/cortex-m/init.S:88
 * Backtrace stopped: previous frame identical to this frame (corrupt stack?)
 * */
#undef CONFIG_ARMV7M_CACHE

/*
 * Flash layout:
 *
 * +++++++++++++
 * |    RO     | Bank 1
 * |           |
 * |           |
 * | ......... |
 * |  Rollback | (last two sectors)
 * +-----------+
 * |    RW     | Bank 2
 * |           |
 * |           |
 * |           |
 * |           |
 * +++++++++++++
 *
 * We adjust the following macros to accommodate a rollback region
 * and RO/RW regions of different sizes.
 */

#undef _IMAGE_SIZE
#undef CONFIG_ROLLBACK_OFF
#undef CONFIG_ROLLBACK_SIZE
#undef CONFIG_FLASH_PSTATE
#undef CONFIG_FW_PSTATE_SIZE
#undef CONFIG_FW_PSTATE_OFF
#undef CONFIG_SHAREDLIB_SIZE
#undef CONFIG_RO_MEM_OFF
#undef CONFIG_RO_STORAGE_OFF
#undef CONFIG_RO_SIZE
#undef CONFIG_RW_MEM_OFF
#undef CONFIG_RW_STORAGE_OFF
#undef CONFIG_RW_SIZE
#undef CONFIG_EC_PROTECTED_STORAGE_OFF
#undef CONFIG_EC_PROTECTED_STORAGE_SIZE
#undef CONFIG_EC_WRITABLE_STORAGE_OFF
#undef CONFIG_EC_WRITABLE_STORAGE_SIZE
#undef CONFIG_WP_STORAGE_OFF
#undef CONFIG_WP_STORAGE_SIZE

#define CONFIG_SHAREDLIB_SIZE   0

/* FIXME(hesling): Write protect resolution is every 4 8kb blocks. This may need to be changed to 4 blocks, instead. */
#define MAX_SIZE(a,b) (((a)>(b)) ? (a) : (b))

/*
 * EC rollback protection block
 *
 * We only need 2 blocks for rollback, but write protection of the RO region will overlap
 * the rollback blocks if we do not delegate an entire write protection region.
 */
#define CONFIG_ROLLBACK_SIZE   (MAX_SIZE(CONFIG_FLASH_WP_BANKS, 2) * CONFIG_FLASH_BANK_SIZE)
#define CONFIG_ROLLBACK_OFF    ((CONFIG_FLASH_SIZE / 2) - CONFIG_ROLLBACK_SIZE)

#define CONFIG_RO_MEM_OFF      0
#define CONFIG_RO_SIZE         CONFIG_ROLLBACK_OFF
#define CONFIG_RW_MEM_OFF      (CONFIG_FLASH_SIZE / 2)
#define CONFIG_RW_SIZE         (CONFIG_FLASH_SIZE / 2)

#define CONFIG_RO_STORAGE_OFF  0
#define CONFIG_RW_STORAGE_OFF  0

#define CONFIG_EC_PROTECTED_STORAGE_OFF   CONFIG_RO_MEM_OFF
#define CONFIG_EC_PROTECTED_STORAGE_SIZE  CONFIG_RO_SIZE
#define CONFIG_EC_WRITABLE_STORAGE_OFF    CONFIG_RW_MEM_OFF
#define CONFIG_EC_WRITABLE_STORAGE_SIZE   CONFIG_RW_SIZE

#define CONFIG_WP_STORAGE_OFF             CONFIG_EC_PROTECTED_STORAGE_OFF
#define CONFIG_WP_STORAGE_SIZE            CONFIG_EC_PROTECTED_STORAGE_SIZE

/* Disabled features */

#undef CONFIG_ADC
#undef CONFIG_HIBERNATE
#undef CONFIG_I2C
#undef CONFIG_LID_SWITCH
#undef CONFIG_TASK_PROFILING

/* Enabled features */

#define CONFIG_AES
#define CONFIG_AES_GCM
#define CONFIG_DMA
#define CONFIG_FORCE_CONSOLE_RESUME
#define CONFIG_FPU
#define CONFIG_HOST_COMMAND_STATUS
#define CONFIG_LOW_POWER_IDLE
#define CONFIG_MKBP_EVENT
#define CONFIG_MKBP_USE_GPIO
#define CONFIG_PRINTF_LEGACY_LI_FORMAT
#define CONFIG_RNG
#define CONFIG_SHA256
#define CONFIG_SHA256_UNROLLED
#define CONFIG_SPI
#define CONFIG_STM_HWTIMER32
#define CONFIG_SUPPRESSED_HOST_COMMANDS \
	EC_CMD_CONSOLE_SNAPSHOT, EC_CMD_CONSOLE_READ, EC_CMD_PD_GET_LOG_ENTRY
#define CONFIG_WATCHDOG_HELP
#define CONFIG_WP_ACTIVE_HIGH

/*
 * We want to prevent flash readout, and use it as indicator of protection
 * status.
 */
#define CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE

/*
 * RW does slow compute, RO does slow flash erase.
 */
#undef CONFIG_WATCHDOG_PERIOD_MS
#define CONFIG_WATCHDOG_PERIOD_MS 10000

/*
 * These allow console commands to be flagged as restricted.
 * Restricted commands will only be permitted to run when
 * console_is_restricted() returns false.
 * See console_is_restricted's definition in board.c.
 */
#define CONFIG_CONSOLE_COMMAND_FLAGS
#define CONFIG_RESTRICTED_CONSOLE_COMMANDS

/* SPI configuration for the fingerprint sensor */
#define CONFIG_SPI_MASTER
#define CONFIG_SPI_FP_PORT  2 /* SPI4: third master config */

/* Setup UART console */

#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 1
#define CONFIG_UART_TX_DMA
#define CONFIG_UART_TX_DMA_PH DMAMUX1_REQ_USART1_TX
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 2048

/* Console commands */

#define CONFIG_CMD_FLASH
#define CONFIG_CMD_FPSENSOR_DEBUG
#define CONFIG_CMD_IDLE_STATS
#define CONFIG_CMD_SPI_XFER

#ifdef SECTION_IS_RW
#	define CONFIG_FP_SENSOR_FPC1145
#	define CONFIG_CMD_FPSENSOR_DEBUG
	/*
	 * Use the malloc code only in the RW section (for the private library),
	 * we cannot enable it in RO since it is not compatible with the
	 * RW verification (shared_mem_init done too late).
	 */
#	define CONFIG_MALLOC
	/* Special memory regions to store large arrays */
#	define FP_FRAME_SECTION     __SECTION(ahb)
#	define FP_TEMPLATE_SECTION /* leave in main memory */

#else /* !SECTION_IS_RW */
	/* RO verifies the RW partition signature */
#	define CONFIG_RSA
#	define CONFIG_RWSIG
#endif  /* SECTION_IS_RW */

#define CONFIG_RSA_KEY_SIZE 3072
#define CONFIG_RSA_EXPONENT_3
#define CONFIG_RWSIG_TYPE_RWSIG

/*
 * We do not use any "locally" generated entropy: this is normally used
 * to add local entropy when the main source of entropy is remote.
 */
#undef CONFIG_ROLLBACK_SECRET_LOCAL_ENTROPY_SIZE
#ifdef SECTION_IS_RW
#	undef CONFIG_ROLLBACK_UPDATE
#endif

/*
 * Add rollback protection
 */
#define CONFIG_ROLLBACK
#define CONFIG_ROLLBACK_SECRET_SIZE 32
#define CONFIG_ROLLBACK_MPU_PROTECT

#ifndef __ASSEMBLER__
	/* Timer selection */
#	define TIM_CLOCK32 2
#	define TIM_WATCHDOG 16

#	include "gpio_signal.h"

void fps_event(enum gpio_signal signal);

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
