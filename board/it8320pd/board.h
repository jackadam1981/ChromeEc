/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IT8320 development board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Optional features */
#undef CONFIG_LID_SWITCH
#define CONFIG_LOW_POWER_IDLE
#define CONFIG_SHA256
/* PD */
#define CONFIG_USB_BCD_DEV             0x0100
#define CONFIG_USB_PD_ALT_MODE
#define CONFIG_USB_PD_ALT_MODE_DFP
#define CONFIG_USB_PD_CHECK_MAX_REQUEST_ALLOWED
#define CONFIG_USB_PD_CUSTOM_VDM
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_PORT_COUNT       2
#define CONFIG_USB_PD_TCPM_ITE83XX
#define CONFIG_USB_PD_TRY_SRC
#define CONFIG_USB_PID                 0x8320
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USBC_VCONN
#define CONFIG_USBC_VCONN_SWAP
#define CONFIG_WATCHDOG

/* Optional console commands */
#define CONFIG_CMD_FLASH
#define CONFIG_CMD_SCRATCHPAD
#define CONFIG_CMD_STACKOVERFLOW

/* Debug */
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096

/* Unused modules */
#undef CONFIG_EC2I
#undef CONFIG_LPC
#undef CONFIG_PECI
#undef CONFIG_PWM
#undef CONFIG_SPI

#ifndef __ASSEMBLER__

#include "gpio_signal.h"

enum adc_channel {
	ADC_VBUSSA,
	ADC_VBUSSB,
	/* Number of ADC channels */
	ADC_CH_COUNT
};

/* Define typical operating power and max power */
#define PD_OPERATING_POWER_MW 15000
#define PD_MAX_POWER_MW       60000
#define PD_MAX_CURRENT_MA     3000
/* Try to negotiate to 20V since i2c noise problems should be fixed. */
#define PD_MAX_VOLTAGE_MV     20000
/* start as a sink in case we have no other power supply/battery */
#define PD_DEFAULT_STATE PD_STATE_SNK_DISCONNECTED
/* TODO: determine the following board specific type-C power constants */
/*
 * delay to turn on the power supply max is ~16ms.
 * delay to turn off the power supply max is about ~180ms.
 */
#define PD_POWER_SUPPLY_TURN_ON_DELAY  30000  /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 250000 /* us */

/* delay to turn on/off vconn */
#define PD_VCONN_SWAP_DELAY 5000 /* us */

/* Reset PD MCU */
void board_reset_pd_mcu(void);
int board_get_battery_soc(void);
void board_pd_vconn_ctrl(int port, int cc_pin, int enabled);
void board_pd_vbus_ctrl(int port, int enabled);
void board_pd_init(int port, int role);

#endif /* !__ASSEMBLER__ */
#endif /* __CROS_EC_BOARD_H */
