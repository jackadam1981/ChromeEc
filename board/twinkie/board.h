/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Twinkie dongle configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* the UART console is on USART1 (PA9/PA10) */
#define CONFIG_UART_CONSOLE 1

/* Optional features */
#define CONFIG_STM_HWTIMER32
#define CONFIG_USB
#define CONFIG_USB_CONSOLE

#if 1 /* PD message injector mode */
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_INTERNAL_COMP
#define CONFIG_PD_USE_DAC_AS_REF
#define CONFIG_HW_CRC
#endif

#define CONFIG_ADC
#define CONFIG_BOARD_PRE_INIT
#define CONFIG_I2C
#define CONFIG_INA231
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH
#undef CONFIG_TASK_PROFILING

/* I2C ports configuration */
#define I2C_PORT_MASTER 0

/* USB configuration */
#define CONFIG_USB_PID 0x500A
/* By default, enable all console messages excepted USB */
#define CC_DEFAULT     (CC_ALL & ~CC_MASK(CC_USB))

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK_PD_RX 1
#define TIM_CLOCK32 2
#define TIM_ADC     3

/* GPIO signal list */
enum gpio_signal {
	GPIO_CC2_ALERT_L,
	GPIO_VBUS_ALERT_L,

	GPIO_CC1_EN,
	GPIO_CC1_PD,
	GPIO_CC2_EN,
	GPIO_CC2_PD,
	GPIO_DAC,
	GPIO_CC2_TX_DATA,

	GPIO_CC1_RA,
	GPIO_USB_DM,
	GPIO_USB_DP,
	GPIO_CC1_RPUSB,
	GPIO_CC1_RP1A5,
	GPIO_CC1_RP3A0,
	GPIO_CC2_RPUSB,

	GPIO_CC1_TX_EN,
	GPIO_CC2_TX_EN,
	GPIO_CC1_TX_DATA,
	GPIO_CC1_RD,
	GPIO_I2C_SCL,
	GPIO_I2C_SDA,
	GPIO_CC2_RD,
	GPIO_LED_G_L,
	GPIO_LED_R_L,
	GPIO_LED_B_L,
	GPIO_CC2_RA,
	GPIO_CC2_RP1A5,
	GPIO_CC2_RP3A0,

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

/* USB string indexes */
enum usb_strings {
	USB_STR_DESC = 0,
	USB_STR_VENDOR,
	USB_STR_PRODUCT,
	USB_STR_VERSION,
	USB_STR_SNIFFER,

	USB_STR_COUNT
};

#endif /* !__ASSEMBLER__ */

/* USB interface indexes (use define rather than enum to expand them) */
#define USB_IFACE_CONSOLE 0
#define USB_IFACE_VENDOR  1
#define USB_IFACE_COUNT   2

/* USB endpoint indexes (use define rather than enum to expand them) */
#define USB_EP_CONTROL   0
#define USB_EP_CON_TX    1
#define USB_EP_CON_RX    2
#define USB_EP_SNIFFER   3
#define USB_EP_COUNT     4

#endif /* __BOARD_H */
