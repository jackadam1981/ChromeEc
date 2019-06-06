/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

#undef CONFIG_HIBERNATE
#undef CONFIG_WP_ALWAYS
#undef CONFIG_FLASH_PSTATE
#undef CONFIG_FLASH_PHYSICAL
#undef CONFIG_FLASH
#undef CONFIG_FMAP
#undef CONFIG_SPI_FLASH
#undef CONFIG_UART_CONSOLE
#undef CONFIG_USB
#undef CONFIG_UART_TX_DMA

#define CONFIG_UART_CONSOLE 3

#define CONFIG_SPI_MASTER
#define CONFIG_SPI_HALFDUPLEX
#define CONFIG_STM32_SPI1_MASTER
#define SPI_LCD_DEVICE (&spi_devices[0])

#define CONFIG_USB_PD_REV30
#define CONFIG_USB_PD_TCPC
#define CONFIG_USB_PD_PORT_COUNT 1


#undef CONFIG_FW_INCLUDE_RO
#undef CONFIG_RW_MEM_OFF
#define CONFIG_RW_MEM_OFF 0
#undef CONFIG_RO_SIZE
#define CONFIG_RO_SIZE 0
/* Fake full size if we had a RO partition */
#undef CONFIG_RW_SIZE
#define CONFIG_RW_SIZE CONFIG_FLASH_SIZE

#undef CONFIG_RO_MEM_OFF
#define CONFIG_RO_MEM_OFF 0
#undef CONFIG_ADC

#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define I2C_PORT_MASTER 0
#define CONFIG_INA231

#define CONFIG_CMD_CLOCKGATES
#define CONFIG_STM_HWTIMER32

/* This is not actually an EC so disable some features. */
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH
#undef CONFIG_WATCHDOG

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2


enum adc_channel {
	/* EC internal die temperature in degrees K. */
	ADC_CH_EC_TEMP = 0,

	ADC_CH_COUNT
};

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */
#endif /* __CROS_EC_BOARD_H */
