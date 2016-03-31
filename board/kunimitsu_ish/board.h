/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Skylake Chrome Reference Design board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Accelerometer */
#ifdef HAS_TASK_MOTIONSENSE
#define CONFIG_ACCEL_KXCJ9
#endif
#define CONFIG_CMD_ACCEL_INFO

/* Optional features */
#define CONFIG_BOARD_VERSION
#define CONFIG_CLOCK_CRYSTAL
#undef  CONFIG_DEBUG_ASSERT
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
/* All data won't fit in data RAM.  So, moving boundary slightly. */
#undef CONFIG_RO_SIZE
#define CONFIG_RO_SIZE (104 * 1024)

#define CONFIG_FLASH_SIZE 524288
#define CONFIG_FPU

/* Undefine features */
#undef CONFIG_ADC_WATCHDOG
#undef CONFIG_BATTERY_CRITICAL_SHUTDOWN_TIMEOUT
#undef CONFIG_CHIPSET_HAS_PP1350
#undef CONFIG_CHIPSET_HAS_PP5000
#undef CONFIG_CMD_CHARGER
#undef CONFIG_CMD_FASTCHARGE
#undef CONFIG_CMD_HASH
#undef CONFIG_CMD_I2C_SCAN
#undef CONFIG_CMD_I2C_XFER
#undef CONFIG_CMD_IDLE_STATS
#undef CONFIG_CMD_KEYBOARD
#undef CONFIG_CMD_INA
#undef CONFIG_CMD_REGULATOR
#undef CONFIG_CMD_PD
#undef CONFIG_CMD_POWER_AP
#undef CONFIG_CMD_POWERINDEBUG
#undef CONFIG_CMD_SHMEM
#undef CONFIG_CMD_TEMP_SENSOR
#undef CONFIG_CMD_TIMERINFO
#undef CONFIG_CMD_TYPEC
#undef CONFIG_CMD_USBMUX
#undef CONFIG_COMMON_GPIO
#undef CONFIG_EXTPOWER
#undef CONFIG_KEYBOARD_KSO_BASE
#undef CONFIG_USB_PD_COMM_ENABLED
#undef CONFIG_USB_PD_DEBUG_DR
#undef CONFIG_USB_PD_I2C_SLAVE_ADDR
#undef CONFIG_USB_PD_LOW_POWER
#undef CONFIG_USB_PD_RX_COMP_IRQ
#undef CONFIG_TCPC_I2C_BASE_ADDR
#undef CONFIG_USB_PD_TRY_SRC_MIN_BATT_SOC
#undef CONFIG_CRC8
#undef CONFIG_FLASH
#undef CONFIG_FMAP
#undef CONFIG_LID_SWITCH
#undef CONFIG_SWITCH
#undef CONFIG_SOFTWARE_PANIC

#define CONFIG_POLLING_UART
/*
 * Allow dangerous commands.
 * TODO(shawnn): Remove this config before production.
 */
#define CONFIG_SYSTEM_UNLOCKED
#define I2C_PORT_ALS MEC1322_I2C2
#define I2C_PORT_ACCEL ISH30_I2C1
#define I2C_PORT_GYRO MEC1322_I2C2
#undef HAS_TASK_LIGHTBAR
#undef HAS_TASK_PDCMD

#define OPT3001_I2C_ADDR OPT3001_I2C_ADDR1

/* Modules we want to exclude */
#undef CONFIG_CMD_ACCELS
#undef CONFIG_CMD_HASH
#undef CONFIG_CMD_TEMP_SENSOR
#undef CONFIG_CMD_TIMERINFO
#undef CONFIG_CONSOLE_CMDHELP
#undef CONFIG_CONSOLE_HISTORY
#undef CONFIG_PECI
#undef CONFIG_SHA256
/* Enable Pseudo G3 */
#define CONFIG_LOW_POWER_PSEUDO_G3

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

/* power signal definitions */
enum power_signal {
	X86_RSMRST_L_PWRGD = 0,
	X86_SLP_S0_DEASSERTED,
	X86_SLP_S3_DEASSERTED,
	X86_SLP_S4_DEASSERTED,
	X86_SLP_SUS_DEASSERTED,
	/* Number of X86 signals */
	POWER_SIGNAL_COUNT
};

/* Light sensors */
enum als_id {
	ALS_OPT3001 = 0,

	ALS_COUNT,
};

/* start as a sink in case we have no other power supply/battery */
#define PD_DEFAULT_STATE PD_STATE_SNK_DISCONNECTED

/* Reset PD MCU */
void board_reset_pd_mcu(void);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
