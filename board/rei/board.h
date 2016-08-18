/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Rei board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Undefining defaults for bringup. */
#define CONFIG_BRINGUP
#undef CONFIG_FLASH
#undef CONFIG_FMAP
#undef CONFIG_FLASH_PSTATE
#undef CONFIG_FLASH_PSTATE_BANK
#undef CONFIG_HIBERNATE
#define CONFIG_POWER_BUTTON_IGNORE_LID
#undef CONFIG_WATCHDOG
#undef CONFIG_WATCHDOG_PERIOD_MS
#define CONFIG_WATCHDOG_PERIOD_MS 120000

/* #define CONFIG_BATTERY_PRESENT_GPIO GPIO_BAT_PRESENT_L */
/* #define CONFIG_BATTERY_SMART */
#define CONFIG_CHARGE_MANAGER
#define CONFIG_CHARGER_INPUT_CURRENT 512
#define CONFIG_CHARGER_V2

#define CONFIG_CHIPSET_ROTOR

#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define I2C_PORT_PMIC		1
#define I2C_PORT_BATTERY	2
#define I2C_PORT_CHARGER	2
#define I2C_PORT_TCPC		3

#define CONFIG_MKBP_EVENT
#define CONFIG_MKBP_EVENT_NO_GPIO

#define CONFIG_POWER_COMMON
#define CONFIG_POWER_BUTTON
#define CONFIG_POWER_BUTTON_ACTIVE_STATE 1

#undef CONFIG_TCPC_I2C_BASE_ADDR
#define CONFIG_TCPC_I2C_BASE_ADDR 0x54 /* 8b addr */
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_PORT_COUNT 1
#define CONFIG_USB_PD_TCPM_MUX
#define CONFIG_USB_PD_TCPM_TCPCI
#define CONFIG_USB_PD_VBUS_DETECT_TCPC
#define CONFIG_USB_POWER_DELIVERY
/* TODO(aaboagye): Verify that this is needed. */
#define CONFIG_USBC_SS_MUX

#define PD_DEFAULT_STATE PD_STATE_SNK_DISCONNECTED

/* TODO(aaboagye): Verify these values. */
#define PD_POWER_SUPPLY_TURN_ON_DELAY	30000 /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY	250000 /* us */
#define PD_VCONN_SWAP_DELAY		5000 /* us */

#define PD_OPERATING_POWER_MW	15000
#define PD_MAX_POWER_MW		60000
#define PD_MAX_CURRENT_MA	3000
#define PD_MAX_VOLTAGE_MV	20000

/* #define CONFIG_SPI_MASTER */

/* Not sure how this number is determined. */
#undef DEFERRABLE_MAX_COUNT
#define DEFERRABLE_MAX_COUNT 16

#define CPU_CLOCK 1000000
/* #define CONFIG_WATCHDOG */

/* The baud rate is 9600 */
#undef CONFIG_UART_BAUD_RATE
#define CONFIG_UART_BAUD_RATE 9600

#define CONFIG_CMD_FORCETIME


#ifndef __ASSEMBLER__
#include "gpio_signal.h"

enum adc_channel {
	ADC_VBUS = -1,
	ADC_CH_COUNT
};

/* TODO(aaboagye): Need to find these power signals at some point. */
enum power_signal {
	POWER_SIGNAL_COUNT,
};

#endif /* defined(__ASSEMBLER__) */
#endif /* __CROS_EC_BOARD_H */
