/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CBI_REPLICATE_H
#define __CROS_EC_CBI_REPLICATE_H

#include "cros_board_info.h"

#if defined(CONFIG_PLATFORM_EC_CBI_FLASH_REPLICATE_EEPROM)
extern const struct cbi_storage_config_t flash_cbi_config, eeprom_cbi_config;
#endif
#endif /* __CROS_EC_CBI_REPLICATE_H */
