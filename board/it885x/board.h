/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __BOARD_H
#define __BOARD_H

/* unused modules */
#undef CONFIG_ADC
#undef CONFIG_EC2I
#undef CONFIG_LPC
#undef CONFIG_PECI
#undef CONFIG_PWM
#undef CONFIG_SPI

/* Optional features */
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
/* USB Power Delivery configuration */
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_ALT_MODE
#define CONFIG_USB_PD_ALT_MODE_DFP
#define CONFIG_USB_PD_CUSTOM_VDM
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_PORT_COUNT 2
#define CONFIG_USB_PD_TCPM_TCPCI
#define CONFIG_USB_PD_VBUS_DETECT_TCPC
#define CONFIG_WP_ACTIVE_HIGH

#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096

/* start as a sink */
#define PD_DEFAULT_STATE PD_STATE_SNK_DISCONNECTED

/* fake board specific type-C power constants */
#define PD_POWER_SUPPLY_TURN_ON_DELAY  30000  /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 250000 /* us */

/* Define typical operating power and max power */
#define PD_OPERATING_POWER_MW 15000
#define PD_MAX_POWER_MW       60000
#define PD_MAX_CURRENT_MA     3000
#define PD_MAX_VOLTAGE_MV     20000

#undef CONFIG_LID_SWITCH

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

#include "registers.h"

/* I2C master port connected to the TCPC */
#define I2C_PORT_TCPC   IT83XX_I2C_CH_A
#define I2C_PORT_PD_MCU IT83XX_I2C_CH_A

/* TCPC I2C slave addresses */
#define TCPC1_I2C_ADDR 0x80
#define TCPC2_I2C_ADDR 0x82

#include "gpio_signal.h"

void button_event(enum gpio_signal signal);
void board_reset_pd_mcu(void);

#endif /* !__ASSEMBLER__ */
#endif /* __BOARD_H */
