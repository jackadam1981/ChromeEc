/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ec_template board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Baseboard features */
#include "baseboard.h"

/* Optional features */
#define CONFIG_SYSTEM_UNLOCKED /* Allow dangerous commands while in dev. */

/*
 * Config options automatically enabled by EC chipset, re-enable once board
 * specific support added
 */
#undef CONFIG_ADC
#undef CONFIG_SWITCH

#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096

/* EC code features */

/* AP to EC communication */

/* AP Chipset configuration */

/* I2C Buses */

/* CrOS Board Information */

/* Keyboard Configuration */

/* LEDs */

/* Sensors */

/* Power Sequencing */

/* Battery Charger */

/* Battery */

/* USB Type C */

/* USB PD */

/* USB PPC */

/* USB TCPC */

/*
 * Macros for GPIO signals used in common code that don't match the
 * schematic net names.
 *
 * Format:
 *    #define GPIO_<EC_codebase_name>    GPIO_<schematic_net_name>
 */
#define GPIO_ENTERING_RW	GPIO_EC_ENTERING_RW
#define GPIO_LID_OPEN		GPIO_EC_LID_OPEN
#define GPIO_WP_L		GPIO_EC_WP_L



#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"


#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
