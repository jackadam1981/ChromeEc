/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Quiche board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Baseboard features */
#include "baseboard.h"

/* Optional features */
#define CONFIG_SYSTEM_UNLOCKED /* Allow dangerous commands while in dev. */


/* Keyboard features */

/* Sensors */

/* USB Type C and USB PD defines */
#define USB_PD_PORT_HOST   0
#define USB_PD_PORT_DP   1
#define CONFIG_USB_PD_PORT_MAX_COUNT 2
#define CONFIG_USB_MUX_PS8822
/* #define BOARD_ALLOW_HIGH_VBUS */
#undef TCPM_V2_ALT_MODE
/* #undef CONFIG_USB_PD_INITIAL_DRP_STATE */
/* #define CONFIG_USB_PD_INITIAL_DRP_STATE PD_DRP_FORCE_SOURCE */

/* USB Type A Features */

/* BC 1.2 */

/* Volume Button feature */

/* Fan features */

/*
 * Macros for GPIO signals used in common code that don't match the
 * schematic names. Signal names in gpio.inc match the schematic and are
 * then redefined here to so it's more clear which signal is being used for
 * which purpose.
 */
#define GPIO_ENTERING_RW	GPIO_EC_ENTERING_RW
#define GPIO_WP_L		GPIO_EC_WP_L

#define BOARD_NUM_POWER_GPIOS 22
#define CONFIG_UART_CONSOLE 3
#define CONFIG_UART_TX_DMA_CH STM32_DMAC_USART3_TX
#define CONFIG_UART_TX_DMA_PH DMAMUX_REQ_USART3_TX

/* I2C port names */
#define I2C_PORT_I2C1		0
#define I2C_PORT_I2C2		1
#define I2C_PORT_I2C3	2
/* Required symbolic I2C port names */
#define I2C_PORT_MP4245 I2C_PORT_I2C2
#define I2C_PORT_EEPROM I2C_PORT_I2C3
#define MP4245_SLAVE_ADDR MP4245_I2C_ADDR_0_FLAGS


#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"
#define GPIO_TRIGGER_1 GPIO_USB3_A3_CDP_EN
#define GPIO_TRIGGER_2 GPIO_USB3_A4_CDP_EN
#define GPIO_DP_HPD GPIO_DDI_MST_IN_HPD

enum  debug_gpio {
	TRIGGER_1 = 0,
	TRIGGER_2,
};

void board_debug_gpio(int trigger, int enable);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
