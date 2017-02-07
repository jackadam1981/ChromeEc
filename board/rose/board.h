/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Sweetberry configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Specify RAM size */
#undef  CONFIG_RAM_SIZE
#define CONFIG_RAM_SIZE 0x20000

/* Use external clock */
/* #define CONFIG_STM32_CLOCK_HSE_HZ 8000000 */

#define CONFIG_BOARD_POST_GPIO_INIT

/* Flash layout 128K RO + 128K RW, no PSTATE */
#undef CONFIG_FLASH_PSTATE
#undef CONFIG_RO_SIZE
#define CONFIG_RO_SIZE     (128 * 1024)
#undef CONFIG_RW_MEM_OFF
#define CONFIG_RW_MEM_OFF  (128 * 1024)
#undef CONFIG_RW_SIZE
#define CONFIG_RW_SIZE     (128 * 1024)
#undef CONFIG_FLASH_SIZE
#define CONFIG_FLASH_SIZE  (256 * 1024)

/* Enable console recasting of GPIO type. */
#define CONFIG_CMD_GPIO_EXTENDED

/* The UART console is on USART2 (PA2/PA3) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 2
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096
/* Don't waste precious DMA channels on console. */
#undef CONFIG_UART_TX_DMA
#undef CONFIG_UART_RX_DMA
#define CONFIG_UART_TX_REQ_CH 4
#define CONFIG_UART_RX_REQ_CH 4

/* We're I2C slave only */
#define CONFIG_I2C
#define I2C_PORT_EC     STM32_I2C1_PORT

/* Slave address for host command */
#ifdef HAS_TASK_HOSTCMD
#define CONFIG_HOSTCMD_I2C_SLAVE_ADDR 0x3c
#define CONFIG_BOARD_I2C_SLAVE_ADDR 0x8c
#endif

/* SPI master */
#define CONFIG_SPI_MASTER
#define CONFIG_SPI_HALFDUPLEX
#define CONFIG_STM32_SPI1_MASTER
/* Enable SPI master xfer command */
#define CONFIG_CMD_SPI_XFER

/* This is not actually a Chromium EC so disable some features. */
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH
#undef CONFIG_WATCHDOG

/* Optional features */
#define CONFIG_STM_HWTIMER32
#define CONFIG_DMA_HELP
#define CONFIG_FPU

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 5

#include "gpio_signal.h"

void button_event(enum gpio_signal signal);
void sensor_event(enum gpio_signal signal);
void board_i2c_process(int read, uint8_t addr, int len, char *buffer,
		       void (*send_response)(int len));
#endif /* !__ASSEMBLER__ */
#endif /* __CROS_EC_BOARD_H */
