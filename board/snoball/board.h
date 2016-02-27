/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Snoball board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* the UART console is on USART1 (PA9/PA10) */
#define CONFIG_UART_CONSOLE 1

/*
 * The console task is too big to include in both RO and RW images. Therefore,
 * if the console task is defined, then only build an RW image. This can be
 * useful for debugging to have a full console. Otherwise, without this task,
 * a full RO and RW is built with a limited one-way output console.
 */
#ifdef HAS_TASK_CONSOLE
#undef CONFIG_FW_INCLUDE_RO
#undef CONFIG_RW_MEM_OFF
#define CONFIG_RW_MEM_OFF 0
#undef CONFIG_RO_SIZE
#define CONFIG_RO_SIZE 0
/* Fake full size if we had a RO partition */
#undef CONFIG_RW_SIZE
#define CONFIG_RW_SIZE CONFIG_FLASH_SIZE

#else
#define CONFIG_DEBUG_PRINTF
#define UARTN CONFIG_UART_CONSOLE
#define UARTN_BASE STM32_USART_BASE(CONFIG_UART_CONSOLE)
#endif

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_ALT_MODE
/* TODO: Consider disabling PD communication in RO */
#define CONFIG_USB_PD_COMM_ENABLED 1
#define CONFIG_USB_PD_CUSTOM_VDM
#define CONFIG_USB_PD_DYNAMIC_SRC_CAP
#define CONFIG_USB_PD_LOGGING
#define CONFIG_USB_PD_LOG_SIZE 1024
#define CONFIG_USB_PD_PORT_COUNT 1
/* TODO: Replace with actual TCPC driver */
#define CONFIG_USB_PD_TCPM_FUSB302

#define CONFIG_ADC
#undef  CONFIG_ADC_WATCHDOG
#define CONFIG_BOARD_PRE_INIT
#define CONFIG_HW_CRC
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#undef CONFIG_LID_SWITCH
#undef CONFIG_WATCHDOG_HELP

#define CONFIG_LTO

/* USB configuration */
#define CONFIG_USB_PID 0x5019
#define CONFIG_USB_BCD_DEV 0x0001 /* v 0.01 */

#define CONFIG_HIBERNATE

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK_MSB 3
#define TIM_CLOCK_LSB 1

#include "gpio_signal.h"

/* ADC signals */
enum adc_channel {
	ADC_VBIAS,
	ADC_VOUT_1,
	ADC_VOUT_2,
	/* Number of ADC channels */
	ADC_CH_COUNT
};

enum board_src_cap {
	SRC_CAP_5V = 0,
	SRC_CAP_12V,
	SRC_CAP_20V,
};

#define PD_DEFAULT_STATE PD_STATE_SRC_DISCONNECTED

/* delay necessary for the voltage transition on the power supply */
/* TODO: Tune these parameters appropriately for snoball */
#define PD_POWER_SUPPLY_TURN_ON_DELAY  50000 /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 50000 /* us */

void board_reset_pd_mcu(void);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
