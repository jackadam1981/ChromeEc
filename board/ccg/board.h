/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel ADL-P-RVP-ITE board-specific configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#undef CONFIG_ADC
#undef CONFIG_LID_SWITCH
#undef CONFIG_HIBERNATE
#undef CONFIG_SPI_FLASH
#undef CONFIG_SWITCH

/* Enable to boot from MECC */
#undef BOOT_FROM_MECC

/* Optional feature - used by ITE */
#define CONFIG_IT83XX_FLASH_CLOCK_48MHZ
#define CONFIG_IT83XX_VCC_1P8V

#ifndef __ASSEMBLER__
#include "gpio_signal.h"

enum adc_channel {
	TEST,
};
#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
