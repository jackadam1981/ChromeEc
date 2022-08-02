/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __BOARD_AGAH_FW_CONFIG_H_
#define __BOARD_AGAH_FW_CONFIG_H_

#include <stdint.h>

/****************************************************************************
 * CBI FW_CONFIG layout for Agah board.
 *
 * Source of truth is the project/draco/agah/config.star configuration file.
 */

enum ec_cfg_keyboard_backlight_type {
	KEYBOARD_BACKLIGHT_DISABLED = 0,
	KEYBOARD_BACKLIGHT_ENABLED = 1
};

enum ec_cfg_keyboard_layout { KB_LAYOUT_DEFAULT = 0, KB_LAYOUT_1 = 1 };

union agah_cbi_fw_config {
	struct {
		uint32_t sd_db : 2;
		enum ec_cfg_keyboard_backlight_type kb_bl : 1;
		uint32_t audio : 3;
		enum ec_cfg_keyboard_layout kb_layout : 2;
		uint32_t reserved_1 : 24;
	};
	uint32_t raw_value;
};

/**
 * Get keyboard type from FW_CONFIG.
 *
 * @return the keyboard type.
 */
enum ec_cfg_keyboard_layout get_ec_cfg_keyboard_layout(void);

/**
 * Read the cached FW_CONFIG.  Guaranteed to have valid values.
 *
 * @return the FW_CONFIG for the board.
 */
union agah_cbi_fw_config get_fw_config(void);

#endif /* __BOARD_AGAH_FW_CONFIG_H_ */
