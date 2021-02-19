/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Brya baseboard configuration */

#ifndef __CROS_EC_BASEBOARD_H
#define __CROS_EC_BASEBOARD_H

/*
 * By default, enable all console messages excepted HC
 */
#define CC_DEFAULT     (CC_ALL & ~(BIT(CC_HOSTCMD)))

/* NPCX9 config */
#define NPCX9_PWM1_SEL    1  /* GPIO C2 is used as PWM1. */
/*
 * This defines which pads (GPIO10/11 or GPIO64/65) are connected to
 * the "UART1" (NPCX_UART_PORT0) controller when used for
 * CONSOLE_UART.
 */
#define NPCX_UART_MODULE2	1 /* 1:GPIO64/65 for UART1 */

#define CONFIG_EXTPOWER_GPIO

/* Host communication */
#define CONFIG_HOSTCMD_ESPI
#define CONFIG_HOSTCMD_ESPI_VW_SLP_S4

/* Common charger defines */
#define CONFIG_CHARGE_MANAGER
#define CONFIG_CHARGER
#define CONFIG_CHARGER_DISCHARGE_ON_AC
#define CONFIG_CHARGER_INPUT_CURRENT		512

#define CONFIG_CMD_CHARGER_DUMP

#define CONFIG_USB_CHARGER
#define CONFIG_BC12_DETECT_PI3USB9201

/*
 * Don't allow the system to boot to S0 when the battery is low and unable to
 * communicate on locked systems (which haven't PD negotiated)
 */
#define CONFIG_CHARGER_MIN_POWER_MW_FOR_POWER_ON_WITH_BATT	15000
#define CONFIG_CHARGER_MIN_BAT_PCT_FOR_POWER_ON			3
#define CONFIG_CHARGER_MIN_BAT_PCT_FOR_POWER_ON_WITH_AC		1
#define CONFIG_CHARGER_MIN_POWER_MW_FOR_POWER_ON		15001

/* Common battery defines */
#define CONFIG_BATTERY_SMART
#define CONFIG_BATTERY_FUEL_GAUGE
#define CONFIG_BATTERY_CUT_OFF
#define CONFIG_BATTERY_PRESENT_CUSTOM
#define CONFIG_BATTERY_HW_PRESENT_CUSTOM
#define CONFIG_BATTERY_REVIVE_DISCONNECT

/* Chipset config */
#define CONFIG_CHIPSET_ALDERLAKE_SLG4BD44540

/* Thermal features */
#define CONFIG_THROTTLE_AP
#define CONFIG_CHIPSET_CAN_THROTTLE

#define CONFIG_PWM

/* Enable I2C Support */
#define CONFIG_I2C
#define CONFIG_I2C_CONTROLLER

#ifndef __ASSEMBLER__

#include <stdbool.h>

#include "common.h"
#include "baseboard_usbc_config.h"
#include "extpower.h"

/**
 * Configure run-time data structures and operation based on CBI data. This
 * typically includes customization for changes in the BOARD_VERSION and
 * FW_CONFIG fields in CBI. This routine is called from the baseboard after
 * the CBI data has been initialized.
 */
__override_proto void board_cbi_init(void);

/*
 * Check battery disconnect state.
 * This function will return if battery is initialized or not.
 * @return true - initialized. false - not.
 */
__override_proto bool board_battery_is_initialized(void);

/*
 * Return the board revision number.
 */
uint8_t get_board_id(void);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BASEBOARD_H */
