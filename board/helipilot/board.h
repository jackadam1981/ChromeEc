/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Helipilot board configuration */

#include "base_board.h"

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/*
 *-------------------------------------------------------------------------*
 * Fingerprint Specific
 *-------------------------------------------------------------------------*
 */

#ifdef SECTION_IS_RW
#define CONFIG_FP_SENSOR_FPC1025
#endif /* SECTION_IS_RW */

/*
 * This restores the incorrect alignment used to release helipilot MP firmware.
 *
 * TODO(b/357938320): Fix this incorrect alignment in a way that is compatible
 * with existing MP firmware.
 */
#undef CONFIG_RW_SIZE
#define CONFIG_RW_SIZE                                          \
	((NPCX_PROGRAM_MEMORY_SIZE / CONFIG_FLASH_ERASE_SIZE) * \
	 CONFIG_FLASH_ERASE_SIZE)

#endif /* __CROS_EC_BOARD_H */
