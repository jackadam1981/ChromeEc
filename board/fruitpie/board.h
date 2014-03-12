/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fruitpie board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* the UART console is on USART2 (PA14/PA15) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 2

/* Optional features */
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH
#undef CONFIG_TASK_PROFILING

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK_MSB  3
#define TIM_CLOCK_LSB 15

/* GPIO signal list */
enum gpio_signal {
	/* Inputs with interrupt handlers are first for efficiency */
	GPIO_VBUS_WAKE = 0,
	GPIO_MASTER_I2C_INT_L,

	/* PD RX/TX */
	GPIO_USB_CC1_PD,
	GPIO_PD_REF1,
	GPIO_PD_REF2,
	GPIO_USB_CC2_PD,
	GPIO_PD_CLK_OUT,
	GPIO_PD_TX_EN,
//	GPIO_PD_CLK_IN,
//	GPIO_PD_TX_DATA,

	/* Power and muxes control */
	GPIO_PP5000_EN,
	GPIO_CC_HOST,
	GPIO_CHARGE_EN_L,
	GPIO_USB_C_5V_EN,
	GPIO_VCONN1_EN,
	GPIO_VCONN2_EN,
	GPIO_SS1_EN_L,
	GPIO_SS2_EN_L,
	GPIO_SS2_USB_MODE_L,
	GPIO_SS1_USB_MODE_L,
	GPIO_DP_MODE,
	GPIO_DP_POLARITY_L,

	/* Not used : no host on that bus */
	GPIO_SLAVE_I2C_INT_L,

	/* Rohm BD92104 connections */
	GPIO_ALERT_L,
	GPIO_USBPD_RST,
	GPIO_USBPD_FORCE_OTG,
	GPIO_USBPD_VIN_EN_L,

	/* Test points */
	GPIO_TP9,
	GPIO_TP11,


	/* Discovery bringup setup */
	GPIO_USER_BUTTON,
	/* Outputs */
	GPIO_LED_BLUE,
	GPIO_LED_GREEN,
	/* Unimplemented signals we emulate */
	GPIO_ENTERING_RW,
	GPIO_WP_L,
	/* Number of GPIOs; not an actual GPIO */
	GPIO_COUNT
};

/* ADC signal */
enum adc_channel {
        ADC_CH_CC1_PD = 0,
        ADC_CH_CC2_PD,
        /* Number of ADC channels */
        ADC_CH_COUNT
};

/* Muxing for the USB type C */
enum typec_mux {
	TYPEC_MUX_NONE,
	TYPEC_MUX_USB1,
	TYPEC_MUX_USB2,
	TYPEC_MUX_DP1,
	TYPEC_MUX_DP2,
};

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
