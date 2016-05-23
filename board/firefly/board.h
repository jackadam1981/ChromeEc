/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Firefly board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* the UART console is on USART1 (PA9/PA10) */
#define CONFIG_UART_CONSOLE 1

/* Optional features */
#define CONFIG_STM_HWTIMER32
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_ADC
#define CONFIG_HW_CRC
#define CONFIG_I2C
#define CONFIG_BOARD_PRE_INIT
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH
#undef CONFIG_TASK_PROFILING

/* I2C ports configuration */
#define I2C_PORT_SLAVE  0

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

/* USB Configuration */
#define CONFIG_USB
#define CONFIG_USB_CONSOLE
#define CONFIG_USB_PID 0x500f
/* Prevent the USB driver from initializing at boot */
/* #define CONFIG_USB_INHIBIT_INIT */
/* USB interface indexes */
#define USB_IFACE_CONSOLE 0
#define USB_IFACE_COUNT   1
/* USB endpoint indexes */
#define USB_EP_CONTROL 0
#define USB_EP_CONSOLE 1
#define USB_EP_COUNT   2

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_ADC     3

#include "gpio_signal.h"

/* ADC signal */
enum adc_channel {
	ADC_CH_CC1_PD = 0,
	ADC_CH_CC2_PD,
	ADC_CH_VBUS_SENSE,
	/* Number of ADC channels */
	ADC_CH_COUNT
};

/* USB string indexes */
enum usb_strings {
	USB_STR_DESC = 0,
	USB_STR_VENDOR,
	USB_STR_PRODUCT,
	USB_STR_VERSION,
	USB_STR_CONSOLE_NAME,

	USB_STR_COUNT
};

extern int8_t current_cap;
void board_process_source_cap(int port, int cnt, uint32_t *src_cap);

#endif /* !__ASSEMBLER__ */

#define CONFIG_USB_PD_PORT_COUNT 1
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_INTERNAL_COMP
#define CONFIG_USB_PD_NO_VBUS_DETECT
#define CONFIG_USB_PD_TCPC
#define CONFIG_USB_PD_TCPM_STUB

#define PD_DEFAULT_STATE PD_STATE_SNK_DISCONNECTED

/* TODO: determine the following board specific type-C power constants */
/*
 * delay to turn on the power supply max is ~16ms.
 * delay to turn off the power supply max is about ~180ms.
 */
#define PD_POWER_SUPPLY_TURN_ON_DELAY  30000  /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 250000 /* us */

/* Define typical operating power and max power */
#define PD_OPERATING_POWER_MW 1000
#define PD_MAX_POWER_MW       1500
#define PD_MAX_CURRENT_MA     300
#define PD_MAX_VOLTAGE_MV     20000

#endif /* __BOARD_H */
