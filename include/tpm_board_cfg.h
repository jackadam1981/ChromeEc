/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_TPM_BOARD_CFG_H
#define __CROS_EC_TPM_BOARD_CFG_H

#include <stdint.h>

#include "common.h"

/* Bit masks for each bit in TPM_BOARD_CFG register */
#define BITMASK_PROGRAMMED_LOCKED	BIT(31)
#define BITMASK_LONG_INT_AP_PULSE	BIT(0)

/*
 * Write on TPM_BOARD_CFG register if BITMASK_PROGRAMMED_LOCKED is clear.
 *
 * @param value: value to write on TPM_BOARD_CFG
 */
void board_cfg_reg_write(uint32_t value);

/*
 * Read TPM_BOARD_CFG register.
 *
 * @param TPM_BOARD_CFG register value in uint32_t type.
 */
uint32_t board_cfg_reg_read(void);

#endif	/* __CROS_EC_TPM_BOARD_CFG_H */
