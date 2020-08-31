/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Lazor board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#include "baseboard.h"

/* TODO(waihong): Remove the following bringup features */
#define CONFIG_BRINGUP
#define CONFIG_SYSTEM_UNLOCKED /* Allow dangerous commands. */
#define CONFIG_USB_PD_DEBUG_LEVEL 3
#define CONFIG_CMD_AP_RESET_LOG
#define CONFIG_CMD_GPIO_EXTENDED
#define CONFIG_CMD_POWERINDEBUG
#define CONFIG_I2C_DEBUG

/* Internal SPI flash on NPCX7 */
#define CONFIG_FLASH_SIZE (512 * 1024)  /* 512KB internal spi flash */

/* Battery */
#define CONFIG_BATTERY_DEVICE_CHEMISTRY  "LION"
#define CONFIG_BATTERY_REVIVE_DISCONNECT
#define CONFIG_BATTERY_FUEL_GAUGE

/* BC 1.2 Charger */
#define CONFIG_BC12_DETECT_PI3USB9201

/* USB */
#define CONFIG_USB_PD_TCPM_PS8751
#define CONFIG_USBC_PPC_SN5S330

/* USB-A */
#define USB_PORT_COUNT 1
#define CONFIG_USB_PORT_POWER_DUMB

#define CONFIG_TABLET_MODE
#define CONFIG_TABLET_MODE_SWITCH
#define CONFIG_GMR_TABLET_MODE
#define GMR_TABLET_MODE_GPIO_L GPIO_TABLET_MODE_L

/* GPIO alias */
#define GPIO_PMIC_RESIN_L GPIO_PM845_RESIN_L

/* Remove not support config from baseboard.h */
#undef CONFIG_PWM_KBLIGHT
#undef CONFIG_ACCEL_FIFO
#undef CONFIG_CMD_ACCELS
#undef CONFIG_VOLUME_BUTTONS

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

enum adc_channel {
	ADC_VBUS,
	ADC_AMON_BMON,
	ADC_PSYS,
	ADC_CH_COUNT
};

enum pwm_channel {
	PWM_CH_DISPLIGHT = 0,
	PWM_CH_COUNT
};

/* List of possible batteries */
enum battery_type {
	BATTERY_AP16L5J,
	BATTERY_AP16L5J_009,
	BATTERY_AP16L8J,
	BATTERY_TYPE_COUNT,
};

/* Custom function to indicate if sourcing VBUS */
int board_is_sourcing_vbus(int port);
/* Enable VBUS sink for a given port */
int board_vbus_sink_enable(int port, int enable);
/* Reset all TCPCs. */
void board_reset_pd_mcu(void);
void board_set_tcpc_power_mode(int port, int mode);

#endif /* !defined(__ASSEMBLER__) */

#endif /* __CROS_EC_BOARD_H */
