/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __BOARD_DOCHI_FW_CONFIG_H_
#define __BOARD_DOCHI_FW_CONFIG_H_

#include <stdint.h>

/****************************************************************************
 * CBI FW_CONFIG layout for Dochi board.
 *
 * Source of truth is the project/brya/dochi/config.star configuration file.
 */

enum ec_cfg_usb_db_type { DB_USB_ABSENT = 0 };

enum ec_cfg_keyboard_backlight_type {
	KEYBOARD_BACKLIGHT_DISABLED = 0,
	KEYBOARD_BACKLIGHT_ENABLED = 1
};

enum ec_cfg_stylus_type { STYLUS_ABSENT = 0, STYLUS_PRSENT = 1 };

enum ec_cfg_numeric_pad_type {
	NUMERIC_PAD_DISABLED = 0,
	NUMERIC_PAD_ENABLED = 1
};

union dochi_cbi_fw_config {
	struct {
		enum ec_cfg_usb_db_type usb_db : 4;
		enum ec_cfg_keyboard_backlight_type kb_bl : 1;
		enum ec_cfg_stylus_type stylus : 1;
		uint32_t table_mode : 1;
		enum ec_cfg_numeric_pad_type num_pad : 1;
		uint32_t reserved_1 : 24;
	};
	uint32_t raw_value;
};

/**
 * Read the cached FW_CONFIG.  Guaranteed to have valid values.
 *
 * @return the FW_CONFIG for the board.
 */
union dochi_cbi_fw_config get_fw_config(void);

/**
 * Get the USB daughter board type from FW_CONFIG.
 *
 * @return the USB daughter board type.
 */
enum ec_cfg_usb_db_type ec_cfg_usb_db_type(void);

#endif /* __BOARD_DOCHI_FW_CONFIG_H_ */
