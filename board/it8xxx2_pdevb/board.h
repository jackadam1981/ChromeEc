/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IT8xxx2 development board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* NOTE: 0->ec evb, non-zero->pd evb */
#define IT83XX_PD_EVB  1
/* Select Baseboard features */
#include "baseboard.h"

/* PD */
#define CONFIG_USB_PD_ALT_MODE
#define CONFIG_USB_PD_ALT_MODE_DFP
#define CONFIG_USB_PD_CHECK_MAX_REQUEST_ALLOWED
#define CONFIG_USB_PD_CUSTOM_PDO
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_PORT_MAX_COUNT    3
#define CONFIG_USB_PD_TCPMV1
#define CONFIG_USB_PD_TCPM_ITE_ON_CHIP
#define CONFIG_USB_PD_TRY_SRC
#define CONFIG_USB_PD_VBUS_DETECT_GPIO
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USBC_VCONN
#define CONFIG_USBC_VCONN_SWAP
#define CONFIG_USB_PD_TCPM_TCPCI
#define CONFIG_USB_PD_DECODE_SOP
#define CONFIG_VBOOT_HASH

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

#define I2C_PORT_CHARGER IT83XX_I2C_CH_C
#define I2C_PORT_BATTERY IT83XX_I2C_CH_C

#include "gpio_signal.h"

enum pwm_channel {
	PWM_CH_FAN,
	PWM_CH_WITH_DSLEEP_FLAG,
	/* Number of PWM channels */
	PWM_CH_COUNT
};

enum adc_channel {
	ADC_VBUSSA,
	ADC_VBUSSB,
	ADC_VBUSSC,
	ADC_EVB_CH_13,
	ADC_EVB_CH_14,
	ADC_EVB_CH_15,
	/* Number of ADC channels */
	ADC_CH_COUNT
};

/* Define typical operating power and max power */
#define PD_OPERATING_POWER_MW 15000
#define PD_MAX_POWER_MW       60000
#define PD_MAX_CURRENT_MA     3000
/* Try to negotiate to 20V since i2c noise problems should be fixed. */
#define PD_MAX_VOLTAGE_MV     20000
/* TODO: determine the following board specific type-C power constants */
/*
 * delay to turn on the power supply max is ~16ms.
 * delay to turn off the power supply max is about ~180ms.
 */
#define PD_POWER_SUPPLY_TURN_ON_DELAY  30000  /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 250000 /* us */

/* delay to turn on/off vconn */
#define PD_VCONN_SWAP_DELAY 5000 /* us */

void board_pd_vbus_ctrl(int port, int enabled);
#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
