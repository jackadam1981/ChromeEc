/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Glower board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* Optional features */
#define CONFIG_SYSTEM_UNLOCKED  /* Allow dangerous commands */
#define CONFIG_WATCHDOG_HELP
#define CONFIG_CLOCK_CRYSTAL
#define CONFIG_WAKE_PIN GPIO_POWER_BUTTON_L
#define CONFIG_SPI_PORT 1
#define CONFIG_SPI_CS_GPIO GPIO_PVT_CS0

/* Modules we want to exclude */
#undef CONFIG_ADC
#undef CONFIG_EEPROM
#undef CONFIG_EOPTION
#undef CONFIG_PSTORE
#undef CONFIG_LID_SWITCH
#undef CONFIG_PECI
#undef CONFIG_SWITCH

#ifndef __ASSEMBLER__

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
