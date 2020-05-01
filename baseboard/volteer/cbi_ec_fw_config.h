/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __VOLTEER_CBI_EC_FW_CONFIG_H_
#define __VOLTEER_CBI_EC_FW_CONFIG_H_

#include "stdbool.h"

/****************************************************************************
 * CBI FW_CONFIG layout shared by all Volteer boards
 *
 * Source of truth is the program/volteer/program.star configuration file.
 */

/*
 * TODO in separate CL - create separate enums for each board project
 */
enum ec_cfg_usb_db_type {
	USB_DB_NONE = 0,
	USB_DB_USB4_GEN2 = 1,
	USB_DB_USB3 = 2,
	USB_DB_USB4_GEN3 = 3,
	USB_DB_COUNT
};

/*
 * Tablet Mode (1 bit), shared by all Volteer boards
 */
enum ec_cfg_tabletmode_type {
	TABLETMODE_DISABLED = 0,
	TABLETMODE_ENABLED = 1,
};

union volteer_cbi_fw_config {
	struct {
		enum ec_cfg_usb_db_type		usb_db : 4;
		uint32_t			thermal : 4;
		uint32_t			audio : 3;
		enum ec_cfg_tabletmode_type	tabletmode : 1;
		uint32_t			lte_db : 2;
		uint32_t			reserved_1 : 2;
		uint32_t			sd_db : 4;
		uint32_t			reserved_2 : 12;
	};
	uint32_t raw_value;
};

/*
 * Each Volteer board must define the default FW_CONFIG options to use
 * if the CBI data has not been initialized.
 */
extern union volteer_cbi_fw_config fw_config_defaults;

/**
 * Initialize the FW_CONFIG from CBI data. If the CBI data is not valid, set the
 * FW_CONFIG to the board specific defaults.
 */
void init_fw_config(void);

/**
 * Read the cached FW_CONFIG.  Guaranteed to have valid values.
 *
 * @return the FW_CONFIG for the board.
 */
union volteer_cbi_fw_config get_fw_config(void);

/**
 * Check if the FW_CONFIG has enabled tablet mode operation
 *
 * @return true if board supports tablet mode, false if the board supports
 * clamshell operation only.
 */
bool ec_config_has_tabletmode(void);

#endif /* __VOLTEER_CBI_EC_FW_CONFIG_H_ */
