/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Eve board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/*
 * Allow dangerous commands.
 * TODO: Remove this config before production.
 */
#define CONFIG_SYSTEM_UNLOCKED
#undef HAS_TASK_LIGHTBAR
#undef HAS_TASK_PDCMD

/*
 * By default, enable all console messages excepted HC, ACPI and event:
 * The sensor stack is generating a lot of activity.
 */
#define CC_DEFAULT     (CC_ALL & ~(CC_MASK(CC_EVENTS) | CC_MASK(CC_LPC)))
#undef CONFIG_HOSTCMD_DEBUG_MODE
#define CONFIG_HOSTCMD_DEBUG_MODE HCDEBUG_OFF

/* ISH specific*/
#define CONFIG_ISH_30
#undef  CONFIG_DEBUG_ASSERT
#define CONFIG_CLOCK_CRYSTAL
#define CONFIG_POLLING_UART

/* EC */
#define CONFIG_FLASH_SIZE 0x80000
#define CONFIG_FPU
#define CONFIG_I2C
#define CONFIG_I2C_MASTER

/* I2C ports */
#define I2C_PORT_ALS            ISH_I2C0
#define I2C_PORT_CHARGER        ISH_I2C0
#define I2C_PORT_BATTERY        ISH_I2C0
#define I2C_PORT_GYRO           ISH_I2C0
#define I2C_PORT_BARO           ISH_I2C0
#define I2C_PORT_ACCEL          I2C_PORT_GYRO

/* Sensor */
#define CONFIG_MKBP_EVENT
#define CONFIG_MKBP_USE_HOST_EVENT
#define CONFIG_ACCELGYRO_BMI160
#define CONFIG_MAG_BMI160_BMM150
#define BMM150_I2C_ADDRESS BMM150_ADDR0	/* 8-bit address */
#define CONFIG_MAG_CALIBRATE

#define OPT3001_I2C_ADDR OPT3001_I2C_ADDR1
#define CONFIG_BARO_BMP280

/* FIFO size is in power of 2. */
#define CONFIG_ACCEL_FIFO 1024

/* Depends on how fast the AP boots and typical ODRs */
#define CONFIG_ACCEL_FIFO_THRES (CONFIG_ACCEL_FIFO / 3)

/* Undefine unfeatures */
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

/* Modules we want to exclude */
#undef CONFIG_CMD_ACCELS
#undef CONFIG_CMD_HASH
#undef CONFIG_CMD_TEMP_SENSOR
#undef CONFIG_CMD_TIMERINFO
#undef CONFIG_CONSOLE_CMDHELP
#undef CONFIG_CONSOLE_HISTORY
#undef CONFIG_PECI
#undef CONFIG_ADC
#undef CONFIG_SHA256
#undef CONFIG_WATCHDOG

/* I2C addresses */
#define I2C_ADDR_BD99992	0x60
#define I2C_ADDR_MP2949		0x40

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

/*
 * Motion sensors:
 * When reading through IO memory is set up for sensors (LPC is used),
 * the first 2 entries must be accelerometers, then gyroscope.
 * For BMI160, accel, gyro and compass sensors must be next to each other.
 *
 * TODO(crosbug.com/p/61098): Understand how the statement above applies
 * since we only have one accelerometer.
 */
enum sensor_id {
	LID_ACCEL = 0,
	LID_GYRO,
	LID_MAG,
	LID_BARO,
};

/* Sensors without hardware FIFO are in forced mode */
#define CONFIG_ACCEL_FORCE_MODE_MASK (1 << LID_BARO)

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
