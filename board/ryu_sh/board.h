/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ryu sensor board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* the UART console is on USART2 (PA2/PA3) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 2

/* By default, enable all console messages excepted USB */
#define CC_DEFAULT     (CC_ALL & ~CC_MASK(CC_USBPD))

/* Optional features */
#define CONFIG_STM_HWTIMER32
#undef CONFIG_USB_POWER_DELIVERY
#undef CONFIG_USB_PD_DUAL_ROLE
#undef CONFIG_USB_PD_INTERNAL_COMP
#undef CONFIG_ADC
#define CONFIG_HW_CRC
#define CONFIG_I2C
#undef  CONFIG_LID_SWITCH
#define CONFIG_VBOOT_HASH
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_TASK_PROFILING
#undef CONFIG_WP_PRESENT

/* I2C ports configuration */
#define I2C_PORT_MASTER 1
#define I2C_PORT_SLAVE  0
#define I2C_PORT_EC I2C_PORT_SLAVE
#define I2C_PORT_ACCEL I2C_PORT_MASTER
#define I2C_PORT_COMPASS I2C_PORT_MASTER

/* slave address for host commands */
#ifdef HAS_TASK_HOSTCMD
#define CONFIG_HOSTCMD_I2C_SLAVE_ADDR 0x3a
#endif

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_ADC     3

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
