/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __BOARD_MOLI_FW_CONFIG_H_
#define __BOARD_MOLI_FW_CONFIG_H_

#include <stdint.h>

/****************************************************************************
 * CBI FW_CONFIG layout for Moli board.
 *
 * Source of truth is the project/brask/moli/config.star configuration file.
 */
enum ec_cfg_power_on_monitor {
	POWER_ON_MONITOR_ENABLE = 0,
	POWER_ON_MONITOR_DISABLE = 1
};

union moli_cbi_fw_config {
	struct {
		uint32_t bj_power : 2;
		uint32_t mlb_usb_tbt : 2;
		uint32_t storage : 2;
		uint32_t audio : 1;
		enum ec_cfg_power_on_monitor po_mon : 1;
		uint32_t reserved_1 : 24;
	};
	uint32_t raw_value;
};

/**
 * Read the cached FW_CONFIG.  Guaranteed to have valid values.
 *
 * @return the FW_CONFIG for the board.
 */
union moli_cbi_fw_config get_fw_config(void);

/**
 * Get enable/disable power on by monitor from FW_CONFIG.
 */
enum ec_cfg_power_on_monitor ec_cfg_power_on_monitor(void);

#endif /* __BOARD_MOLI_FW_CONFIG_H_ */
