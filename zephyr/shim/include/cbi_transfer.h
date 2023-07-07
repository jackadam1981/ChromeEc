/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CBI_TRANSFER_H
#define __CROS_EC_CBI_TRANSFER_H

#include "cros_board_info.h"

#if defined(CONFIG_CBI_FLASH)
extern const struct cbi_storage_config_t flash_cbi_config;
#endif

#if defined(CONFIG_CBI_EEPROM)
extern const struct cbi_storage_config_t eeprom_cbi_config;
#endif

#if defined(CONFIG_PLATFORM_EC_CBI_TRANSFER_EEPROM_FLASH)
/**
 * @brief Transfer CBI in EEPROM to CBI section on flash
 *
 * The function has to be called before performing any read
 * or write operation on CBI.
 */
void cros_cbi_transfer_eeprom_to_flash(void);
#endif
#endif /* __CROS_EC_CBI_TRANSFER_H */
