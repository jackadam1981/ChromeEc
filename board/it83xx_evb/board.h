/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IT83xx development board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Select Baseboard features */
#include "baseboard.h"

/* Optional features */
<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
#define CONFIG_BATTERY_SMART
#define CONFIG_BOARD_VERSION_GPIO
#define CONFIG_FANS 1
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define CONFIG_IT83XX_ENABLE_MOUSE_DEVICE
#define CONFIG_IT83XX_SMCLK2_ON_GPC7
#define CONFIG_KEYBOARD_BOARD_CONFIG
#define CONFIG_KEYBOARD_PROTOCOL_8042
#define CONFIG_LOW_POWER_IDLE
#define CONFIG_LOW_POWER_S0
#define CONFIG_PECI
#define CONFIG_PECI_TJMAX 100
#define CONFIG_POWER_BUTTON
#define CONFIG_PWM
/* Use CS0 of SSPI */
#define CONFIG_SPI_MASTER
#define CONFIG_SPI_FLASH_PORT 0
#define CONFIG_UART_HOST
#define CONFIG_HOSTCMD_LPC

#if IT83XX_PD_EVB
/* PD */
#define CONFIG_USB_PD_ALT_MODE
#define CONFIG_USB_PD_ALT_MODE_DFP
#define CONFIG_USB_PD_CHECK_MAX_REQUEST_ALLOWED
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_PORT_MAX_COUNT    2
#define CONFIG_USB_PD_TCPM_ITE83XX
#define CONFIG_USB_PD_TRY_SRC
#define CONFIG_USB_PD_VBUS_DETECT_GPIO
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USBC_VCONN
#define CONFIG_USBC_VCONN_SWAP
#endif

/* Optional console commands */
#define CONFIG_CMD_FLASH
#define CONFIG_CMD_SCRATCHPAD
#define CONFIG_CMD_STACKOVERFLOW

/* Debug */
#undef CONFIG_CMD_FORCETIME
#undef CONFIG_HOOK_DEBUG
#undef CONFIG_KEYBOARD_DEBUG
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096
=======
#define CONFIG_DAC
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

enum pwm_channel {
	PWM_CH_FAN,
	PWM_CH_WITH_DSLEEP_FLAG,
	/* Number of PWM channels */
	PWM_CH_COUNT
};

enum adc_channel {
	ADC_VBUSSA,
	ADC_VBUSSB,
	ADC_EVB_CH_13,
	ADC_EVB_CH_14,
	ADC_EVB_CH_15,
	ADC_EVB_CH_16,
	/* Number of ADC channels */
	ADC_CH_COUNT
};

#endif /* !__ASSEMBLER__ */
#endif /* __CROS_EC_BOARD_H */
