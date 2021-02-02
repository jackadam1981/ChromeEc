/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Gingerbread board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Baseboard features */
#include "baseboard.h"

/* Optional features */
#define CONFIG_SYSTEM_UNLOCKED /* Allow dangerous commands while in dev. */

#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096

/* USB Type C and USB PD defines */
#define USB_PD_PORT_HOST   0
#define USB_PD_PORT_DP   1
#define CONFIG_USB_PD_PORT_MAX_COUNT 1


/* USB Type A Features */

/* I2C port names */
#define I2C_PORT_I2C1		0
#define I2C_PORT_I2C2		1
#define I2C_PORT_I2C3	2
/* Required symbolic I2C port names */
#define I2C_PORT_MP4245 I2C_PORT_I2C2
#define I2C_PORT_EEPROM I2C_PORT_I2C3
#define MP4245_SLAVE_ADDR MP4245_I2C_ADDR_0_FLAGS

/*
 * Macros for GPIO signals used in common code that don't match the
 * schematic names. Signal names in gpio.inc match the schematic and are
 * then redefined here to so it's more clear which signal is being used for
 * which purpose.
 */
#define GPIO_ENTERING_RW	GPIO_EC_ENTERING_RW
#define GPIO_WP_L		GPIO_EC_WP_L



#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

void board_debug_gpio(int trigger, int enable, int pulse_usec);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
