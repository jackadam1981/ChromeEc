/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Cros Board Info EEPROM
 */
#ifndef __CROS_EC_CBI_EEPROM_H
#define __CROS_EC_CBI_EEPROM_H

#include "common.h"

/**
 * Set and update FW_CONFIG tag field
 *
 * This function is only included when HAS_TASK_CHIPSET is not defined. It is
 * intended to be used for projects which want CBI functions, but do not have an
 * AP and ectool host command access.
 *
 * @param fw_config	updated value for FW_CONFIG tag
 * @return EC_SUCCESS to indicate the field was written correctly.
 *         EC_ERROR_ACCESS_DENIED to indicate WP is active
 *         EC_ERROR_UNKNOWN to indicate that the write operation failed
 */
int cbi_set_fw_config(uint32_t fw_config);

/**
 * Read CBI from EEPROM to the local cache if CBI cache is not valid.
 *
 * @return EC_SUCCESS on success or EC_ERROR_* otherwise.
 */
int cbi_read(void);

#ifdef TEST_BUILD
/**
 * Write the locally cached CBI to EEPROM.
 *
 * @return EC_RES_SUCCESS on success or EC_RES_* otherwise.
 */
int cbi_write(void);
#endif

#endif /* __CROS_EC_CBI_EEPROM_H */
