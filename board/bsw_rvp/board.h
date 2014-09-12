/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* BSW RVP board configuration */

#ifndef __BOARD_H
#define __BOARD_H

#define STRAGO_PO

/* Optional features */
#define CONFIG_SYSTEM_UNLOCKED  /* Allow dangerous commands */
#define CONFIG_WATCHDOG_HELP

#define CONFIG_KEYBOARD_PROTOCOL_8042
#define CONFIG_KEYBOARD_KSO_BASE 0 /* KSO starts from KSO00 */
#define CONFIG_KEYBOARD_IRQ_GPIO GPIO_KBD_IRQ_L
#define CONFIG_POWER_BUTTON
#define CONFIG_POWER_BUTTON_X86
#define CONFIG_LID_SWITCH

#define CONFIG_POWER_COMMON
#define CONFIG_EXTPOWER_GPIO
#define CONFIG_I2C

#define CONFIG_VBOOT_HASH

#define CONFIG_CHARGER
#define CONFIG_CHARGER_V1
#define CONFIG_CHARGER_BQ24715
#define CONFIG_CHARGER_DISCHARGE_ON_AC
#define CONFIG_CHARGER_INPUT_CURRENT 1700   /* 33 W adapter, 19 V, 1.75 A */
#define CONFIG_CHARGER_SENSE_RESISTOR 10    /* Charge sense resistor, mOhm */
#define CONFIG_CHARGER_SENSE_RESISTOR_AC 10 /* Input senso resistor, mOhm */
#define CONFIG_BATTERY_SMART

/* I2C ports */
#define I2C_PORT_BATTERY 3
#define I2C_PORT_CHARGER 3

/* Modules we want to exclude */
#undef CONFIG_EEPROM
#undef CONFIG_EOPTION
#undef CONFIG_PSTORE
#undef CONFIG_PECI
#undef CONFIG_SWITCH

#undef CONFIG_PWM
#undef CONFIG_FANS
#undef CONFIG_ADC
#undef CONFIG_WAKE_PIN
#undef CONFIG_SPI
#undef CONFIG_SPI_PORT
#undef CONFIG_SPI_CS_GPIO

#ifndef __ASSEMBLER__

#include "gpio_signal.h"

/* power signal definitions */
enum power_signal {
	X86_ALL_SYS_PWRGD = 0,
	X86_RSMRST_L_PWRGD,
	X86_SLP_S3_DEASSERTED,
	X86_SLP_S4_DEASSERTED,

	/* Number of X86 signals */
	POWER_SIGNAL_COUNT
};

/* Discharge battery when on AC power for factory test. */
int board_discharge_on_ac(int enable);

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
