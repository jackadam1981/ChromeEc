/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __BOARD_BRASK_FW_CONFIG_H_
#define __BOARD_BRASK_FW_CONFIG_H_

#include <stdint.h>

/****************************************************************************
 * CBI FW_CONFIG layout for Lisbon board.
 *
 * Source of truth is the project/brask/lisbon/config.star configuration file.
 */
enum ec_cfg_audio_type { DB_AUDIO_UNKNOWN = 0, DB_NAU88L25B_I2S = 1 };

enum ec_cfg_bj_power { BJ_135W = 0, BJ_230W = 1 };

union lisbon_cbi_fw_config {
	struct {
		uint32_t audio : 3;
		uint32_t bj_power : 2;
		uint32_t reserved_1 : 27;
	};
	uint32_t raw_value;
};

/**
 * Read the cached FW_CONFIG.  Guaranteed to have valid values.
 *
 * @return the FW_CONFIG for the board.
 */
union lisbon_cbi_fw_config get_fw_config(void);

/**
 * Get the barrel-jack power from FW_CONFIG.
 */
void ec_bj_power(uint32_t *voltage, uint32_t *current);

#endif /* __BOARD_BRASK_FW_CONFIG_H_ */
