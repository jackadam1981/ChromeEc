/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __BOARD_BRASK_FW_CONFIG_H_
#define __BOARD_BRASK_FW_CONFIG_H_

#include <stdint.h>

/****************************************************************************
 * CBI FW_CONFIG layout for Brask board.
 *
 * Source of truth is the project/brask/brask/config.star configuration file.
 */
enum ec_cfg_audio_type { DB_AUDIO_UNKNOWN = 0, DB_NAU88L25B_I2S = 1 };

union brask_cbi_fw_config {
	struct {
		uint32_t audio : 3;
		uint32_t reserved_1 : 29;
	};
	uint32_t raw_value;
};

/**
 * Read the cached FW_CONFIG.  Guaranteed to have valid values.
 *
 * @return the FW_CONFIG for the board.
 */
union brask_cbi_fw_config get_fw_config(void);

#endif /* __BOARD_BRASK_FW_CONFIG_H_ */
