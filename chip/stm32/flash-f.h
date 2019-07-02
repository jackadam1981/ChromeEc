/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STM32_FLASH_F_H
#define __CROS_EC_STM32_FLASH_F_H

#include <stdbool.h>

enum flash_rdp_level {
	FLASH_RDP_LEVEL_INVALID = -1,	/**< Error occurred. */
	FLASH_RDP_LEVEL_0,		/**< No read protection. */
	FLASH_RDP_LEVEL_1,              /**< Flash read from bootloader mode or
					 *   JTAG disabled. Changing to Level 0
					 *   from this level triggers mass
					 *   erase.
					 */
	FLASH_RDP_LEVEL_2,              /**< Same as Level 1, but is permanent
					 *   and can never be disabled.
					 */
};

enum flash_rdp_level flash_physical_get_rdp_level(void);
int flash_physical_set_rdp_level(enum flash_rdp_level level);

bool is_flash_rdp_enabled(void);
int enable_flash_rdp(bool enable);

#endif /* __CROS_EC_STM32_FLASH_F_H */
