/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Guybrush board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Baseboard features */
#include "baseboard.h"

/* Optional features */
#define CONFIG_SYSTEM_UNLOCKED /* Allow dangerous commands while in dev. */

/* Config options automatically enabled, re-enable once support added */
#undef CONFIG_ADC
#undef CONFIG_SWITCH
#undef CONFIG_SPI_FLASH
#undef CONFIG_SUPPORT_CHIP_HIBERNATION

#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096

/*
 * Macros for GPIO signals used in common code that don't match the
 * schematic names. Signal names in gpio.inc match the schematic and are
 * then redefined here to so it's more clear which signal is being used for
 * which purpose.
 */
#define GPIO_WP_L			GPIO_EC_WP_L
#define GPIO_POWER_BUTTON_L		GPIO_MECH_PWR_BTN_ODL
#define GPIO_AC_PRESENT			GPIO_ACOK_OD
#define GPIO_SYS_RESET_L		GPIO_EC_SYS_RST_L
#define GPIO_PCH_PWRBTN_L		GPIO_EC_SOC_PWR_BTN_L
#define GPIO_VOLUME_UP_L		GPIO_VOLUP_BTN_ODL
#define GPIO_VOLUME_DOWN_L		GPIO_VOLDN_BTN_ODL

/* Motion sensing drivers */

/* Keyboard features */

/* Sensors */

/* USB Type C and USB PD defines */

/* USB Type A Features */

/* BC 1.2 */

/* Volume Button feature */

/* Fan features */

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
