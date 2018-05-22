/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Careena board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#include "baseboard.h"

/* Power and battery LEDs */
#define CONFIG_LED_COMMON
#define CONFIG_CMD_LEDTEST

#undef CONFIG_LED_PWM_NEAR_FULL_COLOR
#undef CONFIG_LED_PWM_CHARGE_ERROR_COLOR
#undef CONFIG_LED_PWM_SOC_ON_COLOR
#undef CONFIG_LED_PWM_SOC_SUSPEND_COLOR

#define CONFIG_LED_PWM_NEAR_FULL_COLOR EC_LED_COLOR_BLUE
#define CONFIG_LED_PWM_CHARGE_ERROR_COLOR EC_LED_COLOR_AMBER
#define CONFIG_LED_PWM_SOC_ON_COLOR EC_LED_COLOR_BLUE
#define CONFIG_LED_PWM_SOC_SUSPEND_COLOR EC_LED_COLOR_BLUE

#define CONFIG_LED_PWM_COUNT 1

#define I2C_PORT_KBLIGHT NPCX_I2C_PORT5_0

/* KB backlight driver */
#define CONFIG_LED_DRIVER_LM3630A

#endif /* __CROS_EC_BOARD_H */
