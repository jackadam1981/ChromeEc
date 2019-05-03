/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel ICLY-RVP, ITE EC board-specific configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Chipset Icelake */
#define CONFIG_CHIPSET_ICELAKE

/* Fan features */
#define CONFIG_FANS 1

/* Temperature sensor */
#define CONFIG_TEMP_SENSOR

#include "baseboard.h"

/* Charger */
#define CONFIG_CHARGER_ISL9238

/* DC Jack charge ports */
#undef  CONFIG_DEDICATED_CHARGE_PORT_COUNT
#define CONFIG_DEDICATED_CHARGE_PORT_COUNT 1

/* USB ports */
#define CONFIG_USB_PD_PORT_COUNT 2
#define DEDICATED_CHARGE_PORT 2

/* USB MUX */
#define CONFIG_USB_MUX_VIRTUAL

/* BC1.2 chip */
#define CONFIG_BC12_DETECT_MAX14637

/* Config BB retimer */
#define CONFIG_USB_PD_RETIMER_INTEL_BB

/* I2C ports */
#define CONFIG_IT83XX_SMCLK2_ON_GPC7

#define I2C_PORT_CHARGER	IT83XX_I2C_CH_B
#define I2C_PORT_BATTERY	IT83XX_I2C_CH_B
#define I2C_PORT_PCA9555_BOARD_ID_GPIO	IT83XX_I2C_CH_B
#define I2C_PORT_PORT80		IT83XX_I2C_CH_B
#define I2C_PORT0_BB_RETIMER	IT83XX_I2C_CH_E
#define I2C_PORT1_BB_RETIMER	IT83XX_I2C_CH_E

#define I2C_ADDR_PCA9555_BOARD_ID_GPIO	0x44
#define PORT80_I2C_ADDR			MAX695X_I2C_ADDR1
#define I2C_PORT0_BB_RETIMER_ADDR	0x84
#define I2C_PORT1_BB_RETIMER_ADDR	0x86

#ifndef __ASSEMBLER__

enum iclrvp_charge_ports {
	TYPE_C_PORT_0,
	TYPE_C_PORT_1,
	DC_JACK_PORT_0 = DEDICATED_CHARGE_PORT,
};

/* Define max power */
#define PD_MAX_POWER_MW        60000

int board_get_version(void);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
