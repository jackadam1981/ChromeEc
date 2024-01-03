/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Helipilot board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#include "compile_time_macros.h"

#undef CONFIG_SYSTEM_UNLOCKED

#define CONFIG_ALLOW_UNALIGNED_ACCESS
#define CONFIG_LTO

/*-------------------------------------------------------------------------*
 * Flash layout:
 *
 * +++++++++++++
 * |    RO     |
 * | ......... |
 * |  Rollback | (two sectors)
 * +-----------+
 * |    RW     |
 * |           |
 * |           |
 * |           |
 * |           |
 * +++++++++++++
 *
 * We adjust the following macros to accommodate for a rollback, RO,
 * and RW region of different sizes.
 *
 *-------------------------------------------------------------------------*
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

/*-------------------------------------------------------------------------*
 * Patch for reducing program memory size to increase data RAM
 *-------------------------------------------------------------------------*
 */
#undef NPCX_PROGRAM_MEMORY_SIZE
/* 352 KB program RAM */
#define NPCX_PROGRAM_MEMORY_SIZE ((416 - 64) * 1024)

#undef CONFIG_PROGRAM_MEMORY_BASE
#define CONFIG_PROGRAM_MEMORY_BASE 0x10058000

#undef CONFIG_RAM_BASE
/*
 * Adjust the base address of the Data RAM
 * 0x200C0000 - 64K (0x10000) memory address of Data RAM
 */
#define CONFIG_RAM_BASE 0x200B0000

#undef CONFIG_DATA_RAM_SIZE
/*
 * Define Data RAM size  = 160KB - 4KB (Reserved for booter).
 */
#define CONFIG_DATA_RAM_SIZE ((96 + 64) * 1024)

#undef CONFIG_RAM_SIZE
#define CONFIG_RAM_SIZE (CONFIG_DATA_RAM_SIZE - 0x1000)
/*-------------------------------------------------------------------------*/

#define CONFIG_SHAREDLIB_SIZE 0

#define CONFIG_RO_MEM_OFF 0

/* Need to account for the 64 (0x40) byte long firmware header */
#define CONFIG_RO_STORAGE_OFF 64
#define CONFIG_RO_SIZE (128 * 1024 - 0x1000)

#undef CONFIG_CODE_RAM_SIZE
#define CONFIG_CODE_RAM_SIZE NPCX_PROGRAM_MEMORY_SIZE

#define CONFIG_RO_PUBKEY_READ_ADDR                                      \
	(CONFIG_MAPPED_STORAGE_BASE + CONFIG_EC_PROTECTED_STORAGE_OFF + \
	 CONFIG_RO_PUBKEY_STORAGE_OFF)

#define CONFIG_RWSIG_READ_ADDR                                          \
	((CONFIG_MAPPED_STORAGE_BASE + CONFIG_EC_WRITABLE_STORAGE_OFF + \
	  CONFIG_RW_STORAGE_OFF + RW_SIG_OFFSET))
/*
 * Since NPCX9 executes out of SRAM and only one image (RO/RW) is loaded
 * from flash at a time, we don't apply an offset to program memory
 */
#define CONFIG_RW_MEM_OFF 0
#define CONFIG_RW_STORAGE_OFF 0

/*
 * The remaining flash size (CONFIG_FLASH_SIZE_BYTES -
 * (CONFIG_RW_MEM_OFF - CONFIG_RO_MEM_OFF) exceeds available program memory,
 * can only execute a program as big as the available program SRAM
 */
#define CONFIG_RW_SIZE                                          \
	((NPCX_PROGRAM_MEMORY_SIZE / CONFIG_FLASH_ERASE_SIZE) * \
	 CONFIG_FLASH_ERASE_SIZE)

#define CONFIG_EC_PROTECTED_STORAGE_OFF CONFIG_RO_MEM_OFF
#define CONFIG_EC_PROTECTED_STORAGE_SIZE (CONFIG_RO_SIZE + 0x1000)
#define CONFIG_EC_WRITABLE_STORAGE_OFF \
	(CONFIG_ROLLBACK_OFF + CONFIG_ROLLBACK_SIZE)

#define CONFIG_EC_WRITABLE_STORAGE_SIZE CONFIG_RW_SIZE

#define CONFIG_WP_STORAGE_OFF CONFIG_EC_PROTECTED_STORAGE_OFF
#define CONFIG_WP_STORAGE_SIZE CONFIG_EC_PROTECTED_STORAGE_SIZE
#define CONFIG_WP_ACTIVE_HIGH

/*
 * We want to prevent flash readout, and use it as indicator of protection
 * status.
 */
#define CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE

/*-------------------------------------------------------------------------*
 * Console Defines
 *-------------------------------------------------------------------------*
 */

/* Select which UART Controller is the Console UART */
#undef CONFIG_CONSOLE_UART
#define CONFIG_CONSOLE_UART 0 /* 0:UART1 1:UART2 */
#define NPCX_UART_MODULE2 1 /* 1:GPIO64/65 as UART1 */

#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 2048

#undef CONSOLE_TASK_STACK_SIZE
#define CONSOLE_TASK_STACK_SIZE 4096

/*-------------------------------------------------------------------------*
 * UART Host Command Interface Defines
 *-------------------------------------------------------------------------*
 */
#define NPCX_UART_BAUDRATE_3M

#undef CONFIG_UART_HOST_COMMAND_HW
#define CONFIG_UART_HOST_COMMAND_HW 1

#define CONFIG_USART_HOST_COMMAND

/*-------------------------------------------------------------------------*
 * Disable Features
 *-------------------------------------------------------------------------*
 */
#undef CONFIG_ADC
#undef CONFIG_CMD_ADC
#undef CONFIG_CMD_POWER_AP
#undef CONFIG_HIBERNATE
#undef CONFIG_LID_SWITCH
#undef CONFIG_SWITCH
#undef CONFIG_TEMP_SENSOR
#undef CONFIG_I2C
#undef CONFIG_TASK_PROFILING

#undef CONFIG_CMD_ACCELSPOOF
#undef CONFIG_CMD_APTHROTTLE
#undef CONFIG_CMD_I2C_SCAN
#undef CONFIG_CMD_CHARGER

#undef CONFIG_ACCELGYRO_BMI_COMM_SPI
#undef CONFIG_ACCELGYRO_ICM_COMM_SPI
#undef CONFIG_ACCEL_FIFO_SIZE
#undef CONFIG_ACCEL_SPOOF_MODE
#undef CONFIG_ADC_PROFILE_SINGLE
#undef CONFIG_ADC_WATCHDOG

#if 0
#undef CONFIG_AUX_TIMER_PERIOD_MS
#endif

#undef CONFIG_BATTERY_CRITICAL_SHUTDOWN_TIMEOUT
#undef CONFIG_BATTERY_CUTOFF_TIMEOUT_MSEC
#undef CONFIG_BATTERY_LOW_VOLTAGE_TIMEOUT
#undef CONFIG_BATTERY_MAX_IMBALANCE_MV
#undef CONFIG_BATTERY_PRECHARGE_TIMEOUT
#undef CONFIG_BATT_HOST_FULL_FACTOR
#undef CONFIG_BATT_HOST_SHUTDOWN_PERCENTAGE
#undef CONFIG_BC12_MAX14637_DELAY_FROM_OFF_TO_ON_MS
#undef CONFIG_BC12_SINGLE_DRIVER
#undef CONFIG_BODY_DETECTION_SENSOR
#undef CONFIG_BUTTON_DEBOUNCE
#undef CONFIG_CCD_USBC_PORT_NUMBER
#undef CONFIG_CHARGER_INPUT_CURRENT_DERATE_PCT
#undef CONFIG_CHARGER_MIN_BAT_PCT_FOR_POWER_ON
#undef CONFIG_CHARGER_MIN_BAT_PCT_FOR_POWER_ON_WITH_AC
#undef CONFIG_CHARGER_MIN_BAT_PCT_IMBALANCED_POWER_ON
#undef CONFIG_CHARGER_PROFILE_VOLTAGE_RANGES
#undef CONFIG_CHARGER_SINGLE_CHIP
#undef CONFIG_CHARGER_SM5803_IBAT_PHOT_SEL
#undef CONFIG_CHARGER_SM5803_PROCHOT_DURATION
#undef CONFIG_CHARGER_SM5803_VBUS_MON_SEL
#undef CONFIG_CHARGER_SM5803_VSYS_MON_SEL
#undef CONFIG_CHARGE_DEBUG
#undef CONFIG_CHARGE_MANAGER_BAT_PCT_SAFE_MODE_EXIT
#undef CONFIG_CHARGE_MANAGER_SAFE_MODE
#undef CONFIG_CHIPSET_POWER_SEQ_VERSION
#undef CONFIG_CHIP_PRE_INIT
#undef CONFIG_CMD_BATTFAKE
#undef CONFIG_CMD_CBI
#undef CONFIG_CMD_CHARGE_SUPPLIER_INFO
#undef CONFIG_CMD_CRASH
#undef CONFIG_CMD_DEVICE_EVENT
#undef CONFIG_CMD_FASTCHARGE
#undef CONFIG_CMD_FLASH
#undef CONFIG_CMD_FLASH_WP
#undef CONFIG_CMD_GETTIME
#undef CONFIG_CMD_HASH
#undef CONFIG_CMD_HCDEBUG
#undef CONFIG_CMD_I2C_XFER
#undef CONFIG_CMD_IDLE_STATS
#undef CONFIG_CMD_INA
#undef CONFIG_CMD_KEYBOARD
#undef CONFIG_CMD_MD
#undef CONFIG_CMD_MEM
#undef CONFIG_CMD_MFALLOW
#undef CONFIG_CMD_MMAPINFO
#undef CONFIG_CMD_PD
#undef CONFIG_CMD_PECI
#undef CONFIG_CMD_POWERINDEBUG
#undef CONFIG_CMD_REGULATOR
#undef CONFIG_CMD_RW
#undef CONFIG_CMD_SHMEM
#undef CONFIG_CMD_SLEEPMASK
#undef CONFIG_CMD_SLEEPMASK_SET
#undef CONFIG_CMD_SYSINFO
#undef CONFIG_CMD_SYSJUMP
#undef CONFIG_CMD_SYSLOCK
#undef CONFIG_CMD_TEMP_SENSOR
#undef CONFIG_CMD_TIMERINFO
#undef CONFIG_CMD_TYPEC
#undef CONFIG_CMD_WAITMS

#if 0
#undef CONFIG_CODE_RAM_SIZE
#endif

#if 0
#undef CONFIG_COMMON_GPIO
#undef CONFIG_COMMON_PANIC_OUTPUT
#undef CONFIG_COMMON_RUNTIME
#undef CONFIG_COMMON_TIMER
#endif

#undef CONFIG_CONSOLE_CMDHELP
#undef CONFIG_CONSOLE_COMMAND_FLAGS
#undef CONFIG_CONSOLE_ENABLE_READ_V1
#undef CONFIG_CONSOLE_HISTORY

#if 0
#undef CONFIG_CONSOLE_INPUT_LINE_SIZE
#endif

#if 0
#undef CONFIG_CONSOLE_UART
#endif

#undef CONFIG_CONSOLE_VERBOSE
#undef CONFIG_CROS_FWID_VERSION

#if 0
#undef CONFIG_DATA_RAM_SIZE
#endif

#undef CONFIG_DEBUG_ASSERT
#undef CONFIG_DEBUG_ASSERT_REBOOTS
#undef CONFIG_DEBUG_EXCEPTIONS
#undef CONFIG_DEBUG_STACK_OVERFLOW

/* TODO(tomhughes): set to 0? */
#if 0
#undef CONFIG_DEDICATED_CHARGE_PORT_COUNT
#endif

#undef CONFIG_DMA_DEFAULT_HANDLERS
#undef CONFIG_DSW_PWROK_TO_PWRBTN_US
#undef CONFIG_EC_MAX_SENSOR_FREQ_DEFAULT_MILLIHZ
#undef CONFIG_EC_MAX_SENSOR_FREQ_MILLIHZ

#if 0
#undef CONFIG_EC_PROTECTED_STORAGE_OFF
#undef CONFIG_EC_PROTECTED_STORAGE_SIZE
#undef CONFIG_EC_WRITABLE_STORAGE_OFF
#undef CONFIG_EC_WRITABLE_STORAGE_SIZE
#endif

#undef CONFIG_EVENT_LOG_SIZE

/* TODO(tomhughes): necessary? */
#if 0
#undef CONFIG_EXTERNAL_STORAGE
#endif

#undef CONFIG_EXTPOWER
#undef CONFIG_EXTPOWER_DEBOUNCE_MS
#undef CONFIG_FAN_INIT_SPEED
#undef CONFIG_FINGERPRINT_MCU

#if 0
#undef CONFIG_FLASH_BANK_SIZE
#undef CONFIG_FLASH_CROS
#undef CONFIG_FLASH_ERASE_SIZE
#undef CONFIG_FLASH_PHYSICAL
#undef CONFIG_FLASH_PROTECT_DEFERRED
#undef CONFIG_FLASH_PSTATE_BANK
#undef CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE
#undef CONFIG_FLASH_SIZE_BYTES
#undef CONFIG_FLASH_WRITE_IDEAL_SIZE
#undef CONFIG_FLASH_WRITE_SIZE
#endif

#if 0
#undef CONFIG_FMAP
#undef CONFIG_FPU
#undef CONFIG_FPU_WARNINGS
#undef CONFIG_FW_INCLUDE_RO
#endif

#undef CONFIG_GESTURE_DETECTION_MASK
#undef CONFIG_GESTURE_SIGMO_SENSOR
#undef CONFIG_GESTURE_TAP_MAX_INTERSTICE_T
#undef CONFIG_GESTURE_TAP_SENSOR
#undef CONFIG_GESTURE_TAP_THRES_MG

#if 0
#undef CONFIG_GOOGLETEST
#endif

#undef CONFIG_HARD_SLEEP_HANG_TIMEOUT
#undef CONFIG_HIBERNATE_DELAY_SEC

#if 0
#undef CONFIG_HIBERNATE_PSL_OUT_FLAGS
#endif

/* TODO(tomhughes): what is this? */
#if 0
#undef CONFIG_HOSTCMD_DEBUG_MODE
#endif

#undef CONFIG_HOSTCMD_EVENTS
#undef CONFIG_HOSTCMD_FLASHPD
#undef CONFIG_HOSTCMD_GET_UPTIME_INFO
#undef CONFIG_HOSTCMD_LOCATE_CHIP
#undef CONFIG_HOSTCMD_PD_CHIP_INFO

#if 0
#undef CONFIG_HOSTCMD_RATE_LIMITING_MIN_REST
#undef CONFIG_HOSTCMD_RATE_LIMITING_PERIOD
#undef CONFIG_HOSTCMD_RATE_LIMITING_RECESS
#endif

#undef CONFIG_HOSTCMD_RWHASHPD
#undef CONFIG_HOSTCMD_TYPEC_CONTROL
#undef CONFIG_HOSTCMD_TYPEC_DISCOVERY
#undef CONFIG_HOSTCMD_TYPEC_STATUS
#undef CONFIG_HOST_EVENT_REPORT_MASK
#undef CONFIG_HOST_INTERFACE_ESPI_DEFAULT_VW_WIDTH_US
#undef CONFIG_HOST_INTERFACE_SHI
#undef CONFIG_I2C_CHIP_MAX_TRANSFER_SIZE
#undef CONFIG_I2C_EXTRA_PACKET_SIZE
#undef CONFIG_I2C_MULTI_PORT_CONTROLLER
#undef CONFIG_I2C_NACK_RETRY_COUNT
#undef CONFIG_IMAGE_PADDING

#if 0
#undef CONFIG_IRQ_COUNT
#endif

#undef CONFIG_ISL9238C_BUCK_PHASE_VOLTAGE
#undef CONFIG_KEYBOARD_BOOT_KEYS
#undef CONFIG_KEYBOARD_KSO_BASE
#undef CONFIG_KEYBOARD_RUNTIME_KEYS
#undef CONFIG_LED_PWM_CHARGE_COLOR
#undef CONFIG_LED_PWM_CHARGE_ERROR_COLOR
#undef CONFIG_LED_PWM_LOW_BATT_COLOR
#undef CONFIG_LED_PWM_NEAR_FULL_COLOR
#undef CONFIG_LED_PWM_SOC_ON_COLOR
#undef CONFIG_LED_PWM_SOC_SUSPEND_COLOR
#undef CONFIG_LID_ANGLE_SENSOR_BASE
#undef CONFIG_LID_ANGLE_SENSOR_LID

#if 0
#undef CONFIG_LTO
#endif

#if 0
#undef CONFIG_MAPPED_STORAGE
#undef CONFIG_MAPPED_STORAGE_BASE
#endif

#undef CONFIG_MIA_WDT_VEC
#undef CONFIG_MKBP_EVENT
#undef CONFIG_MKBP_USE_GPIO
#undef CONFIG_MOTION_MIN_SENSE_WAIT_TIME
#undef CONFIG_MOTION_SENSE_RESUME_DELAY_US
#undef CONFIG_MOTION_SENSE_SUSPEND_DELAY_US

#if 0
#undef CONFIG_MPU
#endif

#undef CONFIG_OCPC_DEF_DRIVELIMIT_MILLIVOLTS

#if 0
#undef CONFIG_PANIC_DATA_BASE
#undef CONFIG_PANIC_DATA_SIZE
#undef CONFIG_PANIC_STRIP_GPR
#endif

#undef CONFIG_PD_RETRY_COUNT
#undef CONFIG_PORT80_HISTORY_LEN
#undef CONFIG_PORT80_PRINT_IN_INT
#undef CONFIG_POWER_BUTTON_INIT_TIMEOUT
#undef CONFIG_PRESERVED_END_OF_RAM_SIZE
#undef CONFIG_PRINTF_LONG_IS_32BITS

#if 0
#undef CONFIG_PROGRAM_MEMORY_BASE
#endif

#undef CONFIG_RAA489000_TRICKLE_CHARGE_CURRENT

#if 0
#undef CONFIG_RAM_BANKS
#undef CONFIG_RAM_BANK_SIZE
#undef CONFIG_RAM_BASE
#undef CONFIG_RAM_SIZE
#endif

#if 0
#undef CONFIG_RESTRICTED_CONSOLE_COMMANDS
#endif

#undef CONFIG_RISCV_EXTENSION_M

#if 0
#undef CONFIG_RNG
#undef CONFIG_ROLLBACK
#undef CONFIG_ROLLBACK_MPU_PROTECT
#undef CONFIG_ROLLBACK_OFF
#undef CONFIG_ROLLBACK_SECRET_SIZE
#undef CONFIG_ROLLBACK_SIZE
#undef CONFIG_ROLLBACK_VERSION
#endif

#if 0
#undef CONFIG_RO_HDR_MEM_OFF
#undef CONFIG_RO_HDR_SIZE
#undef CONFIG_RO_MEM_OFF
#undef CONFIG_RO_PUBKEY_READ_ADDR
#undef CONFIG_RO_ROM_RESIDENT_MEM_OFF
#undef CONFIG_RO_ROM_RESIDENT_SIZE
#undef CONFIG_RO_SIZE
#undef CONFIG_RO_STORAGE_OFF
#undef CONFIG_RSA_EXPONENT_3
#undef CONFIG_RSA_KEY_SIZE
#undef CONFIG_RTC
#undef CONFIG_RWSIG_JUMP_TIMEOUT
#undef CONFIG_RWSIG_READ_ADDR
#undef CONFIG_RWSIG_TYPE_RWSIG
#endif

#if 0
#undef CONFIG_RW_MEM_OFF
#undef CONFIG_RW_ROM_RESIDENT_MEM_OFF
#undef CONFIG_RW_ROM_RESIDENT_SIZE
#undef CONFIG_RW_SIZE
#undef CONFIG_RW_STORAGE_OFF
#endif

#undef CONFIG_SENSOR_TIGHT_TIMESTAMPS
#undef CONFIG_SHA256
#undef CONFIG_SHA256_HW_ACCELERATE
#undef CONFIG_SHA256_UNROLLED
#undef CONFIG_SHAREDLIB_SIZE
#undef CONFIG_SHAREDMEM_MINIMUM_SIZE
#undef CONFIG_SHAREDMEM_MINIMUM_SIZE_RWSIG
#undef CONFIG_SLEEP_TIMEOUT_MS

#if 0
#undef CONFIG_SPI_CONTROLLER
#undef CONFIG_SPI_FLASH_READ_WAIT_MS
#undef CONFIG_SPI_FLASH_REGS
#undef CONFIG_SPI_FLASH_W25Q80
#undef CONFIG_SPI_FP_PORT
#endif

#if 0
#undef CONFIG_STACK_SIZE
#endif

#undef CONFIG_STM32_EXTENDED_RESET_FLAGS
#undef CONFIG_SUPPORT_CHIP_HIBERNATION
#undef CONFIG_SUPPRESSED_HOST_COMMANDS
#undef CONFIG_SYNC_QUEUE_SIZE
#undef CONFIG_SYSTEM_SAFE_MODE_PRINT_STACK
#undef CONFIG_SYSTEM_SAFE_MODE_TIMEOUT_MSEC
#undef CONFIG_SYV682X_HV_ILIM
#undef CONFIG_TCPC_I2C_BASE_ADDR_FLAGS

#if 0
#undef CONFIG_UART_BAUD_RATE
#undef CONFIG_UART_HOST_COMMAND_HW
#undef CONFIG_UART_RX_BUF_SIZE
#undef CONFIG_UART_RX_DMA_RECHECKS
#undef CONFIG_UART_TX_BUF_SIZE
#endif

#undef CONFIG_UPDATE_PDU_SIZE
#undef CONFIG_USART_HOST_COMMAND
#undef CONFIG_USBC_PPC_LOGGING
#undef CONFIG_USBC_VCONN_SWAP_DELAY_US
#undef CONFIG_USB_CONSOLE_TX_BUF_SIZE
#undef CONFIG_USB_DPM_SM
#undef CONFIG_USB_I2C_MAX_READ_COUNT
#undef CONFIG_USB_I2C_MAX_WRITE_COUNT
#undef CONFIG_USB_MAXPOWER_MA
#undef CONFIG_USB_PD_CONSOLE_CMD
#undef CONFIG_USB_PD_DEBUG_DR
#undef CONFIG_USB_PD_FLAGS
#undef CONFIG_USB_PD_HOST_CMD
#undef CONFIG_USB_PD_I2C_ADDR_FLAGS
#undef CONFIG_USB_PD_INITIAL_DRP_STATE
#undef CONFIG_USB_PD_LONG_PRESS_MAX_MS
#undef CONFIG_USB_PD_LOW_POWER
#undef CONFIG_USB_PD_PRL_EVENT_LOG_CAPACITY
#undef CONFIG_USB_PD_PULLUP
#undef CONFIG_USB_PD_RX_COMP_IRQ
#undef CONFIG_USB_PD_SHORT_PRESS_MAX_MS
#undef CONFIG_USB_PD_STARTUP_DELAY_MS
#undef CONFIG_USB_PD_TCPC_LPM_EXIT_DEBOUNCE
#undef CONFIG_USB_PD_TCPC_VCONN
#undef CONFIG_USB_PD_TCPMV1_DEBUG
#undef CONFIG_USB_PD_TEMP_SENSOR
#undef CONFIG_USB_PD_TRY_SRC_MIN_BATT_SOC
#undef CONFIG_USB_PE_SM
#undef CONFIG_USB_PORT_POWER_SMART_PORT_COUNT
#undef CONFIG_USB_PRL_SM
#undef CONFIG_USB_TYPEC_SM
#undef CONFIG_USB_VID

#if 0
#undef CONFIG_WATCHDOG
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_WATCHDOG_MAX_RETRIES
#undef CONFIG_WATCHDOG_PERIOD_MS
#undef CONFIG_WATCHDOG_WARNING_LEADING_TIME_MS
#endif

#if 0
#undef CONFIG_WP_ACTIVE_HIGH
#undef CONFIG_WP_STORAGE_OFF
#undef CONFIG_WP_STORAGE_SIZE
#endif

/*-------------------------------------------------------------------------*
 * Enable Features
 *-------------------------------------------------------------------------*
 */
#define CONFIG_BORINGSSL_CRYPTO
#define CONFIG_CMD_FLASH
#define CONFIG_CMD_IDLE_STATS
#define CONFIG_FPU
#define CONFIG_FPU_WARNINGS
#define CONFIG_GOOGLETEST
#define CONFIG_HOST_INTERFACE_SHI
#define CONFIG_MKBP_EVENT
#define CONFIG_MKBP_USE_GPIO
#define CONFIG_PANIC_STRIP_GPR
#define CONFIG_PRINTF_LONG_IS_32BITS
#define CONFIG_RNG
#define CONFIG_SHA256
#define CONFIG_SHA256_UNROLLED
#ifdef SECTION_IS_RW
#define CONFIG_SPI
#define CONFIG_CMD_SPI_XFER
/* TODO(b/130249462): remove for release */
#define CONFIG_CMD_FPSENSOR_DEBUG
#define CONFIG_LOW_POWER_IDLE
#endif

/*-------------------------------------------------------------------------*
 * Watchdog
 *-------------------------------------------------------------------------*
 */

/*
 * RW does slow compute, RO does slow flash erase.
 */
#undef CONFIG_WATCHDOG_PERIOD_MS
#define CONFIG_WATCHDOG_PERIOD_MS 10000
#define CONFIG_WATCHDOG_HELP

/*-------------------------------------------------------------------------*
 * Fingerprint Specific
 *-------------------------------------------------------------------------*
 */

/* SPI configuration for the fingerprint sensor */
#define CONFIG_SPI_CONTROLLER
#define CONFIG_SPI_FP_PORT 0 /* SPI0: only one SPIP (SPI Peripheral) */
#define CONFIG_FINGERPRINT_MCU
#ifdef SECTION_IS_RW
#define CONFIG_FP_SENSOR_FPC1025
/*
 * Use the malloc code only in the RW section (for the private library),
 * we cannot enable it in RO since it is not compatible with the RW
 * verification  (shared_mem_init done too late).
 */
#define CONFIG_MALLOC
/*
 * FP buffers are allocated in regular SRAM
 * TODO(b/124773209): Instead of defining to empty, #undef once all CLs that
 * depend on FP_*_SECTION have landed. Also rename the variables to CONFIG_*.
 */
#define FP_FRAME_SECTION
#define FP_TEMPLATE_SECTION
#endif /* SECTION_IS_RW */

/*
 * These allow console commands to be flagged as restricted.
 * Restricted commands will only be permitted to run when
 * console_is_restricted() returns false.
 * See console_is_restricted's definition in board.c.
 */
#define CONFIG_CONSOLE_COMMAND_FLAGS
#define CONFIG_RESTRICTED_CONSOLE_COMMANDS

/*-------------------------------------------------------------------------*
 * Rollback Block
 *-------------------------------------------------------------------------*
 */

#define CONFIG_ROLLBACK
#define CONFIG_ROLLBACK_SECRET_SIZE 32
#define CONFIG_MPU
#define CONFIG_ROLLBACK_MPU_PROTECT

/*
 * We do not use any "locally" generated entropy: this is normally used
 * to add local entropy when the main source of entropy is remote.
 */
#undef CONFIG_ROLLBACK_SECRET_LOCAL_ENTROPY_SIZE
#ifdef SECTION_IS_RW
#undef CONFIG_ROLLBACK_UPDATE
#endif

/* EC rollback protection block */
#define CONFIG_ROLLBACK_OFF \
	(CONFIG_EC_PROTECTED_STORAGE_OFF + CONFIG_EC_PROTECTED_STORAGE_SIZE)
#define CONFIG_ROLLBACK_SIZE (128 * 1024 * 2) /* 2 blocks of 128KB each */

/*-------------------------------------------------------------------------*
 * RW Signature Verification
 *-------------------------------------------------------------------------*
 */

#ifdef SECTION_IS_RO
/* RO verifies the RW partition signature */
#define CONFIG_RSA
#define CONFIG_RWSIG
#endif /* SECTION_IS_RO */
#define CONFIG_RSA_KEY_SIZE 3072
#define CONFIG_RSA_EXPONENT_3
#define CONFIG_RWSIG_TYPE_RWSIG

/*-------------------------------------------------------------------------*
 * Chip Specific
 *-------------------------------------------------------------------------*
 */

/*
 * Macros for GPIO signals used in common code that don't match the
 * schematic names. Signal names in gpio.inc match the schematic and are
 * then redefined here to so it's more clear which signal is being used for
 * which purpose.
 */
#define GPIO_WP GPIO_HOST_MCU_WP_OD
#define GPIO_SHI_CS_L GPIO_SPI_HOST_CS_MCU_ODL
#define GPIO_FPS_INT GPIO_FP_MCU_INT_L
#define GPIO_EC_INT_L GPIO_MCU_PLATFORM_INT_L
#define GPIO_SLP_ALT_L GPIO_SLP_L

#ifndef __ASSEMBLER__

#include "board_rw.h"
#include "gpio_signal.h" /* needed by registers.h */
#include "registers.h"

void slp_event(enum gpio_signal signal);
void fps_event(enum gpio_signal signal);
#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
