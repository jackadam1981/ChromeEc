/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Servo micro configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* Enable USART1,3,4 and USB streams */
#define CONFIG_STREAM_USART

#define CONFIG_STREAM_USART3
#define CONFIG_STREAM_USART4
#define CONFIG_STREAM_USB
#define CONFIG_CMD_USART_INFO

/* Optional features */
#define CONFIG_STM_HWTIMER32
#define CONFIG_HW_CRC

/* USB Configuration */
#define CONFIG_USB
#define CONFIG_USB_PID 0x501b
#define CONFIG_USB_CONSOLE

/* USB interface indexes (use define rather than enum to expand them) */
#define USB_IFACE_CONSOLE 0
#define USB_IFACE_GPIO    1
#define USB_IFACE_I2C     2
#define USB_IFACE_USART3_STREAM  3
#define USB_IFACE_USART4_STREAM  4
#define USB_IFACE_COUNT   5

/* USB endpoint indexes (use define rather than enum to expand them) */
#define USB_EP_CONTROL 0
#define USB_EP_CONSOLE 1
#define USB_EP_GPIO    2
#define USB_EP_I2C     3
#define USB_EP_USART3_STREAM  4
#define USB_EP_USART4_STREAM  5
#define USB_EP_COUNT   6

/* Enable control of GPIOs over USB */
#define CONFIG_USB_GPIO
/* Enable console recasting of GPIO type. */
#define CONFIG_CONSOLE_DODGY_GPIO_COMMANDS

/* This is not actually an EC so disable some features. */
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH

/* Enable control of I2C over USB */
#define CONFIG_USB_I2C
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define I2C_PORT_MASTER 1

/* Give me some space!! */
#undef CONFIG_FW_INCLUDE_RO
#undef CONFIG_RW_MEM_OFF
#define CONFIG_RW_MEM_OFF 0
#undef CONFIG_RO_SIZE
#define CONFIG_RO_SIZE 0
/* Fake full size if we had a RO partition */
#undef CONFIG_RW_SIZE
#define CONFIG_RW_SIZE CONFIG_FLASH_SIZE


/* PD features */
#define CONFIG_ADC

/*#define CONFIG_USB_CHARGER*/
/*#define CONFIG_CHARGE_MANAGER*/
/*#define CONFIG_CHARGE_RAMP*/
#define CONFIG_USB_POWER_DELIVERY
/*#define CONFIG_USB_PD_ALT_MODE*/
/*#define CONFIG_USB_PD_ALT_MODE_DFP*/
#define CONFIG_USB_PD_CHECK_MAX_REQUEST_ALLOWED
/*#define CONFIG_USB_PD_CUSTOM_VDM*/
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_TRY_SRC
#define CONFIG_USB_PD_INTERNAL_COMP
#define CONFIG_USB_PD_RX_COMP_IRQ
/*#define CONFIG_USB_PD_LOGGING*/
/*#define CONFIG_USB_PD_LOG_SIZE 512*/
#define CONFIG_USB_PD_PORT_COUNT 2
#define CONFIG_USB_PD_TCPC
#define CONFIG_USB_PD_TCPM_STUB
/*#define CONFIG_USBC_SS_MUX_DFP_ONLY*/
/*#define CONFIG_USBC_SS_MUX*/
/*#define CONFIG_USBC_VCONN*/
/*#define CONFIG_USBC_VCONN_SWAP*/

/* Standard-current Rp */
#define PD_SRC_VNC           PD_SRC_DEF_VNC_MV
#define PD_SRC_RD_THRESHOLD  PD_SRC_DEF_RD_THRESH_MV

/* start as a sink in case we have no other power supply/battery */
#define PD_DEFAULT_STATE PD_STATE_SNK_DISCONNECTED

/*
 * delay to turn on the power supply max is ~16ms.
 * delay to turn off the power supply max is about ~180ms.
 */
#define PD_POWER_SUPPLY_TURN_ON_DELAY  30000  /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 250000 /* us */

/* delay to turn on/off vconn */
#define PD_VCONN_SWAP_DELAY 5000 /* us */

/* Define typical operating power and max power */
#define PD_OPERATING_POWER_MW 15000
#define PD_MAX_POWER_MW       60000
#define PD_MAX_CURRENT_MA     3000
#define PD_MAX_VOLTAGE_MV     20000

/* Charge current limit min / max, based on PWM duty cycle */
#define PWM_0_MA	500
#define PWM_100_MA	4000

/* Map current in milli-amps to PWM duty cycle percentage */
#define MA_TO_PWM(curr) (((curr) - PWM_0_MA) * 100 / (PWM_100_MA - PWM_0_MA))



/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_ADC     3


#include "gpio_signal.h"

/* USB string indexes */
enum usb_strings {
	USB_STR_DESC = 0,
	USB_STR_VENDOR,
	USB_STR_PRODUCT,
	USB_STR_SERIALNO,
	USB_STR_VERSION,
	USB_STR_CONSOLE_NAME,
	USB_STR_USART3_STREAM_NAME,
	USB_STR_USART4_STREAM_NAME,

	USB_STR_COUNT
};


/* ADC signal */
enum adc_channel {
	ADC_DUT_CC1_PD = 0,
	ADC_CHG_CC1_PD,
	ADC_CHG_CC2_PD,
	ADC_DUT_CC2_PD,
	/* Number of ADC channels */
	ADC_CH_COUNT
};

/* We got no battery!! */
int board_get_battery_soc(void);

#endif /* !__ASSEMBLER__ */
#endif /* __CROS_EC_BOARD_H */
